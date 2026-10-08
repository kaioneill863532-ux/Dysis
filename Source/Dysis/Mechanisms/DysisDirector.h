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

	virtual void Tick(float DeltaTime) override;

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
		float BladeYaw = 0.0f;
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
