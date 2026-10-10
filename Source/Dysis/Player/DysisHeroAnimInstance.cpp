#include "DysisHeroAnimInstance.h"
#include "DysisCharacter.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	const TCHAR* const HeroClipNames[int32(EDysisHeroClip::Count)] = {
		TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("RunUpstairs"), TEXT("JumpStart"), TEXT("FallLoop"), TEXT("Land"), TEXT("TouchForward") };
	constexpr int32 HeroClip_Idle = int32(EDysisHeroClip::Idle), HeroClip_Walk = int32(EDysisHeroClip::Walk), HeroClip_Run = int32(EDysisHeroClip::Run),
		HeroClip_Upstairs = int32(EDysisHeroClip::RunUpstairs), HeroClip_Land = int32(EDysisHeroClip::Land), HeroClip_Touch = int32(EDysisHeroClip::TouchForward);
	// 哪几段是循环放的；其余放到头就停在最后一帧
	bool HeroClipLoops(int32 i) { return i == HeroClip_Idle || i == HeroClip_Walk || i == HeroClip_Run || i == HeroClip_Upstairs || i == int32(EDysisHeroClip::FallLoop); }
	// 哪几段是原地站着的（脚该钉在地上）、哪几段是脚踩着地的
	bool HeroClipStands(int32 i) { return i == HeroClip_Idle || i == HeroClip_Land || i == HeroClip_Touch; }
	bool HeroClipOnGround(int32 i) { return HeroClipStands(i) || i == HeroClip_Walk || i == HeroClip_Run || i == HeroClip_Upstairs; }
	constexpr float HeroBlendSeconds = 0.16f;
	constexpr int32 HeroGroundHz = 30;

	/** 一段动作的“落地表”，开场时从动作里量出来，每 1/30 秒一格（模型自己的坐标：脚踩的地面是 z = 0，脸朝 +Y，厘米）。
	 *  美术给的动作里骨盆是钉死不动的——原本“人在动、脚踩着地”，变成了“人不动、脚在地上滑”。这张表记的是骨盆该挪回去多少：
	 *    · 站着的那几段：两只脚里低的那只算踩着地的，它往哪边滑，骨盆就往反方向挪同样多（它就不动了）；
	 *      两只脚都离地时（落地那一下的下蹲）骨盆往下放到脚着地；
	 *    · 走、跑、上台阶：整段里脚最低时离地多高，就把人放低多少（“跑”那一段悬着九厘米多）。 */
	struct FDysisHeroGround
	{
		float Length = 0.0f;
		TArray<FVector> Sway;        // 骨盆挪多少
		TArray<FVector2D> Mid;       // 两只脚踝的中点（没挪之前）
		bool IsValid() const { return Sway.Num() > 1 && Length > 0.0f; }
		template <typename T> T At(const TArray<T>& A, float Time) const
		{
			const float F = FMath::Clamp(Time / Length, 0.0f, 1.0f) * float(A.Num() - 1);
			const int32 i = FMath::Min(FMath::FloorToInt(F), A.Num() - 2);
			return FMath::Lerp(A[i], A[i + 1], F - float(i));
		}
		FVector SwayAt(float Time) const { return At(Sway, Time); }
		FVector2D MidAt(float Time) const { return At(Mid, Time); }
	};

	/** 某个时刻脚踝在哪（模型自己的坐标）：从脚一路往上乘到骨盆，骨盆以上的骨头用参考姿势。 */
	FVector SampleHeroFoot(const UAnimSequence* Seq, const FReferenceSkeleton& Ref, int32 Foot, int32 Pelvis, const FTransform& AbovePelvis, double Time)
	{
		FTransform T = FTransform::Identity;
		const FAnimExtractContext At(Time);
		for (int32 b = Foot; b != INDEX_NONE; b = Ref.GetParentIndex(b))
		{
			FTransform Local = Ref.GetRefBonePose()[b];
			Seq->GetBoneTransform(Local, FSkeletonPoseBoneIndex(b), At, false);
			T = T * Local;
			if (b == Pelvis) break;
		}
		return (T * AbovePelvis).GetLocation();
	}

	void BuildHeroGround(FDysisHeroGround& G, const UAnimSequence* Seq, bool bStands, bool bLoops)
	{
		G = FDysisHeroGround();
		const USkeleton* Skeleton = Seq ? Seq->GetSkeleton() : nullptr;
		if (!Skeleton) return;
		const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
		const int32 Pelvis = Ref.FindBoneIndex(TEXT("pelvis"));
		const int32 Foot[2] = { Ref.FindBoneIndex(TEXT("foot_l")), Ref.FindBoneIndex(TEXT("foot_r")) };
		if (Pelvis == INDEX_NONE || Foot[0] == INDEX_NONE || Foot[1] == INDEX_NONE) return;
		const int32 Above = Ref.GetParentIndex(Pelvis);
		const FTransform AbovePelvis = Above != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Above) : FTransform::Identity;
		const float GroundZ = float(FMath::Min(FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Foot[0]).GetLocation().Z,
			FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Foot[1]).GetLocation().Z));   // 脚踩平时脚踝的高度

		G.Length = Seq->GetPlayLength();
		const int32 N = FMath::Max(2, FMath::CeilToInt(G.Length * float(HeroGroundHz)));
		TArray<FVector> P[2];
		double Lowest = TNumericLimits<double>::Max();
		for (int32 i = 0; i <= N; ++i)
			for (int32 k = 0; k < 2; ++k)
			{
				P[k].Add(SampleHeroFoot(Seq, Ref, Foot[k], Pelvis, AbovePelvis, double(G.Length) * i / N));
				Lowest = FMath::Min(Lowest, P[k].Last().Z);
			}
		G.Sway.SetNumZeroed(N + 1);
		G.Mid.SetNumUninitialized(N + 1);
		for (int32 i = 0; i <= N; ++i) G.Mid[i] = FVector2D((P[0][i] + P[1][i]) * 0.5);
		if (!bStands)
		{
			for (FVector& S : G.Sway) S.Z = -FMath::Max(0.0, Lowest - GroundZ);
			return;
		}
		FVector2D O = FVector2D::ZeroVector;
		for (int32 i = 0; i <= N; ++i)
		{
			G.Sway[i] = FVector(O.X, O.Y, -FMath::Max(0.0, FMath::Min(P[0][i].Z, P[1][i].Z) - GroundZ));
			if (i == N) break;
			// 谁低谁踩着地（差一厘米就基本全算它的）
			const double W0 = 1.0 / (1.0 + FMath::Exp(-(P[1][i].Z - P[0][i].Z) / 0.6));
			O -= W0 * FVector2D(P[0][i + 1] - P[0][i]) + (1.0 - W0) * FVector2D(P[1][i + 1] - P[1][i]);
		}
		// 循环的那一段：放完一圈人要回到原处，差的那一点（一两厘米）均摊到整圈里
		if (bLoops)
		{
			const FVector2D Drift(G.Sway[N]);
			for (int32 i = 0; i <= N; ++i) { G.Sway[i].X -= Drift.X * i / N; G.Sway[i].Y -= Drift.Y * i / N; }
		}
	}

	/** 一条腿：大腿、小腿、脚三根骨头，和参考姿势里脚踝的高度。 */
	struct FDysisHeroLeg
	{
		FBoneReference Thigh, Calf, Foot;
		float GroundZ = 0.0f;
		bool IsValid(const FBoneContainer& BC) const { return Thigh.IsValidToEvaluate(BC) && Calf.IsValidToEvaluate(BC) && Foot.IsValidToEvaluate(BC); }
	};

	/** 每段动作一个播放器，各有一个权重；想要的那一段（走和跑是两段按速度分）权重往上走、别的往 0 走，按权重把姿势混在一起。
	 *  混完以后再修一遍（FixBody）：
	 *    · 按各段的“落地表”把骨盆挪回去（站着脚不滑、跑的时候脚着地）；
	 *    · 步幅按速度放大：脚相对胯的前后距离乘一个数，腿用两段骨头的反解跟过去。
	 *  走和跑两段不各放各的：共用一个“相位”（迈到一个循环的哪儿了），两段的时刻都由它定，步子始终对齐。 */
	struct FDysisHeroRoot : FAnimNode_Base
	{
		TArray<FAnimNode_SequencePlayer_Standalone> Players;
		TArray<FDysisHeroGround> Ground;
		TArray<float> Weights, Want;
		TArray<FVector2D> Keep;       // 站着的几段之间换的时候：新的一段整个平移多少，两脚的中点才接得上
		int32 Target = 0;
		float Rate = 1.0f;
		int32 RestartSerial = 0, SeenRestartSerial = 0;

		// 每帧从游戏那边抄过来的
		bool bFixLegs = true;
		float RunBlend = 0.0f, CycleRate = 0.0f, StrideTarget = 1.0f, CyclesPerClip = 2.0f, RunPhaseLead = 0.5f, StrideDropCm = 0.0f;
		// 这边自己记的
		float Stride = 1.0f;
		double Phase = 0.0;            // 迈到第几个循环了（对 CyclesPerClip 取余）
		bool bIdleHeld = false;        // 这一回站定以后“站着”那一段放过了：伸手、落地完了回来接着放，不从头来
		FVector LastOffset = FVector::ZeroVector;
		FBoneReference Pelvis;
		FDysisHeroLeg Legs[2];

		virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override
		{
			Weights.Init(0.0f, Players.Num());
			Want.Init(0.0f, Players.Num());
			Keep.Init(FVector2D::ZeroVector, Players.Num());
			if (Weights.Num() > 0) Weights[0] = 1.0f;
			for (FAnimNode_SequencePlayer_Standalone& P : Players) P.Initialize_AnyThread(Context);
		}
		virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override
		{
			for (FAnimNode_SequencePlayer_Standalone& P : Players) P.CacheBones_AnyThread(Context);
			const FBoneContainer& BC = Context.AnimInstanceProxy->GetRequiredBones();
			Pelvis.BoneName = TEXT("pelvis");
			Pelvis.Initialize(BC);
			static const TCHAR* const Names[2][3] = { { TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l") }, { TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r") } };
			for (int32 i = 0; i < 2; ++i)
			{
				FDysisHeroLeg& L = Legs[i];
				L.Thigh.BoneName = Names[i][0]; L.Calf.BoneName = Names[i][1]; L.Foot.BoneName = Names[i][2];
				L.Thigh.Initialize(BC); L.Calf.Initialize(BC); L.Foot.Initialize(BC);
				if (L.Foot.BoneIndex != INDEX_NONE)
					L.GroundZ = float(FAnimationRuntime::GetComponentSpaceTransformRefPose(BC.GetReferenceSkeleton(), L.Foot.BoneIndex).GetLocation().Z);
			}
		}
		virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override
		{
			if (!Players.IsValidIndex(Target) || Players.Num() < int32(EDysisHeroClip::Count)) return;
			const float Dt = Context.GetDeltaTime();
			// 这一帧各段想要多少
			for (float& W : Want) W = 0.0f;
			const bool bLoco = Target == HeroClip_Walk || Target == HeroClip_Run;
			if (bLoco) { Want[HeroClip_Walk] = 1.0f - RunBlend; Want[HeroClip_Run] = RunBlend; }
			else Want[Target] = 1.0f;
			if (RestartSerial != SeenRestartSerial)
			{
				if (!bLoco) Players[Target].SetAccumulatedTime(0.0f);
				SeenRestartSerial = RestartSerial;
			}
			if (Weights[HeroClip_Idle] + Weights[HeroClip_Land] + Weights[HeroClip_Touch] < KINDA_SMALL_NUMBER) bIdleHeld = false;   // 走开过了
			if (Weights[HeroClip_Walk] + Weights[HeroClip_Run] < KINDA_SMALL_NUMBER) Phase = 0.0;                            // 起步：从左脚踩在正下方开始

			const float Step = Dt / HeroBlendSeconds;
			float Sum = 0.0f;
			for (int32 i = 0; i < Players.Num(); ++i)
			{
				if (Want[i] > 0.0f && Weights[i] < KINDA_SMALL_NUMBER)
				{
					// 一段动作刚被选中（权重还是 0）：从头放。“站着”那一段例外——伸手、落地完了回来，接着刚才的放（人还在刚才的地方）
					if (!(i == HeroClip_Idle && bIdleHeld))
					{
						Players[i].Initialize_AnyThread(FAnimationInitializeContext(Context.AnimInstanceProxy));
						Players[i].CacheBones_AnyThread(FAnimationCacheBonesContext(Context.AnimInstanceProxy));
					}
					// 从“站着”换到伸手：整个人平移一点，两脚的中点接在原地（不然换的那一下两只脚一起滑）
					Keep[i] = FVector2D::ZeroVector;
					if (i != HeroClip_Idle && HeroClipStands(i) && Weights[HeroClip_Idle] > 0.5f && Ground[HeroClip_Idle].IsValid() && Ground[i].IsValid())
					{
						const float TI = Players[HeroClip_Idle].GetAccumulatedTime();
						Keep[i] = Ground[HeroClip_Idle].MidAt(TI) + FVector2D(Ground[HeroClip_Idle].SwayAt(TI)) - Ground[i].MidAt(0.0f);
					}
				}
				Weights[i] = Weights[i] < Want[i] ? FMath::Min(Want[i], Weights[i] + Step) : FMath::Max(Want[i], Weights[i] - Step);
				Sum += Weights[i];
			}
			if (Weights[HeroClip_Idle] > KINDA_SMALL_NUMBER) bIdleHeld = true;
			// 步幅 0.25 秒跟到；相位往前走
			Stride = Stride < StrideTarget ? FMath::Min(StrideTarget, Stride + Dt * 2.0f) : FMath::Max(StrideTarget, Stride - Dt * 2.0f);
			const double Cycles = FMath::Max(1.0, double(CyclesPerClip));
			Phase = FMath::Fmod(Phase + double(CycleRate) * Dt, Cycles);
			if (Sum <= 0.0f) return;
			for (int32 i = 0; i < Players.Num(); ++i)
			{
				Weights[i] /= Sum;
				if (Weights[i] <= KINDA_SMALL_NUMBER) continue;
				if (i == HeroClip_Walk || i == HeroClip_Run)
				{
					// 走、跑：时刻由相位定，播放器自己不往前走
					const double At = FMath::Fmod(Phase + (i == HeroClip_Run ? double(RunPhaseLead) : 0.0), Cycles) / Cycles;
					Players[i].SetAccumulatedTime(float(At) * Ground[i].Length);
					Players[i].SetPlayRate(0.0f);
				}
				else if (i == Target) Players[i].SetPlayRate(Rate);
				Players[i].Update_AnyThread(Context.FractionalWeight(Weights[i]));
			}
		}
		virtual void Evaluate_AnyThread(FPoseContext& Output) override
		{
			TArray<FCompactPose> Poses;
			TArray<FBlendedCurve> Curves;
			TArray<UE::Anim::FStackAttributeContainer> Attributes;
			TArray<float> ActiveWeights;
			for (int32 i = 0; i < Players.Num(); ++i)
				if (Weights[i] > KINDA_SMALL_NUMBER)
				{
					FPoseContext Pose(Output);
					Players[i].Evaluate_AnyThread(Pose);
					Poses.Add(MoveTemp(Pose.Pose));
					Curves.Add(MoveTemp(Pose.Curve));
					Attributes.Add(MoveTemp(Pose.CustomAttributes));
					ActiveWeights.Add(Weights[i]);
				}
			if (Poses.Num() == 0) { Output.ResetToRefPose(); return; }
			FAnimationPoseData Data(Output);
			FAnimationRuntime::BlendPosesTogether(Poses, Curves, Attributes, ActiveWeights, Data);
			Output.Pose.NormalizeRotations();
			LastOffset = FVector::ZeroVector;
			if (bFixLegs) FixBody(Output);
		}

		void FixBody(FPoseContext& Output)
		{
			const FBoneContainer& BC = Output.Pose.GetBoneContainer();
			if (!Pelvis.IsValidToEvaluate(BC)) return;
			// 骨盆该挪多少：各段的按权重加起来
			FVector Off = FVector::ZeroVector;
			for (int32 i = 0; i < Players.Num(); ++i)
				if (Weights[i] > KINDA_SMALL_NUMBER && Ground[i].IsValid())
					Off += Weights[i] * (Ground[i].SwayAt(Players[i].GetAccumulatedTime()) + FVector(Keep[i].X, Keep[i].Y, 0.0));
			Off.Z -= StrideDropCm * FMath::Max(Stride - 1.0f, 0.0f) / 0.3f;   // 步子迈大了：人矮一点，腿才够得着
			const bool bMove = !Off.IsNearlyZero(0.01);
			const bool bWarp = FMath::Abs(Stride - 1.0f) > 0.005f && Legs[0].IsValid(BC) && Legs[1].IsValid(BC);
			if (!bMove && !bWarp) return;
			LastOffset = Off;

			FCSPose<FCompactPose> CS;
			CS.InitPose(Output.Pose);
			if (bMove)
			{
				const FCompactPoseBoneIndex Pi = Pelvis.GetCompactPoseIndex(BC);
				FTransform T = CS.GetComponentSpaceTransform(Pi);
				T.AddToTranslation(Off);
				TArray<FBoneTransform> One;
				One.Emplace(Pi, T);
				CS.SafeSetCSBoneTransforms(One);
			}
			if (bWarp)
				for (const FDysisHeroLeg& L : Legs)
				{
					const FCompactPoseBoneIndex Ti = L.Thigh.GetCompactPoseIndex(BC), Ci = L.Calf.GetCompactPoseIndex(BC), Fi = L.Foot.GetCompactPoseIndex(BC);
					FTransform ThighT = CS.GetComponentSpaceTransform(Ti), CalfT = CS.GetComponentSpaceTransform(Ci), FootT = CS.GetComponentSpaceTransform(Fi);
					const FVector Hip = ThighT.GetLocation(), Knee = CalfT.GetLocation(), Foot = FootT.GetLocation();
					// 步幅：脚相对胯的前后距离放大（模型的前方是 +Y；人总是脸朝着走的方向）
					FVector Goal = Foot;
					Goal.Y = Hip.Y + (Foot.Y - Hip.Y) * Stride;
					Goal.Z = FMath::Max(Goal.Z, double(L.GroundZ));   // 不踩进地里
					// 膝盖朝原来弯的那一边
					FVector Bend = (Knee - (Hip + Foot) * 0.5).GetSafeNormal();
					if (Bend.IsNearlyZero()) Bend = FVector(0.0, 1.0, 0.0);
					AnimationCore::SolveTwoBoneIK(ThighT, CalfT, FootT, Knee + Bend * 100.0, Goal, /*bAllowStretching*/ false, 1.0, 1.0);
					TArray<FBoneTransform> Three;
					Three.Emplace(Ti, ThighT); Three.Emplace(Ci, CalfT); Three.Emplace(Fi, FootT);
					CS.SafeSetCSBoneTransforms(Three);
				}
			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
		}
	};

	struct FDysisHeroProxy : FAnimInstanceProxy
	{
		FDysisHeroRoot Root;
		explicit FDysisHeroProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

		virtual void Initialize(UAnimInstance* Instance) override
		{
			const UDysisHeroAnimInstance* Hero = CastChecked<UDysisHeroAnimInstance>(Instance);
			Root.Players.SetNum(Hero->Clips.Num());
			Root.Ground.SetNum(Hero->Clips.Num());
			for (int32 i = 0; i < Hero->Clips.Num(); ++i)
			{
				Root.Players[i].SetSequence(Hero->Clips[i]);
				Root.Players[i].SetLoopAnimation(HeroClipLoops(i));
				if (HeroClipOnGround(i)) BuildHeroGround(Root.Ground[i], Hero->Clips[i], HeroClipStands(i), HeroClipLoops(i));
			}
			FAnimInstanceProxy::Initialize(Instance);
		}
		virtual FAnimNode_Base* GetCustomRootNode() override { return &Root; }
		virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override
		{
			OutNodes.Add(&Root);
			for (FAnimNode_SequencePlayer_Standalone& P : Root.Players) OutNodes.Add(&P);
		}
		virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
		{
			FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
			const UDysisHeroAnimInstance* Hero = CastChecked<UDysisHeroAnimInstance>(Instance);
			Root.Target = Hero->ActiveClip;
			Root.Rate = Hero->PlaybackRate;
			Root.RestartSerial = Hero->RestartSerial;
			Root.bFixLegs = Hero->bFixLegs;
			Root.RunBlend = Hero->RunBlend;
			Root.CycleRate = Hero->CycleRate;
			Root.StrideTarget = Hero->StrideScale;
			Root.CyclesPerClip = Hero->CyclesPerClip;
			Root.RunPhaseLead = Hero->RunPhaseLead;
			Root.StrideDropCm = Hero->StrideDropCm;
		}
	};
}

void UDysisHeroAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const ADysisCharacter* Hero = Cast<ADysisCharacter>(TryGetPawnOwner());
	const UCharacterMovementComponent* Move = Hero ? Hero->GetCharacterMovement() : nullptr;
	if (!Move) return;
	const FVector V = Hero->GetVelocity();
	const float Speed = float(V.Size2D());
	// 动作用的速度：往上慢一拍跟（起步先迈一小步），往下立刻跟
	GaitSpeed = Speed > GaitSpeed ? FMath::Min(Speed, GaitSpeed + GaitAccel * DeltaSeconds) : Speed;
	EDysisHeroClip Clip = EDysisHeroClip::Idle;
	PlaybackRate = 1.0f;
	StrideScale = 1.0f;
	CycleRate = 0.0f;
	if (Move->IsFalling())
	{
		// 往上升的那一段放起跳，其余时间放下落
		Clip = V.Z > 80.0 ? EDysisHeroClip::JumpStart : EDysisHeroClip::FallLoop;
		if (Clip == EDysisHeroClip::JumpStart) PlaybackRate = 1.8f;
	}
	else if (Hero->GetLandingSeconds() > 0.0f && Speed < 80.0f) { Clip = EDysisHeroClip::Land; PlaybackRate = 2.4f; }
	else if (Hero->IsTouching()) Clip = EDysisHeroClip::TouchForward;
	else if (Hero->IsAscendingSteps()) { Clip = EDysisHeroClip::RunUpstairs; PlaybackRate = FMath::Clamp(Speed / 320.0f, 0.7f, 2.0f); }
	else if (Speed > 8.0f && Clips.IsValidIndex(HeroClip_Run) && Clips[HeroClip_Walk] && Clips[HeroClip_Run])
	{
		// 走和跑：速度到 WalkBlendFromSpeed 为止全是“走”，再往上按比例混进“跑”，到“跑”本身的速度以上全是“跑”。
		// 比动作本身快（慢）的部分，步幅和步子的快慢各担一半（开方）；脚在地上往后蹬的速度始终 = 人往前走的速度
		const float Cycles = FMath::Max(CyclesPerClip, 1.0f);
		const float WalkStride = WalkClipSpeed * Clips[HeroClip_Walk]->GetPlayLength() / Cycles;    // 一个循环往前走多远（厘米）
		const float RunStride = RunClipSpeed * Clips[HeroClip_Run]->GetPlayLength() / Cycles;
		const float BlendFrom = FMath::Clamp(WalkBlendFromSpeed, WalkClipSpeed, RunClipSpeed - 1.0f);
		auto WalkScale = [this](float V) { return FMath::Clamp(FMath::Sqrt(V / FMath::Max(WalkClipSpeed, 1.0f)), StrideMin, StrideMax); };
		float StrideCm;
		if (GaitSpeed <= BlendFrom)
		{
			RunBlend = 0.0f;
			StrideScale = WalkScale(GaitSpeed);
			StrideCm = WalkStride * StrideScale;
		}
		else if (GaitSpeed >= RunClipSpeed)
		{
			RunBlend = 1.0f;
			StrideScale = FMath::Clamp(FMath::Sqrt(GaitSpeed / RunClipSpeed), StrideMin, StrideMax);
			StrideCm = RunStride * StrideScale;
		}
		else
		{
			RunBlend = (GaitSpeed - BlendFrom) / (RunClipSpeed - BlendFrom);
			StrideScale = FMath::Lerp(WalkScale(BlendFrom), 1.0f, RunBlend);
			StrideCm = FMath::Lerp(WalkStride * WalkScale(BlendFrom), RunStride, RunBlend);
		}
		CycleRate = FMath::Min(GaitSpeed / FMath::Max(StrideCm, 1.0f), 4.0f);
		Clip = RunBlend >= 0.5f ? EDysisHeroClip::Run : EDysisHeroClip::Walk;
	}
	ActiveClip = int32(Clip);
	RestartSerial = Hero->GetTouchSerial();
	// 脚落地：一个循环两下。相位和动作那边的一起走（那边在走、跑两段都退场以后才清零，这里一停就清，只差在急停再起步的那一瞬）
	if (CycleRate > 0.0f)
	{
		const double Before = FMath::FloorToDouble(GaitPhase * 2.0 - 2.0 * FootfallPhase);
		GaitPhase += double(CycleRate) * DeltaSeconds;
		if (FMath::FloorToDouble(GaitPhase * 2.0 - 2.0 * FootfallPhase) > Before) ++StepCount;
	}
	else GaitPhase = 0.0;
}

