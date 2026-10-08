// 日落回廊 · 测试用的第一人称玩家：WASD 走、鼠标看、空格跳、Shift 跑，身上挂着 UDysisTimeComponent。
// 组员有自己的 Character 时，直接把 UDysisTimeComponent 加到那个 Character 上就行，这个类可以不用。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DysisCharacter.generated.h"

class UCameraComponent;
class UDysisTimeComponent;
class UDysisInteractComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class DYSIS_API ADysisCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	/** 构造期换装 UDysisCharacterMovement（光路携带/虚拟地面；只能构造期换，§8.1）。 */
	ADysisCharacter(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis")
	TObjectPtr<UDysisTimeComponent> Time;

	/** 摸一下交互（屏幕中心射线 → IDysisInteractable）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis")
	TObjectPtr<UDysisInteractComponent> Interact;

	UPROPERTY(EditAnywhere, Category = "Dysis")
	float WalkSpeed = 450.f;

	UPROPERTY(EditAnywhere, Category = "Dysis")
	float RunSpeed = 900.f;

	virtual void PostInitializeComponents() override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// ───── coyote（灰盒：刚踏空 0.14 s 里还能起跳；"还站得住"半边在移动组件/beam 侧，调研 §8.1）─────
	virtual void Falling() override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

	/** 离地后允许起跳的窗口（灰盒 0.14 s）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis")
	float CoyoteTime = 0.14f;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void RunOn();
	void RunOff();
	void TryInteractPressed();
	void JumpPressed();
	bool UiBlocksInput() const;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RunAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	/** coyote 窗口标志（离地起表、跳了即灭）。 */
	bool bCanCoyoteJump = false;
	FTimerHandle CoyoteTimerHandle;
};