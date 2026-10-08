// 日落回廊 · 机关总管：把灰盒 v0.12 里各个机关的逻辑按原样搬过来（一个机关对应灰盒里的一个 update 函数），
// 关卡里的机关部件（模型里 SM_Mech_… 那些）由它来挪。GameMode 开局生成一个。
// 现在管着：
//   水闸和瀑布；L1 的机关（原来 A、B 两个，已经合成一个）和两块推拉石板          —— DysisDirector.cpp
//   屋顶：光圈叶片、16 级升起的踏步、夜里降下去的楼梯、细桥的桥门、接光（取下金苹果）  —— DysisDirectorRoof.cpp
// 别的机关做到哪搬到哪。
//
// 互动也在这里（灰盒 INTERACTS）：每个互动点有位置、够得着的水平距离、脚的高度范围、什么时候能用、提示文字、按下去做什么；
// 人走到旁边按 E，同时够得着几个时取最近的。提示文字和机关反馈按文案表（UI/DysisCopy.h）。
// 找部件靠 Tag：关卡里每个模型部件都带一个和自己名字一样的 Tag（Art/Models/temple-v0.12/ue_prepare_mechanisms.py 打的）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisDirector.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UBoxComponent;
class UDysisTimeComponent;
struct FDysisRoofPieceSpec;

/** 一个互动点（灰盒 INTERACTS 的一行）。 */
struct FDysisInteract
{
	FName Id;
	TFunction<FVector()> Pos;              // 只用水平位置（有的会动，所以是函数）
	TFunction<FVector2D()> ZRange;         // 脚的高度范围（厘米）
	float RadiusCm = 160.0f;               // 水平距离
	TFunction<bool()> When;                // 空 = 一直能用
	TFunction<FText()> Label;
	TFunction<void()> Act;
	TFunction<FVector()> Anchor;           // 提示（圆角方块里的 E 和旁边的字）浮在哪；空 = 互动点上方 1.1 m
};

UCLASS()
class DYSIS_API ADysisDirector : public AActor
{
	GENERATED_BODY()

public:
	ADysisDirector();

	static ADysisDirector* Get(const UObject* WorldContext);

	/** 关卡里叫这个名字的模型部件（没有就是空）。 */
	AActor* Piece(FName Label) const;

	// ───── 互动 ─────

	/** 脚站在这里时够得着的互动点里最近的一个（没有就是空）。 */
	const FDysisInteract* NearestInteract(const FVector& FootCm) const;

	/** 这个互动点的提示浮在世界里的哪一点。 */
	FVector InteractAnchor(const FDysisInteract& I) const;

	/** 按 E：做最近那个互动点的事。返回有没有做。 */
	bool Interact(const FVector& FootCm);

	// ───── 状态（灰盒里同名的量） ─────

