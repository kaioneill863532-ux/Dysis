// 日落回廊 · 主角的动作：按人此刻的状态（站着 / 走 / 跑 / 上台阶 / 起跳 / 下落 / 落地 / 伸手）挑一段动作放，
// 换动作时用 0.16 秒过渡。没有动画蓝图：动作的播放和混合是这个类自己搭的（做法来自场景美术 2026-10-09 的工程）。
// 2026-10-10 加了三样（见 .cpp 里 FDysisHeroRoot 的说明）：
//   · 站着时脚不滑：把美术动作里被钉死的骨盆晃动还原回来，踩着地的那只脚就不动了；
//   · 走和跑按速度混在一起，两段的步子对齐；步子的快慢、大小跟着速度变，脚在地上不打滑；
//   · 跑的那一段整个人放低到脚着地（原来悬着九厘米多）。
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

	/** 现在放的是哪一段（A_Idle、A_Walk……；走和跑混着放时报占得多的那一段。测试和排查用）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Hero")
	FName GetActiveClipName() const;

	/** 步态现在的样子（JSON：各段的权重、步子的相位和快慢、步幅、骨盆挪了多少；测试和排查用）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Hero")
	FString DescribeGait();

	/** 两段动作本身的速度：脚踩在地上那一段往后蹬得有多快（厘米/秒，从动作里量出来的）。
	 *  游戏里走是 1.6 m/s、跑是 5.2 m/s：走的时候全是“走”那一段（步幅放大、步子放快来凑速度），
	 *  速度过了 WalkBlendFromSpeed 才开始混进“跑”，到“跑”本身的速度以上全是“跑”。
	 *  “跑”的 360 是在游戏里试出来的：按 300–380 各跑一遍，量踩地时脚掌在地上滑多快，360 时最小（走 3.1 m/s 时每秒约 20 cm）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float WalkClipSpeed = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float RunClipSpeed = 360.0f;

	/** 速度到这个数为止全是“走”那一段（比游戏里走的速度高一点，免得走着走着混进跑的样子）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float WalkBlendFromSpeed = 190.0f;

	/** 步幅放大以后腿要够得着，人得矮一点：步幅每放大三成，骨盆放低这么多厘米。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float StrideDropCm = 4.0f;

	/** 一个循环里两只脚各在什么时候落地（相位 0–1；走的那一段量出来右脚 0.3、左脚 0.8，跑的差不多）。脚步声按它响。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float FootfallPhase = 0.3f;

	/** 走、跑这两段动作各放了几个循环（一个循环 = 左右各迈一步）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float CyclesPerClip = 2.0f;

	/** “跑”那一段比“走”那一段早多少个循环（走的开头是左脚踩在正下方，跑的开头是右脚）：对齐以后两段才能混。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float RunPhaseLead = 0.5f;

	/** 步幅最少 / 最多放大到多少倍（再大腿就够不着了）。速度对不上的那部分用放快 / 放慢步子补。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float StrideMin = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float StrideMax = 1.3f;

	/** 人是一按键就到全速的；动作用的速度慢一拍跟上（每秒涨这么多厘米/秒），起步时先迈一小步再跑起来。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float GaitAccel = 1400.0f;

	/** 修不修（还原骨盆的晃动、放低、步幅）。关掉就是美术动作的原样，对比用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Hero")
	bool bFixLegs = true;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> Clips;

	// 下面几个由 NativeUpdateAnimation 在游戏线程上算好，每帧抄给动作那一边
	int32 ActiveClip = 0;
	float PlaybackRate = 1.0f;    // 走、跑以外的那几段放多快
	int32 RestartSerial = 0;      // 加一 = 现在这一段从头放（再按一次互动）
	float RunBlend = 0.0f;        // 走和跑怎么混：0 = 全是走，1 = 全是跑
	float CycleRate = 0.0f;       // 每秒迈几个循环
	float StrideScale = 1.0f;     // 步幅放大多少
	float GaitSpeed = 0.0f;       // 动作用的速度（慢一拍跟上真的速度）
	double GaitPhase = 0.0;       // 这一回起步以来迈了几个循环（和动作那边的相位一起走）
	int32 StepCount = 0;          // 脚落地了多少下（一直往上加）

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
