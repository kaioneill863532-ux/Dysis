#include "DysisInvisibleWall.h"
#include "Components/BoxComponent.h"

ADysisInvisibleWall::ADysisInvisibleWall()
{
	PrimaryActorTick.bCanEverTick = false;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->SetMobility(EComponentMobility::Static);
	Box->SetBoxExtent(ExtentCm);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);   // 只挡人
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCanEverAffectNavigation(false);
	Box->SetGenerateOverlapEvents(false);
	Box->ShapeColor = FColor(255, 120, 40);
}

void ADysisInvisibleWall::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (Box) Box->SetBoxExtent(ExtentCm);
}
