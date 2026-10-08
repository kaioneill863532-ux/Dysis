#include "DysisCharacter.h"
#include "DysisCharacterMovement.h"
#include "DysisGreybox.h"
#include "Sky/DysisTimeComponent.h"
#include "Interaction/DysisInteractComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"
#include "UI/DysisHUD.h"
#include "UI/DysisDialogueComponent.h"

namespace
{
	constexpr float CapsuleRadius = 30.f, CapsuleHalfHeight = 90.f;   // 灰盒 BODY_R 0.3 m；身高 1.8 m
}

ADysisCharacter::ADysisCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDysisCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	// 人朝着走的方向转；镜头跟着鼠标（灰盒 heading / yaw 分开）
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(GetCapsuleComponent());
	CameraArm->bUsePawnControlRotation = true;
	CameraArm->bDoCollisionTest = true;      // 碰到墙、台阶、顶就往前收，镜头不穿进墙里
	CameraArm->ProbeSize = 12.f;
	CameraArm->TargetArmLength = CameraDistance;
	CameraArm->bEnableCameraLag = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	// 临时外形（灰盒的袍子：底半径 0.33 m、高 1.43 m，头在 1.56 m）
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	auto MakePart = [this](FName Name, UStaticMesh* InMesh, const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(GetCapsuleComponent());
		Part->SetStaticMesh(InMesh);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->CastShadow = true;
		Part->bCastHiddenShadow = true;   // 第一人称时人藏起来，影子留着
		return Part;
	};
	AvatarBody = MakePart(TEXT("AvatarBody"), Cone.Succeeded() ? Cone.Object : nullptr, FVector(0.f, 0.f, -CapsuleHalfHeight + 71.5f), FVector(0.66f, 0.66f, 1.43f));
	AvatarHead = MakePart(TEXT("AvatarHead"), Sphere.Succeeded() ? Sphere.Object : nullptr, FVector(0.f, 0.f, -CapsuleHalfHeight + 156.f), FVector(0.25f));

	Time = CreateDefaultSubobject<UDysisTimeComponent>(TEXT("DysisTime"));
	Interact = CreateDefaultSubobject<UDysisInteractComponent>(TEXT("DysisInteract"));

	ApplyMovementNumbers();
}

void ADysisCharacter::ApplyMovementNumbers()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move) return;
	Move->MaxWalkSpeed = bRunning ? RunSpeed : WalkSpeed;
	Move->JumpZVelocity = JumpSpeed;
	Move->GravityScale = GravityCm / 980.f;
	// 灰盒里速度是“按键即到”，空中也一样听方向键：加减速给得很大，空中控制给满
	Move->MaxAcceleration = 20000.f;
	Move->BrakingDecelerationWalking = 20000.f;
	Move->BrakingDecelerationFalling = 20000.f;
	Move->GroundFriction = 8.f;
	Move->AirControl = 1.f;
	Move->MaxStepHeight = 55.f;              // 灰盒 STEP 0.55 m
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 600.f, 0.f);
	JumpMaxHoldTime = 0.f;
}

FVector ADysisCharacter::GetFootLocation() const
{
	return GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void ADysisCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyMovementNumbers();   // 属性可能在编辑器里改过
	SetViewMode(ViewMode);
}

void ADysisCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 输入在运行时建好，不依赖内容资源
	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(this, Name);
		A->ValueType = Type;
		return A;
	};
	MoveAction = MakeAction(TEXT("IA_DysisMove"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_DysisLook"), EInputActionValueType::Axis2D);
	ZoomAction = MakeAction(TEXT("IA_DysisZoom"), EInputActionValueType::Axis1D);
	JumpAction = MakeAction(TEXT("IA_DysisJump"), EInputActionValueType::Boolean);
	RunAction = MakeAction(TEXT("IA_DysisRun"), EInputActionValueType::Boolean);
	InteractAction = MakeAction(TEXT("IA_DysisInteract"), EInputActionValueType::Boolean);
	ViewAction = MakeAction(TEXT("IA_DysisView"), EInputActionValueType::Boolean);
	RespawnAction = MakeAction(TEXT("IA_DysisRespawn"), EInputActionValueType::Boolean);

	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Dysis"));
	auto Swizzle = [this]() { return NewObject<UInputModifierSwizzleAxis>(this); };   // 默认 YXZ：键值放到 Y（前后）
	auto Negate = [this]() { return NewObject<UInputModifierNegate>(this); };
	for (const FKey& Key : { EKeys::W, EKeys::Up }) Mapping->MapKey(MoveAction, Key).Modifiers.Add(Swizzle());
	for (const FKey& Key : { EKeys::S, EKeys::Down }) { FEnhancedActionKeyMapping& M = Mapping->MapKey(MoveAction, Key); M.Modifiers.Add(Swizzle()); M.Modifiers.Add(Negate()); }
	for (const FKey& Key : { EKeys::A, EKeys::Left }) Mapping->MapKey(MoveAction, Key).Modifiers.Add(Negate());
	for (const FKey& Key : { EKeys::D, EKeys::Right }) Mapping->MapKey(MoveAction, Key);
	Mapping->MapKey(LookAction, EKeys::Mouse2D);
	Mapping->MapKey(ZoomAction, EKeys::MouseWheelAxis);
	Mapping->MapKey(JumpAction, EKeys::SpaceBar);
	Mapping->MapKey(RunAction, EKeys::LeftShift);
	Mapping->MapKey(RunAction, EKeys::RightShift);
	Mapping->MapKey(InteractAction, EKeys::E);
	Mapping->MapKey(ViewAction, EKeys::V);
	Mapping->MapKey(RespawnAction, EKeys::R);
}

void ADysisCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			Sub->AddMappingContext(Mapping, 0);
		// 俯仰范围（灰盒 pitch −1.2 … 1.25 弧度，正 = 往下看）；开局微微俯视
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = -71.6f;
			PC->PlayerCameraManager->ViewPitchMax = 68.75f;
		}
		FRotator Control = PC->GetControlRotation();
		Control.Pitch = StartPitch;
		PC->SetControlRotation(Control);
	}
}

void ADysisCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* In = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		In->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADysisCharacter::Move);
		In->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADysisCharacter::Look);
		In->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ADysisCharacter::Zoom);
		In->BindAction(JumpAction, ETriggerEvent::Started, this, &ADysisCharacter::JumpPressed);
		In->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		In->BindAction(RunAction, ETriggerEvent::Started, this, &ADysisCharacter::RunOn);
		In->BindAction(RunAction, ETriggerEvent::Completed, this, &ADysisCharacter::RunOff);
		In->BindAction(InteractAction, ETriggerEvent::Started, this, &ADysisCharacter::TryInteractPressed);
		In->BindAction(ViewAction, ETriggerEvent::Started, this, &ADysisCharacter::ToggleViewPressed);
		In->BindAction(RespawnAction, ETriggerEvent::Started, this, &ADysisCharacter::RespawnPressed);
	}
}

bool ADysisCharacter::UiBlocksInput() const
{
	// 主界面、设置页开着的时候，回车 / 空格 / E 是给界面用的，不是跳和互动
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		if (const ADysisHUD* H = Cast<ADysisHUD>(PC->GetHUD()))
			return H->IsMenuOpen() || H->IsSettingsOpen();
	return false;
}

void ADysisCharacter::JumpPressed()
{
	if (!UiBlocksInput()) Jump();
}

void ADysisCharacter::TryInteractPressed()
{
	if (UiBlocksInput()) return;
	// 对白播放中，E 键先当“下一句”
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (ADysisHUD* H = Cast<ADysisHUD>(PC->GetHUD()))
			if (H->ActiveDialogue && H->ActiveDialogue->IsPlaying())
			{ H->ActiveDialogue->Advance(); return; }
	if (Interact) Interact->TryInteract();
}

void ADysisCharacter::ToggleViewPressed()
{
	if (UiBlocksInput()) return;
	SetViewMode(ViewMode == EDysisViewMode::FirstPerson ? EDysisViewMode::ThirdPerson : EDysisViewMode::FirstPerson);
}

void ADysisCharacter::RespawnPressed()
{
	if (UiBlocksInput()) return;
	if (UDysisCharacterMovement* Move = Cast<UDysisCharacterMovement>(GetCharacterMovement())) Move->RespawnNow();
}

void ADysisCharacter::Move(const FInputActionValue& Value)
{
	FVector2D V = Value.Get<FVector2D>();
	if (V.SizeSquared() > 1.0) V.Normalize();   // 斜着走不比直着走快
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), V.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), V.X);
}

void ADysisCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	AddControllerYawInput(V.X);
	AddControllerPitchInput(-V.Y);
}

void ADysisCharacter::Zoom(const FInputActionValue& Value)
{
	if (UiBlocksInput()) return;
	// 滚轮往前 = 拉近
	CameraDistance = FMath::Clamp(CameraDistance - Value.Get<float>() * CameraWheelStep, CameraDistanceMin, CameraDistanceMax);
}