	/** 两块推拉石板：0 = 开局（西边“上升的窗”关着、南边“虹的窗”开着），1 = 反过来。1.6 秒滑到位。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	float SliderPos = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	float SliderTarget = 0.0f;

	/** 机关拉过几次（文案：单数次、双数次的反馈不一样）。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	int32 LeverPulls = 0;

	/** 屋顶踏步升起来多少（0–1）：人上了环道顺时针往前走，前面 16 级跟着升成阶梯。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	float CrownUp = 0.0f;

	/** 入夜以后回到桥头，另外半圈降成楼梯（一直留着）。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bCrownDn = false;

	/** 细桥的桥门开了多少（0–1）；接住最后一缕光以后锁死。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	float DoorOpen = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bDoorLocked = false;

	/** 接住了最后一缕光（取下了金苹果）：从这一刻起是夜里。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bCaught = false;

	/** 测试用：按名字触发一个互动点（"sluice"、"lever"、"takeApple"……），不管人站在哪。返回有没有这个点、当时能不能用。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	bool DebugInteract(FName Id);

	/** 测试用：屋顶现在的样子（JSON）：踏步升了多少、各块的高度、光圈、桥门。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	FString DescribeRoof() const;

	/** 三相像的底座转到第几格（6 格：0 月之龛、1 夜里照女神、2 白天照日之龛……）。按一次绞盘转一格。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	int32 MirrorSlot = 2;

	/** 日之龛打开了没有。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bSunNicheOpen = false;

	/** 测试用：三相像现在的样子（JSON）：朝向、仰角、是哪一相、镜面中心和法线。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	FString DescribeMirror() const;

	/** 测试用：直接把三相像转到第几格。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	void DebugSetMirrorSlot(int32 Slot);

	/** 测试用：直接入夜 / 回到白天（不用真的去屋顶接光）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	void DebugSetNight(bool bNight);

	// ───── 虹那条支线（伊莉丝浮雕 → 虹桥 → 窗台石沿和虹之龛 → 棱镜 → 塞勒涅）—— DysisDirectorRainbow.cpp ─────

	/** 伊莉丝浮雕解开了：影子的头落进了人形，虹醒过来。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bReliefDone = false;

	/** 虹桥现在能不能踩（长到头以后能踩；入夜以后整座消失）。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bBridgeOn = false;

	/** 南窗下的石沿和虹之龛从墙里伸出来多少（0–1）。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	float SillK = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bIrisNicheOpen = false;

	/** 棱镜的铜柱从窗台里升起来多少（0–1）。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	float PrismExt = 0.0f;

	/** 棱镜转到第几格（8 格：第 0 格哪一色都不对，第 1–7 格依次让红…紫落在塞勒涅的眼睛上）。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	int32 PrismSlot = 0;

	/** 靛色的光落进了塞勒涅的眼睛（她醒了）。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bSeleneOn = false;

	/** 测试用：这条支线现在的样子（JSON）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	FString DescribeRainbow() const;

	/** 测试用：棱镜在第 Slot 格、时刻 H 时，七色各落在哪（JSON：七个点，照不到的是 null；near = 哪一色落在塞勒涅眼睛上）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	FString DebugPrismHits(int32 Slot, float H) const;

	/** 测试用：把“影子对上了多少”清零（一个个位置试的时候用，免得真的解开）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	void DebugResetRelief();

	// ───── 夜里第一段：天鹅、墙里的两段楼梯、东南墙的月亮浮雕 —— DysisDirectorNight.cpp ─────

	/** 女神像（月光下是黑天鹅）的底座转到第几格（8 格，第 0 格是对的）。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	int32 SwanSlot = 3;

	/** 黑天鹅头的影子落进了浮雕的空白：浮雕连同后面的墙沉下去，露出往下的楼梯。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bSwanSolved = false;

	/** 测试用：夜里这一段现在的样子（JSON）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	FString DescribeNight() const;

	/** 测试用：直接把女神像摆到第几格、黑天鹅显出来多少（0–1）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	void DebugSetSwan(int32 Slot, float Form);

	/** 测试用：某块月石的每个取样点照没照到（"lit,box,blk,…"；box = 落在镜子照亮的那一块外面，blk = 被挡住）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	FString DebugMoonstoneLit(FName Id) const;

	// ───── 夜里第二段：月之龛、双子、月桥 —— DysisDirectorTwins.cpp ─────

	/** 双子并肩站在月光里了：月桥出现。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bTwinsJoined = false;

	/** 拿到了月亮碎片。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bMoonShard = false;

	/** 测试用：双子和月桥现在的样子（JSON）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	FString DescribeTwins() const;

	/** 测试用：站在这里的一尊像（脚的位置，厘米）被月亮直射照着多少（腰、胸、头三个点）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	float DebugStatueMoonLit(FVector FootCm) const;

	// ───── 夜里第三段：瀑布后面的女神、半桥、影桥、放苹果和结局 —— DysisDirectorFinale.cpp ─────

	/** 月光整个落进了女神怀里的月亮：她亮起来，水池东北边伸出半桥。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bGoddessLit = false;

	/** 金苹果放上了水亭浑天仪的月托。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bApplePlaced = false;

	/** 测试用：这一段现在的样子（JSON）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	FString DescribeFinale() const;

	/** 测试用：女神身上的取样点照没照到（"lit,box,…|lit,box,…"：前五个是怀里的月亮，后五个是全身）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	FString DebugGoddessLit() const;

	/** 测试用：时刻 H 时屋顶细桥落在水面上的影子的四个角（JSON；月亮太低就是 null）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	FString DebugBridgeShadow(float H) const;

	/** 测试用：月桥直接出现（不用先解双子）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	void DebugShowMoonBridge();

	virtual void Tick(float DeltaTime) override;

	// ───── 夜里几样“看的”东西：金苹果的光、瀑布上的月虹、结局的星座 —— DysisDirectorSky.cpp ─────

	/** 伊莉丝和塞勒涅的那段话说完了：之后夜里月亮升到东边不高不低的地方，瀑布前面会有一道月虹。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	bool bSeleneTalked = false;

	/** 月虹现在显出来多少（0–1）。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	float MoonbowK = 0.0f;

	/** 测试用：直接算作那段话说完了（或者没说过）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	void DebugSetSeleneTalked(bool bTalked);

	/** 测试用：直接放结局（bAllShards = 三片碎片都集齐了的那一个，有星座）。人要先站在水亭上。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	void DebugPlayEnding(bool bAllShards);

	/** 测试用：这几样现在的样子（JSON）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	FString DescribeSkyFx() const;

	/** 测试用：脚站在这里时旁边会浮出哪个互动（它的名字；没有就是空）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	FString DebugPromptAt(FVector FootCm) const;

protected:
	virtual void BeginPlay() override;

private:
	void CollectPieces();
	void BuildInteracts();
	/** 玩家身上的时间组件（区域、时刻、脚的位置都从它读）。 */
	UDysisTimeComponent* PlayerTime() const;
	bool GameStarted() const;

