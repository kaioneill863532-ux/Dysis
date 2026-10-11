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
		// 上半身（见头文件 bFixUpperBody）
		bool bFixUpper = true;
		float ArmBase = 1.15f, ArmGain = 1.6f, TwistDeg = 5.0f, UpstairsLeanDeg = 8.0f, UpstairsArmPhase = 0.478f;
		float ExtraDrop = 0.0f, LastDt = 0.0f;                       // 腿够不着时另外再矮多少（厘米）
		float Slope = 0.0f, StairLift = 1.9f;                        // 脚下的坡度；上下台阶时脚抬高到几倍
		float LastTwist = 0.0f, LastArmGain = 1.0f, LastUpper = 0.0f, LastArmDeg[2] = { 0.0f, 0.0f };   // 上一帧实际用的数（测试和排查用）
		FBoneReference Spine1, Spine2, Chest, Neck, UpperArm[2], Hand[2];
		TArray<FCompactPoseBoneIndex> UpperBones;                    // spine_01 和它底下所有的骨头
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
			Spine1.BoneName = TEXT("spine_01"); Spine2.BoneName = TEXT("spine_02"); Chest.BoneName = TEXT("spine_03"); Neck.BoneName = TEXT("neck_01");
			UpperArm[0].BoneName = TEXT("upperarm_l"); UpperArm[1].BoneName = TEXT("upperarm_r"); Hand[0].BoneName = TEXT("hand_l"); Hand[1].BoneName = TEXT("hand_r");
			for (FBoneReference* R : { &Spine1, &Spine2, &Chest, &Neck, &UpperArm[0], &UpperArm[1], &Hand[0], &Hand[1] }) R->Initialize(BC);
			UpperBones.Reset();
			if (Spine1.IsValidToEvaluate(BC))
			{
				const FCompactPoseBoneIndex Top = Spine1.GetCompactPoseIndex(BC);
				TArray<bool> In;
				In.Init(false, BC.GetCompactPoseNumBones());
				for (int32 b = 0; b < In.Num(); ++b)      // 骨头是按“爹在前”排的
				{
					const FCompactPoseBoneIndex Bi(b), Parent = BC.GetParentBoneIndex(Bi);
					In[b] = Bi == Top || (Parent.GetInt() != INDEX_NONE && In[Parent.GetInt()]);
					if (In[b]) UpperBones.Add(Bi);
				}
			}
		}
		bool UpperValid(const FBoneContainer& BC) const
		{
			return Spine2.IsValidToEvaluate(BC) && Chest.IsValidToEvaluate(BC) && Neck.IsValidToEvaluate(BC)
				&& UpperArm[0].IsValidToEvaluate(BC) && UpperArm[1].IsValidToEvaluate(BC) && Hand[0].IsValidToEvaluate(BC) && Hand[1].IsValidToEvaluate(BC);
		}
		virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override
		{
			if (!Players.IsValidIndex(Target) || Players.Num() < int32(EDysisHeroClip::Count)) return;
			const float Dt = Context.GetDeltaTime();
			LastDt = Dt;
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
			// 上台阶时腰以上用“走”那一段的姿势：那一段这时候没在放，这里替它定时刻——跟着“上台阶”那一段走，两段的腿对齐（手和腿才是反着摆的）
			if (bFixUpper && Weights[HeroClip_Upstairs] > KINDA_SMALL_NUMBER && Weights[HeroClip_Walk] <= KINDA_SMALL_NUMBER
				&& Ground[HeroClip_Upstairs].Length > 0.0f && Ground[HeroClip_Walk].Length > 0.0f)
			{
				const double Up = double(Players[HeroClip_Upstairs].GetAccumulatedTime()) / Ground[HeroClip_Upstairs].Length + double(UpstairsArmPhase);
				Players[HeroClip_Walk].SetAccumulatedTime(float(Up - FMath::FloorToDouble(Up)) * Ground[HeroClip_Walk].Length / float(Cycles));
				Players[HeroClip_Walk].SetPlayRate(0.0f);
				Players[HeroClip_Walk].Update_AnyThread(Context.FractionalWeight(Weights[HeroClip_Upstairs]));
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
			// 上台阶：腰以上换成“走”那一段的（美术“上台阶”那一段里两只手一直架在胸前）
			const float UpK = Weights.IsValidIndex(HeroClip_Upstairs) ? Weights[HeroClip_Upstairs] : 0.0f;
			if (bFixUpper && UpK > KINDA_SMALL_NUMBER && UpperBones.Num() > 0)
			{
				FPoseContext Walk(Output);
				Players[HeroClip_Walk].Evaluate_AnyThread(Walk);
				for (const FCompactPoseBoneIndex B : UpperBones)
				{
					FTransform T = Output.Pose[B];
					T.BlendWith(Walk.Pose[B], UpK);
					Output.Pose[B] = T;
				}
				Output.Pose.NormalizeRotations();
			}
			LastOffset = FVector::ZeroVector;
			LastTwist = 0.0f; LastArmGain = 1.0f; LastUpper = 0.0f; LastArmDeg[0] = LastArmDeg[1] = 0.0f;
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
			// 步子迈大了：人矮一点，腿才够得着。矮的只是胯——脚还在动作里原来的高度上（下面算脚的落点时把这一截加回去；
			// 头一版脚也跟着矮下去、再被地面顶回来，抬脚的高度就没了，腿反而更够不着）
			// 步幅、坡度、“腿够不着就把胯压低”这些修正是给走和跑用的：按这两段动作现在占多少来算（LocoK）。
			// 头一版不管放的是什么动作都修——起跳那一下腿是蹬直的，被当成“够不着”把胯猛地往下拽，落地时又来一次，
			// 看着就是抽了一下（2026-10-11 用户：跳起来的时候动作有点抽搐）。现在起跳、下落、落地时它们跟着走跑的比重淡出去。
			const float Loco = Weights[HeroClip_Walk] + Weights[HeroClip_Run], LocoK = FMath::Clamp(Loco, 0.0f, 1.0f);
			const float StrideNow = 1.0f + (Stride - 1.0f) * LocoK;
			const float BaseDrop = StrideDropCm * FMath::Max(StrideNow - 1.0f, 0.0f) / 0.3f;
			Off.Z -= BaseDrop;
			const bool bMove = !Off.IsNearlyZero(0.01);
			// 脚下有坡（上下台阶）：脚按坡度落，迈步时抬高一些
			const float SlopeNow = Slope * Loco, SlopeK = FMath::Clamp(FMath::Abs(SlopeNow) / 0.42f, 0.0f, 1.0f), LiftK = 1.0f + (StairLift - 1.0f) * SlopeK;
			const bool bWarp = LocoK > 0.02f && (FMath::Abs(StrideNow - 1.0f) > 0.005f || FMath::Abs(SlopeNow) > 0.02f) && Legs[0].IsValid(BC) && Legs[1].IsValid(BC);
			// 上半身：只在走、上台阶的时候动（跑的那一段手本来就摆得开）。UpK = 往上走的坡有多陡（身子往前倾多少）
			const float UpK = FMath::Max(Weights[HeroClip_Upstairs], FMath::Clamp(SlopeNow / 0.42f, 0.0f, 1.0f) * Weights[HeroClip_Walk]);
			const float UpperK = bFixUpper && UpperValid(BC) ? FMath::Clamp(Weights[HeroClip_Walk] + Weights[HeroClip_Upstairs], 0.0f, 1.0f) : 0.0f;
			const bool bUpper = UpperK > KINDA_SMALL_NUMBER;
			if (!bWarp) ExtraDrop = FMath::Max(0.0f, ExtraDrop - 30.0f * LastDt);
			if (!bMove && !bWarp && !bUpper) return;
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
			{
				// 步子迈得大、两脚分得最开的那一下腿会够不着：够不着多少，人就再矮多少（马上矮下去、慢慢起来）。
				// 人迈大步时本来就是这样一起一伏的；整圈都按最矮的那一下蹲着走反而怪。
				float Need = 0.0f;
				for (const FDysisHeroLeg& L : Legs)
				{
					const FVector Hip = CS.GetComponentSpaceTransform(L.Thigh.GetCompactPoseIndex(BC)).GetLocation(), Knee = CS.GetComponentSpaceTransform(L.Calf.GetCompactPoseIndex(BC)).GetLocation(),
						Foot = CS.GetComponentSpaceTransform(L.Foot.GetCompactPoseIndex(BC)).GetLocation();
					const double Reach = ((Knee - Hip).Size() + (Foot - Knee).Size()) * 0.995;
					FVector Goal(Foot.X, Hip.Y + (Foot.Y - Hip.Y) * StrideNow, FMath::Max(Foot.Z + BaseDrop, double(L.GroundZ)));
					Goal.Z = L.GroundZ + (Goal.Z - L.GroundZ) * LiftK + SlopeNow * (Goal.Y - Hip.Y) * (SlopeNow * (Goal.Y - Hip.Y) < 0.0 ? 0.6 : 1.0);
					const double Flat = FVector2D(Goal - Hip).Size();
					if (Flat < Reach) Need = FMath::Max(Need, float((Hip.Z - Goal.Z) - FMath::Sqrt(Reach * Reach - Flat * Flat)));
				}
				ExtraDrop = FMath::Max(FMath::Clamp(Need * LocoK, 0.0f, 12.0f), ExtraDrop - 30.0f * LastDt);
				if (ExtraDrop > 0.01f)
				{
					const FCompactPoseBoneIndex Pi = Pelvis.GetCompactPoseIndex(BC);
					FTransform T = CS.GetComponentSpaceTransform(Pi);
					T.AddToTranslation(FVector(0.0, 0.0, -ExtraDrop));
					TArray<FBoneTransform> One;
					One.Emplace(Pi, T);
					CS.SafeSetCSBoneTransforms(One);
					LastOffset.Z -= ExtraDrop;
				}
				for (const FDysisHeroLeg& L : Legs)
				{
					const FCompactPoseBoneIndex Ti = L.Thigh.GetCompactPoseIndex(BC), Ci = L.Calf.GetCompactPoseIndex(BC), Fi = L.Foot.GetCompactPoseIndex(BC);
					FTransform ThighT = CS.GetComponentSpaceTransform(Ti), CalfT = CS.GetComponentSpaceTransform(Ci), FootT = CS.GetComponentSpaceTransform(Fi);
					const FVector Hip = ThighT.GetLocation(), Knee = CalfT.GetLocation(), Foot = FootT.GetLocation();
					// 步幅：脚相对胯的前后距离放大（模型的前方是 +Y；人总是脸朝着走的方向）
					FVector Goal = Foot;
					Goal.Y = Hip.Y + (Foot.Y - Hip.Y) * StrideNow;
					Goal.Z = FMath::Max(Goal.Z + BaseDrop + ExtraDrop, double(L.GroundZ));   // 脚的高度照动作里原来的；不踩进地里
					// 坡上：迈步抬高一些；前脚落在高处、后脚在低处（低处那只脚只探六成：人上楼时后脚是踮着的，全探下去胯就得跟着蹲）
					Goal.Z = L.GroundZ + (Goal.Z - L.GroundZ) * LiftK + SlopeNow * (Goal.Y - Hip.Y) * (SlopeNow * (Goal.Y - Hip.Y) < 0.0 ? 0.6 : 1.0);
					// 膝盖朝前弯（模型的前方是 +Y），只留原来动作里一点点往外撇的量。
					// 头一版是“朝原来弯的那一边”：腿快伸直的时候膝盖几乎就在胯和脚的连线上，“那一边”全凭零点几厘米的偏差定——
					// 步幅一放大（腿更直），膝盖就一会儿朝前一会儿朝外，看着是腿往外撇（2026-10-10 用户：膝盖怪怪的、腿外扩）
					FVector Bend = Knee - (Hip + Foot) * 0.5;
					const FVector Was = Bend.GetSafeNormal();
					Bend = FVector(FMath::Clamp(Bend.X, -1.5, 1.5) * 0.25, 10.0, 0.0).GetSafeNormal();
					if (LocoK < 0.999f)      // 正在换到别的动作（起跳、落地、站住）：脚的落点、膝盖的朝向都跟着淡回动作里原来的
					{
						Goal = FMath::Lerp(Foot, Goal, double(LocoK));
						if (!Was.IsNearlyZero()) Bend = FMath::Lerp(Was, Bend, double(LocoK)).GetSafeNormal();
					}
					AnimationCore::SolveTwoBoneIK(ThighT, CalfT, FootT, Knee + Bend * 100.0, Goal, /*bAllowStretching*/ false, 1.0, 1.0);
					TArray<FBoneTransform> Three;
					Three.Emplace(Ti, ThighT); Three.Emplace(Ci, CalfT); Three.Emplace(Fi, FootT);
					CS.SafeSetCSBoneTransforms(Three);
				}
			}
			if (bUpper)
			{
				auto Turn = [&CS](FCompactPoseBoneIndex Bi, const FQuat& By)      // 一根骨头原地转（它底下的跟着转）
				{
					FTransform T = CS.GetComponentSpaceTransform(Bi);
					T.SetRotation((By * T.GetRotation()).GetNormalized());
					TArray<FBoneTransform> One;
					One.Emplace(Bi, T);
					CS.SafeSetCSBoneTransforms(One);
				};
				// 两只手这一刻各在胸口前面（+）还是后面（−）多远；垂着不摆时手在胸口前 2 cm
				const double ChestY = CS.GetComponentSpaceTransform(Chest.GetCompactPoseIndex(BC)).GetLocation().Y;
				float D[2], Len[2];
				for (int32 k = 0; k < 2; ++k)
				{
					const FVector Sh = CS.GetComponentSpaceTransform(UpperArm[k].GetCompactPoseIndex(BC)).GetLocation(), Hd = CS.GetComponentSpaceTransform(Hand[k].GetCompactPoseIndex(BC)).GetLocation();
					D[k] = float(Hd.Y - ChestY) - 2.0f;
					Len[k] = FMath::Max(float((Hd - Sh).Size()), 20.0f);
				}
				const float G = ArmBase + ArmGain * FMath::Max(Stride - 1.0f, 0.0f) + 0.35f * UpK;
				// 肩膀跟着手拧（左手在前，左肩也往前；模型的左边是 +X、前面是 +Y）；上台阶时身子往前倾。头少跟一些，脸还是朝前
				const float Twist = TwistDeg * FMath::Clamp((D[0] - D[1]) / 26.0f, -1.5f, 1.5f) * FMath::Min(G * 0.5f, 1.0f) * UpperK, Lean = -UpstairsLeanDeg * UpK;
				const FQuat Body = FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Twist)) * FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Lean));
				Turn(Spine2.GetCompactPoseIndex(BC), Body);
				Turn(Neck.GetCompactPoseIndex(BC), FQuat(FVector::XAxisVector, FMath::DegreesToRadians(-Lean * 0.6f)) * FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(-Twist * 0.8f)));
				// 手臂：这一刻偏开多少，再多摆（G − 1）倍
				for (int32 k = 0; k < 2; ++k)
				{
					const float Ang = (G - 1.0f) * FMath::Atan2(D[k], Len[k]) * UpperK;
					Turn(UpperArm[k].GetCompactPoseIndex(BC), FQuat(FVector::XAxisVector, Ang));
					LastArmDeg[k] = FMath::RadiansToDegrees(Ang);
				}
				LastTwist = Twist; LastArmGain = G; LastUpper = UpperK;
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
			Root.bFixUpper = Hero->bFixUpperBody;
			Root.ArmBase = Hero->ArmSwingBase; Root.ArmGain = Hero->ArmSwingGain; Root.TwistDeg = Hero->TorsoTwistDeg;
			Root.UpstairsLeanDeg = Hero->UpstairsLeanDeg; Root.UpstairsArmPhase = Hero->UpstairsArmPhase;
			Root.Slope = Hero->bStairGait ? Hero->GroundSlope : 0.0f; Root.StairLift = Hero->StairLift;
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
	// 脚下的坡度：最近 0.3 秒里升了多少 ÷ 走了多远。台阶是一级一级“跳”上去的（逐帧看是 0、0、17 cm…），要摊开算；传送、腾空不算
	{
		const FVector Loc = Hero->GetActorLocation();
		if (bSlopePrev && DeltaSeconds > 1.0e-4f)
		{
			const FVector D = Loc - SlopePrev;
			const bool bCount = !Move->IsFalling() && D.SizeSquared() < 200.0 * 200.0;
			const float Keep = FMath::Exp(-DeltaSeconds / 0.3f);
			SlopeRise = SlopeRise * Keep + (bCount ? float(D.Z) : 0.0f);
			SlopeRun = SlopeRun * Keep + (bCount ? float(D.Size2D()) : 0.0f);
		}
		SlopePrev = Loc; bSlopePrev = true;
		const float Raw = (SlopeRun > 12.0f && !Move->IsFalling() && Speed > 40.0f) ? FMath::Clamp(SlopeRise / SlopeRun, -0.45f, 0.45f) : 0.0f;
		GroundSlope = FMath::FInterpTo(GroundSlope, FMath::Abs(Raw) > 0.06f ? Raw : 0.0f, DeltaSeconds, 8.0f);
	}
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
	else if (!bStairGait && Hero->IsAscendingSteps()) { Clip = EDysisHeroClip::RunUpstairs; PlaybackRate = FMath::Clamp(Speed / 320.0f, 0.7f, 2.0f); }
	else if (Speed > 8.0f && Clips.IsValidIndex(HeroClip_Run) && Clips[HeroClip_Walk] && Clips[HeroClip_Run])
	{
		// 走和跑：速度到 WalkBlendFromSpeed 为止全是“走”，再往上按比例混进“跑”，到“跑”本身的速度以上全是“跑”。
		// 比动作本身快（慢）的部分，步幅和步子的快慢各担一半（开方）；脚在地上往后蹬的速度始终 = 人往前走的速度。
		// 步子的快慢有个顶（WalkCadenceSpeed：走这么快时的步频），过了这个顶，多出来的速度全靠步子迈大
		const float Cycles = FMath::Max(CyclesPerClip, 1.0f);
		const float WalkStride = WalkClipSpeed * Clips[HeroClip_Walk]->GetPlayLength() / Cycles;    // 一个循环往前走多远（厘米）
		const float RunStride = RunClipSpeed * Clips[HeroClip_Run]->GetPlayLength() / Cycles;
		const float BlendFrom = FMath::Clamp(WalkBlendFromSpeed, WalkClipSpeed, RunClipSpeed - 1.0f);
		auto WalkScale = [this](float V)
		{
			const float Clip = FMath::Max(WalkClipSpeed, 1.0f), Cap = FMath::Sqrt(FMath::Max(WalkCadenceSpeed, Clip) / Clip);
			const float Half = FMath::Sqrt(V / Clip);
			return FMath::Clamp(Half > Cap ? V / (Clip * Cap) : Half, StrideMin, StrideMax);
		};
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
		// 上下台阶：步子收小一点（一步跨两级还多，看着像在迈大步冲）
		if (bStairGait)
		{
			const float Shorten = FMath::Lerp(1.0f, StairStride, FMath::Clamp(FMath::Abs(GroundSlope) / 0.42f, 0.0f, 1.0f));
			StrideScale *= Shorten; StrideCm *= Shorten;
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
		TEXT("\"idleSwayX\":[%.1f,%.1f],\"idleSwayY\":[%.1f,%.1f],\"drop\":{\"walk\":%.1f,\"run\":%.1f,\"upstairs\":%.1f},")
		TEXT("\"extraDrop\":%.2f,\"slope\":%.3f,\"upper\":{\"k\":%.3f,\"armGain\":%.3f,\"armDeg\":[%.1f,%.1f],\"twist\":%.2f,\"bones\":%d,\"walkTime\":%.3f}}"),
		*GetActiveClipName().ToString(), StepCount, *W, R.Phase, CycleRate, R.Stride, RunBlend, GaitSpeed, R.LastOffset.X, R.LastOffset.Y, R.LastOffset.Z,
		Lo.X, Hi.X, Lo.Y, Hi.Y, Drop[0], Drop[1], Drop[2],
		R.ExtraDrop, GroundSlope, R.LastUpper, R.LastArmGain, R.LastArmDeg[0], R.LastArmDeg[1], R.LastTwist, R.UpperBones.Num(),
		R.Players.IsValidIndex(HeroClip_Walk) ? R.Players[HeroClip_Walk].GetAccumulatedTime() : 0.0f);
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