void ADysisCharacter::RunOn() { bRunning = true; GetCharacterMovement()->MaxWalkSpeed = RunSpeed; }
void ADysisCharacter::RunOff() { bRunning = false; GetCharacterMovement()->MaxWalkSpeed = WalkSpeed; }

// ───────────────────────── 镜头（灰盒 updateCamera） ─────────────────────────

void ADysisCharacter::SetViewMode(EDysisViewMode Mode)
{
	ViewMode = Mode;
	UpdateCamera(0.f);
}

void ADysisCharacter::SetAvatarShown(bool bShown)
{
	if (bAvatarShown == bShown) return;
	bAvatarShown = bShown;
	// 藏起来但影子留着（bCastHiddenShadow）
	if (AvatarBody) AvatarBody->SetHiddenInGame(!bShown);
	if (AvatarHead) AvatarHead->SetHiddenInGame(!bShown);
}

void ADysisCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bStartPitchApplied)
	{
		// 开局微微俯视。出生流程的最后引擎会把镜头的俯仰清零，所以等到第一帧再设。
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			FRotator Control = PC->GetControlRotation();
			Control.Pitch = StartPitch;
			PC->SetControlRotation(Control);
			bStartPitchApplied = true;
		}
	}
	UpdateCamera(DeltaSeconds);
}

void ADysisCharacter::UpdateCamera(float Dt)
{
	if (!CameraArm) return;
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (ViewMode == EDysisViewMode::FirstPerson)
	{
		CameraArm->bDoCollisionTest = false;
		CameraArm->TargetArmLength = 0.f;
		CameraArm->SocketOffset = FVector::ZeroVector;
		CameraArm->TargetOffset = FVector(0.f, 0.f, EyeHeight - HalfHeight);
		SetAvatarShown(false);
		return;
	}

	// 墙里的楼梯只有 1 m 宽：进去以后镜头不再偏肩，瞄点收到楼梯中线上，拉近到 2.6 m 以内
	const FVector Foot = GetFootLocation();
	const float R = DysisGB::ROf(Foot);
	const bool bInWall = (Time && Time->Zone.StartsWith(TEXT("tun:"))) || (R > DysisGB::R_IN - 15.f && R < DysisGB::R_OUT);
	CamTun = DysisGB::Toward(CamTun, bInWall ? 1.f : 0.f, 3.f, Dt);

	const FVector Right = FRotationMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::Y);
	FVector Target = Foot + Right * (CameraShoulder * (1.f - CamTun)) + FVector(0.f, 0.f, CameraTargetHeight - 8.f * CamTun);
	if (bInWall && R > 100.f)
	{
		const float RR = FMath::Lerp(R, FMath::Clamp(R, DysisGB::TUN_R0 + 32.f, DysisGB::TUN_R1 - 32.f), CamTun);
		Target.X *= RR / R;
		Target.Y *= RR / R;
	}
	CameraArm->bDoCollisionTest = true;
	CameraArm->SocketOffset = FVector::ZeroVector;
	CameraArm->TargetOffset = Target - GetActorLocation();   // 世界方向的偏移
	CameraArm->TargetArmLength = FMath::Lerp(CameraDistance, FMath::Min(CameraDistance, CameraTunnelDistance), CamTun);

	// 镜头被墙顶到贴脸的时候把人藏起来（灰盒：距离 > 0.9 m 才画人）
	if (Camera) SetAvatarShown(Dt <= 0.f || FVector::Dist(Camera->GetComponentLocation(), Target) > 90.f);
}

// ───────────────────────── 刚踏空还能跳（灰盒 0.14 s） ─────────────────────────

bool ADysisCharacter::CanJumpInternal_Implementation() const
{
	return Super::CanJumpInternal_Implementation() || bCanCoyoteJump;
}

void ADysisCharacter::Falling()
{
	Super::Falling();                                 // 从地面进入下落那一刻起表
	bCanCoyoteJump = true;
	GetWorldTimerManager().SetTimer(CoyoteTimerHandle,
		[this]() { bCanCoyoteJump = false; }, FMath::Max(CoyoteTime, 0.01f), false);
}

void ADysisCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	bCanCoyoteJump = false;                           // 用掉了就灭
}

void ADysisCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
	if (!bPressedJump && !GetCharacterMovement()->IsFalling())
		bCanCoyoteJump = false;                       // 落地 / 站稳即清
}