	// 水闸、瀑布
	void ToggleSluice();
	void UpdateWaterfall();
	// 机关和推拉石板（灰盒 pullLever / updateSlider / placeSlider）
	void PullLever();
	void UpdateSlider(float Dt);
	void PlaceSlider();

	// 屋顶（灰盒 buildCrown / updateCrown / updateIris / catchLight / updateCatch）
	struct FRoofWall { TObjectPtr<UBoxComponent> Box; float AzMid = 0, R0 = 0, R1 = 0, HalfDeg = 0; };
	struct FRoofPiece
	{
		const FDysisRoofPieceSpec* Spec = nullptr;
		float Z = 0.0f;                              // 踏面现在的高度（厘米）
		TWeakObjectPtr<AActor> Tread, Shaft, Blade;
		FVector TreadBase = FVector::ZeroVector;     // 关卡里摆的位置（升起来的踏步摆的是升到头的样子）
		TWeakObjectPtr<AActor> Curb;                 // 踏步内沿的一道铜边：跟着踏步走，只挡镜头
		FVector CurbBase = FVector::ZeroVector;
		float BladeYaw = 0.0f;
		TWeakObjectPtr<UStaticMeshComponent> BladeLook;   // 叶片只管看的那一份（形状不动，用材质剪出光圈）
		float BladeLookDz = 0.0f;
		TArray<FRoofWall> Walls;                     // 看不见的墙（跟着踏面走），人不会掉进光圈、也走不出屋顶
	};
	void SetupRoof();
	UBoxComponent* MakeWall();
	void AddArcWall(FRoofPiece& P, float A0, float A1, float R0, float R1);
	void PlaceWall(const FRoofWall& W, float ZBottom) const;
	void PlaceRoofPiece(FRoofPiece& P);
	void PlaceBlades();
	void UpdateCrown(float Dt);
	void UpdateIris(float Dt);
	void CatchLight();
	void UpdateCatch(float Dt);
	FVector ArmTopCm() const;

	// 三相像和日之龛（灰盒 mirrorStatue / placeMirror / updateMirrors / updateNiches）—— DysisDirectorMirror.cpp
	void SetupMirror();
	void AddMirrorInteracts();
	void PlaceMirror();
	void UpdateMirrors(float Dt);
	void UpdateNiches(float Dt);
	/** 从 Start 朝 Dir 看过去有没有被挡住（三相像自己、人、光的踩踏板不算）。 */
	bool MirrorSeesLight(const FVector& Start, const FVector& Dir) const;
	TWeakObjectPtr<AActor> MirrorStatue, MirrorPlate;
	float MirrorStatueYaw0 = 0.0f;
	FQuat MirrorPlateBase = FQuat::Identity;
	float MirrorYawNow = 153.191f;
	float MirrorW3[3] = { 1.0f, 0.0f, 0.0f };   // 暗、日、月
	int32 MirrorForm = 0;
	TWeakObjectPtr<AActor> SunLid, SunShard;
	FQuat SunLidBase = FQuat::Identity;
	FVector SunShardBase = FVector::ZeroVector;
	float SunNicheT = 0.0f;

