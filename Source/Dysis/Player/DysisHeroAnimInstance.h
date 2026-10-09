// 日落回廊 · 主角的动作：按人此刻的状态（站着 / 走 / 跑 / 上台阶 / 起跳 / 下落 / 落地 / 伸手）挑一段动作放，
// 换动作时用 0.16 秒过渡。没有动画蓝图：动作的播放和混合是这个类自己搭的（做法来自场景美术 2026-10-09 的工程）。
#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DysisHeroAnimInstance.generated.h"

class UAnimSequence;

/** 用到的那几段动作；顺序就是 Clips 里的下标。美术给的另外十段（鼓掌、环顾等）已导进工程，还没接按键。 */
enum class EDysisHeroClip : uint8 { Idle, Walk, Run, RunUpstairs, JumpStart, FallLoop, Land, TouchForward, Count };

UCLASS(Transient)
class DYSIS_API UDysisHeroAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** 现在放的是哪一段（A_Idle、A_Walk……；测试和排查用）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Hero")
	FName GetActiveClipName() const;

	/** 速度超过这个数就用“跑”的动作（厘米/秒）。美术的走路动作自然速度约 1.7 m/s、跑步约 4.2 m/s，
	 *  而游戏里走是 3.1 m/s、跑是 5.2 m/s（灰盒的数），所以平时走路放的是慢一点的“跑”。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float RunClipFromSpeed = 230.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> Clips;

	// 下面三个由 NativeUpdateAnimation 在游戏线程上算好，每帧抄给动作那一边
	int32 ActiveClip = 0;
	float PlaybackRate = 1.0f;
	int32 RestartSerial = 0;      // 加一 = 现在这一段从头放（再按一次互动）

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
