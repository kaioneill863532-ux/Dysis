#include "DysisMoteField.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"

UDysisMoteField::UDysisMoteField()
{
	SetMobility(EComponentMobility::Movable);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(false);
	bUseAsOccluder = false;
	NumCustomDataFloats = 3;     // 每一粒自己的三个随机数
	BoundsScale = 3.0f;          // 小片是在材质里挪到位的，引擎以为它们都挤在原地——把范围报大一点，免得被错当成“在画面外”
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded()) SetStaticMesh(Plane.Object);
}

UMaterialInstanceDynamic* UDysisMoteField::Mid()
{
	if (!MoteMID)
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Fx/M_DysisMote.M_DysisMote")))
		{
			MoteMID = UMaterialInstanceDynamic::Create(Mat, this);
			SetMaterial(0, MoteMID);
		}
	return MoteMID;
}

void UDysisMoteField::Build(int32 Count, int32 Seed)
{
	ClearInstances();
	Mid();
	FRandomStream Rnd(Seed);
	TArray<FTransform> Xf;
	Xf.Init(FTransform::Identity, Count);
	AddInstances(Xf, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ false);
	for (int32 i = 0; i < Count; ++i)
	{
		const float Data[3] = { Rnd.FRand(), Rnd.FRand(), Rnd.FRand() };
		SetCustomData(i, MakeArrayView(Data, 3), i == Count - 1);
	}
}

void UDysisMoteField::SetLook(const FLinearColor& Color, float SizeCm, const FVector& Flow, float Wobble, float Glow)
{
	if (UMaterialInstanceDynamic* M = Mid())
	{
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetScalarParameterValue(TEXT("Size"), SizeCm);
		M->SetVectorParameterValue(TEXT("Flow"), FLinearColor(float(Flow.X), float(Flow.Y), float(Flow.Z), 0.0f));
		M->SetScalarParameterValue(TEXT("Wobble"), Wobble);
		M->SetScalarParameterValue(TEXT("Glow"), Glow);
	}
}

void UDysisMoteField::SetOpacity(float Opacity)
{
	if (UMaterialInstanceDynamic* M = Mid()) M->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	const bool bShow = Opacity > 0.01f && MoteMID != nullptr;
	if (IsVisible() != bShow) SetVisibility(bShow);
}
