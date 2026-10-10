#include "DysisCharacter.h"
#include "DysisCharacterMovement.h"
#include "DysisHeroAnimInstance.h"
#include "DysisGreybox.h"
#include "Sky/DysisTimeComponent.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisSkyActor.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/DysisInteractComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
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
	CameraArm->bDoCollisionTest = false;     // 挡镜头的事自己算（UpdateCamera）：引擎自带的那一套瞄点一进墙镜头就整个弹到人身上
	CameraArm->TargetArmLength = CameraDistance;
	CameraArm->bEnableCameraLag = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	// 备用外形（灰盒的袍子：底半径 0.33 m、高 1.43 m，头在 1.56 m）：主角模型找不到的时候才用
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

	// 正式的主角模型：资产到 BeginPlay 再找。脚踩在胶囊底；模型导出时脸朝 +Y，转 −90° 朝前
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -CapsuleHalfHeight));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetGenerateOverlapEvents(false);
	GetMesh()->SetCanEverAffectNavigation(false);
	GetMesh()->bCastHiddenShadow = true;   // 第一人称时人藏起来，影子留着
	GetMesh()->bReceivesDecals = false;    // 脚下那团影子不要印到自己腿上

	BlobShadow = CreateDefaultSubobject<UDecalComponent>(TEXT("BlobShadow"));
	BlobShadow->SetupAttachment(GetCapsuleComponent());
	BlobShadow->SetRelativeLocation(FVector(0.f, 0.f, -CapsuleHalfHeight));
	BlobShadow->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));   // 朝正下方投
	BlobShadow->DecalSize = FVector(70.f, 44.f, 44.f);            // 上下各 70 cm（台阶、斜着的光都盖得住）、半径 44 cm
	BlobShadow->SetVisibility(false);

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
	SetupHeroMesh();
	SetViewMode(ViewMode);
}

// ───────────────────────── 主角模型和动作 ─────────────────────────

void ADysisCharacter::SetupHeroMesh()
{
	// 模型由 Art/Characters/ue_import_hero.py 导入；用到时再找（不在构造函数里攥着，导入脚本重跑时引擎要能换掉它）
	USkeletalMesh* Hero = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Dysis/Characters/Hero/SK_DysisHero.SK_DysisHero"));
	bHeroMesh = Hero != nullptr;
	PrevHeroLocation = GetActorLocation();
	if (!bHeroMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Dysis 主角：找不到 /Game/Dysis/Characters/Hero/SK_DysisHero，先用圆锥（跑一遍 Art/Characters/ue_import_hero.py）"));
		return;
	}
	GetMesh()->SetSkeletalMeshAsset(Hero);
	if (UMaterialInterface* Blob = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Fx/M_DysisBlobShadow.M_DysisBlobShadow"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		BlobShadowMID = UMaterialInstanceDynamic::Create(Blob, this);
		BlobShadow->SetDecalMaterial(BlobShadowMID);
		BlobShadow->SetVisibility(true);
	}
	GetMesh()->SetAnimInstanceClass(UDysisHeroAnimInstance::StaticClass());
	HeroMID = GetMesh()->GetMaterial(0) ? GetMesh()->CreateDynamicMaterialInstance(0) : nullptr;   // 头发的摆动要每帧给材质递数
	// 圆锥用不着了：不画，也不留影子
	for (UStaticMeshComponent* Part : { AvatarBody.Get(), AvatarHead.Get() })
		if (Part)
		{
			Part->SetVisibility(false);
			Part->SetCastShadow(false);
		}
}