	// 虹那条支线（灰盒 IRISREL / RB / BRIDGE / SILL / PRISM / SELENE）—— DysisDirectorRainbow.cpp
	void SetupRainbow();
	void AddRainbowInteracts();
	void UpdateIrisRelief(float Dt);
	void UpdateRainbow(float Dt);
	void PlaceSill(float K);
	void SetBridgeOn(bool bOn);
	void PlacePrism();
	/** 从 Start 朝 Dir 看过去有没有被挡住（人、光自己不算）。 */
	bool RainbowSeesLight(const FVector& Start, const FVector& Dir) const;
	/** 一道色光从棱镜出去落到哪里。 */
	bool SpectrumHit(const FVector& OutDir, FVector& OutPos, FVector& OutNormal) const;
	UStaticMeshComponent* MakeGlow(UStaticMesh* Mesh, const FLinearColor& Color, float Intensity);

	TWeakObjectPtr<class ADysisBeamActor> IrisBeam;
	float ReliefAlign = 0.0f, ReliefMissCm = 900.0f;
	float RainbowT = 0.0f;                   // 浮雕解开以后过了多久（灰盒 RB.active）
	float BridgeK = 0.0f;
	TWeakObjectPtr<AActor> BridgeActor;
	TArray<TWeakObjectPtr<UBoxComponent>> BridgeFloor;   // 能踩的面：一段段薄板
	TArray<TWeakObjectPtr<UBoxComponent>> BridgeRails;
	struct FSillPart { TWeakObjectPtr<AActor> Actor; FVector Base = FVector::ZeroVector; float Yaw0 = 0.0f; bool bSolid = false; };
	TArray<FSillPart> SillParts;             // 石沿、龛的背板、两扇门、碎片
	FVector SillOffset = FVector::ZeroVector;
	float IrisNicheT = 0.0f;
	struct FPrismPart { TWeakObjectPtr<AActor> Actor; FVector Base = FVector::ZeroVector; float Yaw0 = 0.0f; };
	TArray<FPrismPart> PrismParts;           // 铜柱、棱镜、铜缝、转盘
	TWeakObjectPtr<UBoxComponent> PrismBlock;
	float PrismYawDeg = 0.0f, PrismWheelDeg = 0.0f;
	FVector PrismHitPos[7];
	bool bPrismHit[7] = { false, false, false, false, false, false, false };
	int32 SeleneColor = -1;
	float SeleneLit = 0.0f, SeleneTalkIn = -1.0f;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> BridgeMID;
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> BandsMID;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BandsPlane;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PrismRays;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PrismSpots;
	UPROPERTY()
	TObjectPtr<class UDysisDialogueComponent> SeleneDialogue;

	// 夜里第一段（灰盒 SWAN / TUNWIN / SEALS / MOONSTONES / MREL）—— DysisDirectorNight.cpp
	/** 月石：被月光（直射，或经三相像的镜子反射）照着时显形 / 隐去，光离开后慢慢变回去。 */
	struct FMoonstone
	{
		FName Id;
		TArray<FVector> Samples;                    // 取样点：照到的比例 ≥ Need 才算照着
		int32 Source = 0;                           // 0 不看光、1 月亮直射、2 镜子反射的月光
		float Need = 0.999f;
		bool bOneShot = false, bPerm = false, bDormant = false, bLit = false;
		float K = 0.0f, LitFrac = 0.0f;
		bool bHasDoor = false;                      // 人站在门洞里时不会合上
		float DoorAz = 0.0f, DoorHalfDeg = 3.0f, DoorZ = 0.0f;
		TArray<TWeakObjectPtr<AActor>> Parts;       // 显形以后才有的（能踩）
		TArray<TWeakObjectPtr<AActor>> Blocks;      // 显形以后就没了的（平时挡着）
		const TCHAR* PermCopy = nullptr;            // 头一次照到时的提示
	};
	void SetupNight();
	void AddNightInteracts();
	void UpdateSwan(float Dt);
	void UpdateStairs(float Dt);
	void UpdateMoonstones(float Dt);
	void SetMoonstoneState(FMoonstone& Ms, float K);
	bool MoonSampleLit(const FMoonstone& Ms, const FVector& P, const TCHAR** OutWhy = nullptr) const;
	bool PlayerOnMoonstone(const FMoonstone& Ms) const;
	FMoonstone* FindMoonstone(FName Id);
	FVector SwanHeadCm() const;
	TArray<FMoonstone> Moonstones;
	float MoonstoneClock = 0.0f;
	mutable TWeakObjectPtr<class ADysisBeamActor> MoonMirrorBeam;
	TWeakObjectPtr<AActor> SwanGoddess, SwanBird, SwanRelief, SwanDoor, MoonDisk;
	float SwanYaw0 = 0.0f, SwanYawNow = 0.0f, SwanLit = 0.0f, SwanForm = 0.0f, SwanMissCm = 900.0f, SwanSolvedT = 0.0f, SwanOpenT = 0.0f;
	FVector SwanReliefBase = FVector::ZeroVector, SwanDoorBase = FVector::ZeroVector;
	struct FStairWindow { TWeakObjectPtr<AActor> Actor; FVector Base = FVector::ZeroVector; float Open = 0.0f; bool bGone = false; };
	struct FStairSet { TArray<FStairWindow> Windows; float T = -1.0f; TWeakObjectPtr<AActor> Seal; FVector SealBase = FVector::ZeroVector; float SealOpen = 0.0f; };
	FStairSet StairSets[2];                      // 0 = TS（天鹅后面，三层 → 二层），1 = TR（月亮浮雕后面，二层 → 一层）

