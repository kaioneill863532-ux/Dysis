// 日落回廊 · 玩家。数值照灰盒 v0.12（index.html 的 updatePlayer / updateCamera）：
//   走 3.1 m/s、跑 5.2 m/s（Shift）；起跳 7.6 m/s、重力 22 m/s²、最快下落 30 m/s；身体半径 0.3 m、迈得上 0.55 m 的台阶；
//   默认第三人称（镜头离人 4.6 m，滚轮 2.2–9 m，偏右肩 0.55 m），V 切第一人称（眼高 1.62 m）；
//   进了墙里的窄楼梯，镜头收到 2.6 m 以内、不再偏肩。空格跳，E 互动，R 回上一个落脚点。
// 人物外形先用一个圆锥顶一个球代替（等正式模型）。第一人称时看不见自己，但影子还在（伊莉丝浮雕要用影子）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DysisCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UDysisTimeComponent;
class UDysisInteractComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EDysisViewMode : uint8
{
	ThirdPerson,
	FirstPerson,
};

UCLASS()
class DYSIS_API ADysisCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	/** 构造期换装 UDysisCharacterMovement（只能构造期换）。 */
	ADysisCharacter(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis")
	TObjectPtr<UCameraComponent> Camera;

	/** 临时外形：圆锥身子、球脑袋。正式模型来了换掉这两个。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis")
	TObjectPtr<UStaticMeshComponent> AvatarBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis")
	TObjectPtr<UStaticMeshComponent> AvatarHead;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis")
	TObjectPtr<UDysisTimeComponent> Time;

	/** E 键互动：离得够近的机关里最近的那个。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis")
	TObjectPtr<UDysisInteractComponent> Interact;

	// ───── 移动（灰盒数值，厘米 / 秒） ─────
	UPROPERTY(EditAnywhere, Category = "Dysis|Move")
	float WalkSpeed = 310.f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Move")
	float RunSpeed = 520.f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Move")
	float JumpSpeed = 760.f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Move")
	float GravityCm = 2200.f;

	/** 刚踏空的这一小会儿里还能起跳（灰盒 0.14 s）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Move")
	float CoyoteTime = 0.14f;

	// ───── 镜头 ─────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Camera")
	EDysisViewMode ViewMode = EDysisViewMode::ThirdPerson;

	/** 第三人称镜头离人多远（厘米）；滚轮在 Min–Max 之间调。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Camera")
	float CameraDistance = 460.f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Camera")
	float CameraDistanceMin = 220.f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Camera")
	float CameraDistanceMax = 900.f;

	/** 滚轮每格（厘米）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Camera")
	float CameraWheelStep = 40.f;

	/** 镜头瞄点往右肩偏多少、离脚多高（厘米）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Camera")
	float CameraShoulder = 55.f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Camera")
	float CameraTargetHeight = 150.f;

	/** 第一人称的眼高（厘米）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Camera")
	float EyeHeight = 162.f;

	/** 在墙里的窄楼梯上镜头最远多远（厘米）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Camera")
	float CameraTunnelDistance = 260.f;

	/** 入夜以后镜头用固定曝光（不然自动曝光会把月夜提亮得和白天一样）。数越大越亮，加 1 亮一倍。
	 *  −1.0 是拿灰盒同样位置、同样时刻的画面对出来的：月光照不到的墙、夜空、月光照到的地面都和灰盒差不多亮。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Camera")
	float NightExposureBias = -1.0f;

	/** 刚入夜、天还没黑透的那一阵曝光再加多少（太阳落到地平线下 9° 以后就不加了）。负数 = 暗一些：
	 *  灰盒里曝光是从白天的 0.95 慢慢升到夜里的 1.9 的，刚入夜时比深夜低 0.8 档；那一阵环境光本身亮得多（ADysisSkyActor::DayFillLux）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Camera")
	float NightTwilightBoost = -0.8f;

	/** 开局镜头的俯仰（度，负 = 往下看；灰盒 0.22 弧度）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Camera")
	float StartPitch = -12.6f;

	UFUNCTION(BlueprintCallable, Category = "Dysis")
	void SetViewMode(EDysisViewMode Mode);

	/** 脚底的位置（厘米）。 */
	FVector GetFootLocation() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	/** 过剧情时人定在原地：任何“往哪边走”的输入都不收。 */
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;

	/** 正在过剧情（对话框开着）：这时候人和镜头都定住，不能走、跳、互动、转视角，只能单击鼠标翻页。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Player")
	bool InDialogue() const;

	/** 单击鼠标左键：剧情对话翻到下一句（测试里也直接叫它）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Player")
	void AdvancePressed();
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// ───── 刚踏空还能跳（coyote） ─────
	virtual void Falling() override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value);
	void RunOn();
	void RunOff();
	void TryInteractPressed();
	void JumpPressed();
	void ToggleViewPressed();
	void RespawnPressed();
	bool UiBlocksInput() const;
	void ApplyMovementNumbers();
	void UpdateCamera(float Dt);
	void SetAvatarShown(bool bShown);

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ZoomAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RunAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ViewAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RespawnAction;

	/** 单击鼠标左键：剧情对话翻到下一句。 */
	UPROPERTY()
	TObjectPtr<UInputAction> AdvanceAction;

	bool bRunning = false;
	bool bDialogueLookLocked = false;   // 过剧情时让控制器不收“转镜头”的输入（开始 / 结束各叫一次，成对）
	bool bStartPitchApplied = false;
	bool bAvatarShown = true;
	float CamTun = 0.0f;   // 0 = 在开阔处，1 = 在墙里的楼梯上（镜头收紧）

	/** coyote 窗口标志（离地起表、跳了即灭）。 */
	bool bCanCoyoteJump = false;
	FTimerHandle CoyoteTimerHandle;
};
