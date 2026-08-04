#include "TissueBlock.h"
#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TissueManagerSubsystem.h"

ATissueBlock::ATissueBlock()
{
	PrimaryActorTick.bCanEverTick = false;

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	RootComponent = BaseMesh;

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	ProceduralMesh->SetupAttachment(RootComponent);

	ProceduralMesh->bUseComplexAsSimpleCollision = true;
	ProceduralMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void ATissueBlock::SliceTissue(UPrimitiveComponent* HitComponent, FVector SliceLocation, FVector SliceNormal)
{
	UProceduralMeshComponent* TargetProcMesh = Cast<UProceduralMeshComponent>(HitComponent);
	if (!TargetProcMesh) return;

	UProceduralMeshComponent* OutOtherHalf = nullptr;

	UE_LOG(LogTemp, Display, TEXT("SliceTissue"));

	UKismetProceduralMeshLibrary::SliceProceduralMesh(
		TargetProcMesh,
		SliceLocation,
		SliceNormal,
		true,
		OutOtherHalf,
		EProcMeshSliceCapOption::CreateNewSectionForCap,
		nullptr
	);

	if (OutOtherHalf)
	{
		OutOtherHalf->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

		OnTissueSliced.Broadcast(OutOtherHalf);
	}
}

void ATissueBlock::BeginPlay()
{
	Super::BeginPlay();
	
	if (!bIsSlicedPiece && BaseMesh && ProceduralMesh)
	{
		UKismetProceduralMeshLibrary::CopyProceduralMeshFromStaticMeshComponent(
			BaseMesh,
			0,
			ProceduralMesh,
			true
		);

		BaseMesh->SetVisibility(false);
		BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UWorld* World = GetWorld())
	{
		if (UTissueManagerSubsystem* Manager = World->GetSubsystem<UTissueManagerSubsystem>())
		{
			Manager->RegisterTissueBlock(this);
		}
	}
}