	// 夜里第二段（灰盒 MSHRINE / TWN / MOONBRIDGE）—— DysisDirectorTwins.cpp
	void SetupTwins();
	void AddTwinsInteracts();
	void UpdateTwins(float Dt);
	float StatueMoonLit(const FVector& FootCm) const;
	int32 TwinState = 0;                         // 0 藏在墙里、1 墙透开了（在龛里）、2 正在拉出来、3 出来了（可以推）、4 并肩了
	float TwinT = 0.0f, PolluxAz = 0.0f, PolluxLit = 0.0f, CastorLit = 0.0f, TwinLitClock = 0.0f;
	bool bShrineTold = false;
	TWeakObjectPtr<AActor> Pollux, Castor, TwinChain, MoonDeck, MoonShrineShard, MoonShrineNiche;
	float PolluxYaw0 = 0.0f;
	TWeakObjectPtr<UBoxComponent> PolluxBlock, ChainBlock;
	TArray<TWeakObjectPtr<UBoxComponent>> MoonRails;

	// 夜里第三段（灰盒 GOD / SHADOWBR / placeApple）—— DysisDirectorFinale.cpp
	void SetupFinale();
	void AddFinaleInteracts();
	void UpdateGoddess(float Dt);
	void UpdateShadowBridge(float Dt);
	void UpdateFinale(float Dt);
	bool RoofBridgeShadow(float H, FVector OutQuad[4]) const;
	float GoddessLit = 0.0f, GoddessSweep = 0.0f, GoddessDwell = 0.0f, HalfBridgeExt = 0.0f, ShadowOn = 0.0f, FinaleT = 0.0f;
	bool bGoddessGlint = false, bGoddessHinted = false, bShadowTold = false, bEndingStarted = false, bEndingDone = false;
	TWeakObjectPtr<AActor> HalfDeck;
	float BridgeAwayT = 0.0f;                       // 结局时屋顶细桥退走的进度（秒）
	TArray<TWeakObjectPtr<AActor>> BridgeAway;
	TArray<FVector> BridgeAwayBase;
	TWeakObjectPtr<UBoxComponent> HalfFloor;
	TArray<TWeakObjectPtr<UBoxComponent>> HalfRails;
	TWeakObjectPtr<class ADysisInvisibleWall> ShadowFloor;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ShadowPlane;
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> ShadowMID;
	UPROPERTY()
	TObjectPtr<class UDysisDialogueComponent> EndingDialogue;

	// “查看××”：文案表“查看显示”那一组（走近了按 E 看一句描述）—— DysisDirectorViews.cpp
	void AddViewInteracts();
	void UpdateViews();
	bool bTopArmillaryTold = false;              // 殿顶浑天仪那句“金苹果”的描述说过了
	bool bMenuPose = false;                      // 主界面开着：屋顶的踏步摆成升到头的样子
	/** 入夜以后狄西斯自己说的那一句，用剧情对话框放。 */
	UPROPERTY()
	TObjectPtr<class UDysisDialogueComponent> NightDialogue;