void ADysisCharacter::UpdateHair(float Dt)
{
	if (!HeroMID || Dt <= 1.0e-4f) return;
	// 头这一帧的速度（位移 + 动作里头自己的晃动都算在内）；瞬移（重生、测试里的传送）不算
	const FVector Head = GetMesh()->GetSocketLocation(TEXT("head"));
	FVector HeadVel = bHairInit ? (Head - HairPrevHead) / Dt : FVector::ZeroVector;
	if (HeadVel.SizeSquared() > FMath::Square(2500.0)) { HeadVel = FVector::ZeroVector; HairPrevHeadVel = FVector::ZeroVector; }
	const FVector Kick = (HeadVel - HairPrevHeadVel).GetClampedToMaxSize(700.0);
	HairPrevHead = Head;
	HairPrevHeadVel = HeadVel;
	bHairInit = true;
	// 弹簧：头的速度一变，发梢被甩向反方向，然后晃回来（小步长算，帧率低也稳）
	HairLagVel -= Kick * HairInertia;
	const float W = 2.0f * PI * FMath::Max(HairSpringHz, 0.1f);
	for (float Left = FMath::Min(Dt, 0.1f); Left > 0.0f; Left -= 1.0f / 120.0f)
	{
		const float H = FMath::Min(Left, 1.0f / 120.0f);
		HairLagVel += (-W * W * HairLag - 2.0f * HairDamping * W * HairLagVel) * H;
		HairLag += HairLagVel * H;
	}
	// 一直在走：往后飘一点、抬起来一点
	const FVector Vel = GetVelocity();
	const float Speed = float(Vel.Size2D());
	const FVector Fwd = FRotator(0.0, GetActorRotation().Yaw, 0.0).Vector(), Right = FVector::CrossProduct(FVector::UpVector, Fwd);
	const FVector Raw = HairLag - FVector(Vel.X, Vel.Y, 0.0) * HairDragSeconds;
	// 按人的朝向收一收：往前（穿进后背）最多 2 cm，往后、往两边、往上各有个头
	const float F = FMath::Clamp(float(Raw | Fwd), -HairMaxLagCm, 2.0f);
	const float R = FMath::Clamp(float(Raw | Right), -0.7f * HairMaxLagCm, 0.7f * HairMaxLagCm);
	const float U = FMath::Clamp(float(Raw.Z) + 2.5f * FMath::Min(Speed / 520.0f, 1.0f), -2.0f, 0.5f * HairMaxLagCm);
	HairLagShown = Fwd * F + Right * R + FVector::UpVector * U;
	HairBreezeNow = DysisGB::Toward(HairBreezeNow, FMath::Lerp(HairBreezeCm, HairRunBreezeCm, FMath::Min(Speed / 520.0f, 1.0f)), 4.0f, Dt);
	HeroMID->SetVectorParameterValue(TEXT("HairLag"), FLinearColor(HairLagShown));
	HeroMID->SetVectorParameterValue(TEXT("HairSwayA"), FLinearColor(Right * HairBreezeNow));
	HeroMID->SetVectorParameterValue(TEXT("HairSwayB"), FLinearColor((Fwd * -0.6 + FVector::UpVector * 0.4) * (HairBreezeNow * 0.7f)));
}

FString ADysisCharacter::DescribeHair() const
{
	return FString::Printf(TEXT("{\"on\":%s,\"lag\":[%.2f,%.2f,%.2f],\"lagCm\":%.2f,\"breeze\":%.2f}"), HeroMID ? TEXT("true") : TEXT("false"),
		HairLagShown.X, HairLagShown.Y, HairLagShown.Z, HairLagShown.Size(), HairBreezeNow);
}

void ADysisCharacter::UpdateHeroState(float Dt)
{
	UpdateHair(Dt);
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const FVector Loc = GetActorLocation(), Delta = Loc - PrevHeroLocation;
	PrevHeroLocation = Loc;
	const float Speed = float(GetVelocity().Size2D());
	const bool bGround = Move && Move->IsMovingOnGround();
	// 踩着台阶（或斜着的光）往上：人在地上、在走，而且这一帧在升高。留 0.18 秒，免得一级和一级之间动作来回换
	if (bGround && Speed > 40.f && Dt > 1.0e-4f && Delta.Z / Dt > 45.0 && Delta.Z < 60.0) StepHoldSeconds = 0.18f;
	else StepHoldSeconds = FMath::Max(0.f, StepHoldSeconds - Dt);
	bAscendingSteps = StepHoldSeconds > 0.f && Speed > 40.f && bGround;
	LandingSeconds = FMath::Max(0.f, LandingSeconds - Dt);
	// 脚下的影子：离地（跳起来、往下落）的时候淡掉大半
	BlobShadowK = DysisGB::Toward(BlobShadowK, bGround ? 1.f : 0.25f, 4.f, Dt);
	if (BlobShadowMID) BlobShadowMID->SetScalarParameterValue(TEXT("Opacity"), BlobShadowK * BlobShadowStrength);
	// 站着换重心时身子会挪到一只脚上（离胶囊中心二十厘米）：影子跟着身子走
	if (bHeroMesh && BlobShadow)
	{
		const FVector Hip = GetMesh()->GetSocketLocation(TEXT("pelvis"));
		BlobShadow->SetWorldLocation(FVector(Hip.X, Hip.Y, Loc.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	}
	// 伸手：一走动、一离地就停
	TouchSeconds = (Speed > 35.f || !bGround) ? 0.f : FMath::Max(0.f, TouchSeconds - Dt);
}

void ADysisCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	LandingSeconds = 0.25f;
}

void ADysisCharacter::PlayTouch()
{
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!bHeroMesh || !Move || !Move->IsMovingOnGround() || GetVelocity().Size2D() > 35.0) return;
	const UDysisHeroAnimInstance* Anim = Cast<UDysisHeroAnimInstance>(GetMesh()->GetAnimInstance());
	const UAnimSequence* Clip = Anim && Anim->Clips.IsValidIndex(int32(EDysisHeroClip::TouchForward)) ? Anim->Clips[int32(EDysisHeroClip::TouchForward)].Get() : nullptr;
	TouchSeconds = Clip ? Clip->GetPlayLength() : 0.0f;
	++TouchSerial;
}

