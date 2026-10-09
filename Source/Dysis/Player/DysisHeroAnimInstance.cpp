#include "DysisHeroAnimInstance.h"
#include "DysisCharacter.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	const TCHAR* const HeroClipNames[int32(EDysisHeroClip::Count)] = {
		TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("RunUpstairs"), TEXT("JumpStart"), TEXT("FallLoop"), TEXT("Land"), TEXT("TouchForward") };
	// 哪几段是循环放的；其余放到头就停在最后一帧
	bool HeroClipLoops(int32 i)
	{
		return i == int32(EDysisHeroClip::Idle) || i == int32(EDysisHeroClip::Walk) || i == int32(EDysisHeroClip::Run)
			|| i == int32(EDysisHeroClip::RunUpstairs) || i == int32(EDysisHeroClip::FallLoop);
	}
	constexpr float HeroBlendSeconds = 0.16f;

	/** 每段动作一个播放器，各有一个权重；目标那一段的权重往 1 走、别的往 0 走，按权重把姿势混在一起。 */
	struct FDysisHeroRoot : FAnimNode_Base
	{
		TArray<FAnimNode_SequencePlayer_Standalone> Players;
		TArray<float> Weights;
		int32 Target = 0;
		float Rate = 1.0f;
		int32 RestartSerial = 0, SeenRestartSerial = 0;

		virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override
		{
			Weights.Init(0.0f, Players.Num());
			if (Weights.Num() > 0) Weights[0] = 1.0f;
			for (FAnimNode_SequencePlayer_Standalone& P : Players) P.Initialize_AnyThread(Context);
		}
		virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override
		{
			for (FAnimNode_SequencePlayer_Standalone& P : Players) P.CacheBones_AnyThread(Context);
		}
		virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override
		{
			if (!Players.IsValidIndex(Target)) return;
			if (RestartSerial != SeenRestartSerial)
			{
				Players[Target].SetAccumulatedTime(0.0f);
				SeenRestartSerial = RestartSerial;
			}
			const float Step = Context.GetDeltaTime() / HeroBlendSeconds;
			float Sum = 0.0f;
			for (int32 i = 0; i < Players.Num(); ++i)
			{
				// 一段动作刚被选中（权重还是 0）：从头放
				if (i == Target && Weights[i] < KINDA_SMALL_NUMBER)
				{
					Players[i].Initialize_AnyThread(FAnimationInitializeContext(Context.AnimInstanceProxy));
					Players[i].CacheBones_AnyThread(FAnimationCacheBonesContext(Context.AnimInstanceProxy));
				}
				Weights[i] = FMath::Clamp(Weights[i] + (i == Target ? Step : -Step), 0.0f, 1.0f);
				Sum += Weights[i];
			}
			if (Sum <= 0.0f) return;
			for (int32 i = 0; i < Players.Num(); ++i)
			{
				Weights[i] /= Sum;
				if (Weights[i] <= KINDA_SMALL_NUMBER) continue;
				if (i == Target) Players[i].SetPlayRate(Rate);
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
			for (int32 i = 0; i < Hero->Clips.Num(); ++i)
			{
				Root.Players[i].SetSequence(Hero->Clips[i]);
				Root.Players[i].SetLoopAnimation(HeroClipLoops(i));
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
	EDysisHeroClip Clip = EDysisHeroClip::Idle;
	PlaybackRate = 1.0f;
	if (Move->IsFalling())
	{
		// 往上升的那一段放起跳，其余时间放下落
		Clip = V.Z > 80.0 ? EDysisHeroClip::JumpStart : EDysisHeroClip::FallLoop;
		if (Clip == EDysisHeroClip::JumpStart) PlaybackRate = 1.8f;
	}
	else if (Hero->GetLandingSeconds() > 0.0f && Speed < 80.0f) { Clip = EDysisHeroClip::Land; PlaybackRate = 2.4f; }
	else if (Hero->IsTouching()) Clip = EDysisHeroClip::TouchForward;
	else if (Hero->IsAscendingSteps()) { Clip = EDysisHeroClip::RunUpstairs; PlaybackRate = FMath::Clamp(Speed / 320.0f, 0.7f, 2.0f); }
	else if (Speed > RunClipFromSpeed) { Clip = EDysisHeroClip::Run; PlaybackRate = FMath::Clamp(Speed / 420.0f, 0.6f, 1.6f); }
	else if (Speed > 8.0f) { Clip = EDysisHeroClip::Walk; PlaybackRate = FMath::Clamp(Speed / 170.0f, 0.35f, 1.6f); }
	ActiveClip = int32(Clip);
	RestartSerial = Hero->GetTouchSerial();
}

FName UDysisHeroAnimInstance::GetActiveClipName() const
{
	return Clips.IsValidIndex(ActiveClip) && Clips[ActiveClip] ? Clips[ActiveClip]->GetFName() : NAME_None;
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