	// 金苹果的光、月虹、结局的星座 —— DysisDirectorSky.cpp
	void SetupSkyFx();
	void UpdateAppleLight(float Dt);
	void UpdateMoonbow(float Dt);
	void StartStarShow();
	void UpdateEndingStars(float Dt);
	bool StarShowDone() const;
	struct FConStar { FVector Dir = FVector::UpVector; float Gain = 1.0f, At = 0.0f, PulseAt = 1.0e9f; };
	struct FConLine { int32 A = 0, B = 0; float At = 0.0f; };
	TArray<FConStar> ConStarInfo;
	TArray<FConLine> ConLineInfo;
	float StarShowT = -1.0f, StarShowEnd = 0.0f;   // 星座的动画放到第几秒（< 0 = 没开始）
	bool bSeleneTalkStarted = false, bEndingAll = false;
	FLinearColor AppleLightColor = FLinearColor::White;
	UPROPERTY()
	TObjectPtr<class UPointLightComponent> AppleLight;
	/** 月之龛里碎片的一点微光（灰盒里碎片自己发光）：龛透开了、碎片还在的时候亮着，夜里远远就能看见龛在哪。 */
	UPROPERTY()
	TObjectPtr<class UPointLightComponent> ShrineGlow;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MoonbowPlane;
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> MoonbowMID;
	UPROPERTY()
	TObjectPtr<class UInstancedStaticMeshComponent> ConStars;
	UPROPERTY()
	TObjectPtr<class UInstancedStaticMeshComponent> ConLines;
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> ConStarMID;
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> ConLineMID;
	UPROPERTY()
	TObjectPtr<class ACameraActor> EndingCamera;

	/** 光圈叶片“只管看的那一份”用的材质（每种原来的材质一个），参数 Aperture = 现在的光圈半径。 */
	UPROPERTY()
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> IrisMIDs;

	/** 机关的音效 —— DysisDirectorSfx.cpp：每帧看一遍各个机关的状态，哪个变了就在它那里放对应的一次性音效
	 *  （音效表在 Audio/DysisSfxDefaults.cpp；脚步、环境声、界面声还是 ADysisSfxDirector 管）。 */
	void UpdateSfx();
	bool SfxRose(FName Key, float Now, float Threshold);
	bool SfxFell(FName Key, float Now, float Threshold);
	bool SfxChanged(FName Key, float Now);
	void Sfx(const TCHAR* Event, const FVector& At) const;
	FVector PieceLoc(FName Label, const FVector& Fallback) const;
	TMap<FName, float> SfxPrev;

	/** 关卡标题（灰盒 LEVELS / updateLevel）：头一次走进某一段，屏幕中间出一次标题。 */
	void UpdateLevelTitle();
	int32 SeenLevel = -1;

	TMap<FName, TWeakObjectPtr<AActor>> Pieces;
	TArray<FDysisInteract> Interacts;

	struct FSliderPanel { TWeakObjectPtr<AActor> Actor; float Az = 0.0f, Shift = 0.0f, Z = 0.0f; bool bOpenAt = false; float Yaw0 = 0.0f, Az0 = 0.0f; };
	TArray<FSliderPanel> Panels;
	TWeakObjectPtr<AActor> LeverArm;
	FQuat LeverArmBase = FQuat::Identity;
	FVector LeverAxis = FVector::RightVector;

	TArray<FRoofPiece> RoofPieces;
	int32 TopPieceIndex = -1;                // 最高的那一级（第 16 级）
	TWeakObjectPtr<AActor> TopBar, DoorLeaf;
	FVector TopBarBase = FVector::ZeroVector;
	float DoorClosedYaw = 0.0f;
	FRoofWall TopBarWall;
	float IrisShownCm = -1.0f;               // 叶片现在摆的是多大的光圈
	float CineSeconds = 0.0f;                // 接住光以后过了多久

	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> WallComponents;

	/** 水闸那里现在没有模型，先立一根小柱子标出位置。 */
	UPROPERTY(VisibleAnywhere, Category = "Dysis|Director")
	TObjectPtr<UStaticMeshComponent> SluiceMarker;

	/** 金苹果（先用一个小球）：白天挂在屋顶浑天仪上，取下以后跟在人身边。 */
	UPROPERTY(VisibleAnywhere, Category = "Dysis|Director")
	TObjectPtr<UStaticMeshComponent> Apple;
};