FName ADysisCharacter::GetHeroClipName() const
{
	const UDysisHeroAnimInstance* Anim = bHeroMesh ? Cast<UDysisHeroAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr;
	return Anim ? Anim->GetActiveClipName() : NAME_None;
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
	AdvanceAction = MakeAction(TEXT("IA_DysisAdvance"), EInputActionValueType::Boolean);

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
	Mapping->MapKey(AdvanceAction, EKeys::LeftMouseButton);
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
		In->BindAction(AdvanceAction, ETriggerEvent::Started, this, &ADysisCharacter::AdvancePressed);
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

bool ADysisCharacter::InDialogue() const
{
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		if (const ADysisHUD* H = Cast<ADysisHUD>(PC->GetHUD()))
			return H->ActiveDialogue && H->ActiveDialogue->IsPlaying();
	return false;
}

void ADysisCharacter::AdvancePressed()
{
	// 过剧情时单击鼠标 = 下一句
	if (UiBlocksInput()) return;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (ADysisHUD* H = Cast<ADysisHUD>(PC->GetHUD()))
			if (H->ActiveDialogue && H->ActiveDialogue->IsPlaying()) H->ActiveDialogue->AdvanceByPlayer();
}

void ADysisCharacter::JumpPressed()
{
	// 过剧情时空格、E 和鼠标一样是“下一句”（对话不再自己翻页，只用键盘的人也得能翻）
	if (InDialogue()) { AdvancePressed(); return; }
	if (!UiBlocksInput()) Jump();
}

void ADysisCharacter::TryInteractPressed()
{
	if (InDialogue()) { AdvancePressed(); return; }
	if (UiBlocksInput()) return;
	if (Interact && Interact->TryInteract()) PlayTouch();   // 互动成功：伸一下手
}

void ADysisCharacter::ToggleViewPressed()
{
	if (UiBlocksInput() || InDialogue()) return;
	SetViewMode(ViewMode == EDysisViewMode::FirstPerson ? EDysisViewMode::ThirdPerson : EDysisViewMode::FirstPerson);
}

void ADysisCharacter::RespawnPressed()
{
	if (UiBlocksInput() || InDialogue()) return;
	if (UDysisCharacterMovement* Move = Cast<UDysisCharacterMovement>(GetCharacterMovement())) Move->RespawnNow();
}

void ADysisCharacter::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	if (!InDialogue()) Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
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
	if (InDialogue()) return;   // 过剧情时镜头也定住
	const FVector2D V = Value.Get<FVector2D>();
	AddControllerYawInput(V.X);
	AddControllerPitchInput(-V.Y);
}

void ADysisCharacter::Zoom(const FInputActionValue& Value)
{
	if (UiBlocksInput() || InDialogue()) return;
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
	if (bHeroMesh)
	{
		GetMesh()->SetHiddenInGame(!bShown);
		return;
	}
	if (AvatarBody) AvatarBody->SetHiddenInGame(!bShown);
	if (AvatarHead) AvatarHead->SetHiddenInGame(!bShown);
}

void ADysisCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHeroState(DeltaSeconds);
	// 过剧情时镜头定住：在控制器那一层关掉“转镜头”的输入（鼠标、手柄都算），对话结束再放开
	if (const bool bTalking = InDialogue(); bTalking != bDialogueLookLocked)
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			bDialogueLookLocked = bTalking;
			PC->SetIgnoreLookInput(bTalking);
		}
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
	if (Camera)
	{
		// 白天自动曝光（殿里殿外亮度差很多）；入夜以后固定曝光，月夜才是暗的
		const bool bNightLook = Time && Time->bNight;
		// 黄昏的调子（太阳快落山时压暗、偏橙红，见 ADysisSkyActor::ApplyDuskLook）：白天把它加在自动曝光上
		const ADysisSkyActor* Sky = Time ? Time->SkyActor.Get() : nullptr;
		FPostProcessSettings& PP = Camera->PostProcessSettings;
		const float DuskBias = Sky ? Sky->ApplyDuskLook(PP) : 0.0f;
		static const IConsoleVariable* DefaultBias = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.AutoExposure.Bias"));
		PP.bOverride_AutoExposureMethod = bNightLook;
		PP.AutoExposureMethod = AEM_Manual;
		PP.bOverride_AutoExposureApplyPhysicalCameraExposure = bNightLook;
		PP.AutoExposureApplyPhysicalCameraExposure = false;
		PP.bOverride_AutoExposureBias = bNightLook || DuskBias != 0.0f;
		PP.AutoExposureBias = bNightLook ? NightExposureBias : (DefaultBias ? DefaultBias->GetFloat() : 1.0f) + DuskBias;
		if (bNightLook && NightTwilightBoost != 0.0f)
		{
			const float SunAlt = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(UDysisSkyLibrary::DysisSunDir(Time->H).Z, -1.0, 1.0)));
			PP.AutoExposureBias += NightTwilightBoost * (1.0f - DysisGB::Smoothstep(1.5f, -9.0f, SunAlt));
		}
	}
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
	// 镜头不穿墙（2026-10-10：以前瞄点是“人往右偏 55 cm”那一点，人贴着右边的墙走时它会伸进墙里，
	// 镜头就整个弹到人身上、下一帧又弹回去——看着是按方向键镜头乱晃，转视角时还会看到墙里墙外）：
	//   ① 瞄点先从人头顶上的中线朝肩头那边扫一个小球，被墙挡住就停在墙跟前；
	//   ② 再从瞄点朝镜头的方向扫同一个小球，被挡住就收到那里（立刻收），前面空了再慢慢放回去。
	// 小球的半径要盖得住“低模的墙面比碰撞用的原模型往里鼓出来的那一点”加上镜头自己的近裁面。
	const float WantArm = FMath::Lerp(CameraDistance, FMath::Min(CameraDistance, CameraTunnelDistance), CamTun);
	const FVector Head = Foot + FVector(0.f, 0.f, CameraTargetHeight - 8.f * CamTun);
	float FreeArm = WantArm;
	if (UWorld* World = GetWorld())
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(DysisCamera), /*bTraceComplex*/ false, this);
		const FCollisionShape Ball = FCollisionShape::MakeSphere(CameraProbeRadius);
		FHitResult Hit;
		if (!Head.Equals(Target, 1.0) && World->SweepSingleByChannel(Hit, Head, Target, FQuat::Identity, ECC_Camera, Ball, Query))
			Target = Hit.bStartPenetrating ? Head : Hit.Location;
		const FVector Back = -GetControlRotation().Vector();
		if (World->SweepSingleByChannel(Hit, Target, Target + Back * WantArm, FQuat::Identity, ECC_Camera, Ball, Query))
			FreeArm = Hit.bStartPenetrating ? 0.f : FMath::Max(0.f, Hit.Distance);
	}
	CamArmNow = (Dt <= 0.f || CamArmNow < 0.f || FreeArm < CamArmNow) ? FreeArm : DysisGB::Toward(CamArmNow, FreeArm, CameraPushOutSpeed, Dt);
	CameraArm->bDoCollisionTest = false;
	CameraArm->SocketOffset = FVector::ZeroVector;
	CameraArm->TargetOffset = Target - GetActorLocation();   // 世界方向的偏移
	CameraArm->TargetArmLength = CamArmNow;

	// 镜头被墙顶到贴脸的时候把人藏起来（灰盒：距离 > 0.9 m 才画人）
	SetAvatarShown(Dt <= 0.f || CamArmNow > 90.f);
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