FName UDysisHeroAnimInstance::GetActiveClipName() const
{
	return Clips.IsValidIndex(ActiveClip) && Clips[ActiveClip] ? Clips[ActiveClip]->GetFName() : NAME_None;
}

FString UDysisHeroAnimInstance::DescribeGait()
{
	const FDysisHeroRoot& R = GetProxyOnGameThread<FDysisHeroProxy>().Root;
	FString W;
	for (int32 i = 0; i < R.Weights.Num() && i < int32(EDysisHeroClip::Count); ++i)
		W += FString::Printf(TEXT("%s\"%s\":%.3f"), i ? TEXT(",") : TEXT(""), HeroClipNames[i], R.Weights[i]);
	// “站着”那一段的落地表：骨盆左右、前后各挪多远（核对用：和从动作里另量的数应当一致）
	FVector Lo(0.0), Hi(0.0);
	if (R.Ground.IsValidIndex(HeroClip_Idle))
		for (const FVector& S : R.Ground[HeroClip_Idle].Sway) { Lo = Lo.ComponentMin(S); Hi = Hi.ComponentMax(S); }
	float Drop[3] = { 0.0f, 0.0f, 0.0f };
	const int32 Which[3] = { HeroClip_Walk, HeroClip_Run, HeroClip_Upstairs };
	for (int32 k = 0; k < 3; ++k)
		if (R.Ground.IsValidIndex(Which[k]) && R.Ground[Which[k]].IsValid()) Drop[k] = float(-R.Ground[Which[k]].Sway[0].Z);
	return FString::Printf(TEXT("{\"clip\":\"%s\",\"steps\":%d,\"weights\":{%s},\"phase\":%.3f,\"cycleRate\":%.3f,\"stride\":%.3f,\"runBlend\":%.3f,\"gaitSpeed\":%.1f,\"offset\":[%.2f,%.2f,%.2f],")
		TEXT("\"idleSwayX\":[%.1f,%.1f],\"idleSwayY\":[%.1f,%.1f],\"drop\":{\"walk\":%.1f,\"run\":%.1f,\"upstairs\":%.1f}}"),
		*GetActiveClipName().ToString(), StepCount, *W, R.Phase, CycleRate, R.Stride, RunBlend, GaitSpeed, R.LastOffset.X, R.LastOffset.Y, R.LastOffset.Z,
		Lo.X, Hi.X, Lo.Y, Hi.Y, Drop[0], Drop[1], Drop[2]);
}

FAnimInstanceProxy* UDysisHeroAnimInstance::CreateAnimInstanceProxy()
{
	// 动作资产用到时再找（不在构造函数里攥着：导入脚本重跑时引擎要能换掉它们）
	if (Clips.Num() == 0)
		for (const TCHAR* Name : HeroClipNames)
			Clips.Add(LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Dysis/Characters/Hero/Animations/A_%s.A_%s"), Name, Name)));
	return new FDysisHeroProxy(this);
}

void UDysisHeroAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
