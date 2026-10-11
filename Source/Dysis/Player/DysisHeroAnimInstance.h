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
	 *  游戏里走是 1.95 m/s、跑是 5.2 m/s：走的时候全是“走”那一段（步幅放大、步子放快来凑速度），
	 *  速度过了 WalkBlendFromSpeed 才开始混进“跑”，到“跑”本身的速度以上全是“跑”。
	 *  “跑”的 360 是在游戏里试出来的：按 300–380 各跑一遍，量踩地时脚掌在地上滑多快，360 时最小（走 3.1 m/s 时每秒约 20 cm）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float WalkClipSpeed = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float RunClipSpeed = 360.0f;

	/** 速度到这个数为止全是“走”那一段（比游戏里走的速度高一点，免得走着走着混进跑的样子）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float WalkBlendFromSpeed = 235.0f;

	/** 走路的步子最快到多快：按“走这么快（厘米/秒）时的步频”封顶，再快的那部分全靠步子迈大。
	 *  2026-10-10 用户：走 190 时步子太碎，要“步频回到走 160 时的样子，每一步迈远一些”。
	 *  腿长 76.5 cm：这个步频下步幅放到 1.54 倍（一步 90 cm）就是极限了，再大要么脚在地上滑、要么人得蹲着走——试过走 195（步幅 1.54），
	 *  用户看了说步子迈得太大、很怪，宁可慢一点：走的速度回到 160（步幅 1.27，正好就是这个步频）。这个封顶留着：以后再调快也不会把步子调碎。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float WalkCadenceSpeed = 160.0f;

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
	float StrideMax = 1.6f;

	/** 人是一按键就到全速的；动作用的速度慢一拍跟上（每秒涨这么多厘米/秒），起步时先迈一小步再跑起来。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float GaitAccel = 1400.0f;

	/** 修不修（还原骨盆的晃动、放低、步幅）。关掉就是美术动作的原样，对比用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Hero")
	bool bFixLegs = true;

	/** 上半身修不修（2026-10-10 用户：走和上楼梯时上半身不动、上楼梯时手臂的姿势怪）。
	 *  量出来的：“走”那一段手前后只摆 26 cm（腿摆 57 cm，游戏里还把步幅放大了五成），所以显得上半身是僵的；
	 *  “上台阶”那一段两只手一直架在胸前、右手总在前面、和腿差着四分之一拍。
	 *  修法：① 手臂的摆幅跟着步幅放大（ArmSwingBase + ArmSwingGain ×（步幅 − 1）倍），肩膀跟着手拧一点（TorsoTwistDeg），头不跟着拧；
	 *        ② 上台阶时腿还用“上台阶”那一段，腰以上（spine_01 往上：身子、手臂、手指、头）换成“走”那一段的，时刻和腿对齐（UpstairsArmPhase），身子往前倾 UpstairsLeanDeg。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Hero")
	bool bFixUpperBody = true;

	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float ArmSwingBase = 1.15f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float ArmSwingGain = 1.6f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float TorsoTwistDeg = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float UpstairsLeanDeg = 8.0f;

	/** “上台阶”放到一圈的哪儿时，“走”那一段该在一个循环的哪儿（两段的腿对齐：从动作里量出来两段左右脚前后差的相位，上台阶 106°、走 −82°）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float UpstairsArmPhase = 0.478f;

	/** 上下台阶（或踩着斜的光）时的腿。美术“上台阶”那一段是按跑的速度做的，走着上楼时只能放慢了放——
	 *  2026-10-10 用户：上半身好了，下半身看着怪。现在不用那一段了：就用平地上走 / 跑的动作，脚按脚下的坡度落
	 *  （前脚落得高、后脚低；迈步时脚抬高到 StairLift 倍；步子收到 StairStride 倍），身子按坡度往前倾。bStairGait 关掉 = 回到用“上台阶”那一段。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Hero")
	bool bStairGait = true;

	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float StairLift = 1.9f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float StairStride = 0.85f;

	/** 走路时身子往前倾几度（2026-10-11 用户：人正常走路会稍微前倾一点，现在是竖直的，不自然；一点点就够，不要多）。
	 *  量出来的：美术“走”那一段里身子比站着还往后仰 1°。从腰（spine_01）往前倾，头只跟一半，脸还是朝前。
	 *  按速度给：走到 WalkCadenceSpeed 时倾满；只在“走”那一段上加（跑的那一段自己带着前倾）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float WalkLeanDeg = 4.0f;

	/** 推东西时身子往前压几度（2026-10-11 用户：推波吕丢刻斯那里做一个推的动作）。美术没给这一段动作，是现算的：
	 *  腿照常走；身子从腰往前压，两只手平着伸到胸前（胳膊留一点弯、肘朝下），手掌立起来顶在像上，头抬着看前面。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	float PushLeanDeg = 16.0f;

	/** 走路时两只脚的脚尖各往里收几度（X = 左脚，Y = 右脚）。量出来的：美术“走”那一段里脚踩着地时脚尖往外撇——左 15°、右 12°；
	 *  膝盖是朝正前弯的（我们修过），脚尖再撇这么多，看着就是外八字（2026-10-11 用户；从背后看更明显）。收到 6° 上下。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Hero")
	FVector2D FootTurnInDeg = FVector2D(9.5, 6.0);

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
	float GroundSlope = 0.0f;     // 脚下的坡度（升高 ÷ 往前走的距离；上台阶 0.4 上下，平地 0，下台阶是负的）
	float SlopeRise = 0.0f, SlopeRun = 0.0f;    // 最近零点几秒里升了多少、走了多远（越早的占得越少）
	float WalkLean = 0.0f;        // 这一刻该前倾几度（按速度）
	bool bPushing = false;        // 正推着东西走（波吕丢刻斯像）：两只手伸到胸前顶着、身子往前压
	float AirSeconds = 0.0f;      // 这一回腾空多久了
	bool bAirShown = false;       // 这一回腾空放过起跳 / 下落的动作了（放过才接“落地”）
	FVector SlopePrev = FVector::ZeroVector;
	bool bSlopePrev = false;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
