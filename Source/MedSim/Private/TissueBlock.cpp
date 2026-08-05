#include "TissueBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "UDynamicMesh.h"

ATissueBlock::ATissueBlock()
{
	PrimaryActorTick.bCanEverTick = false;

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	RootComponent = BaseMesh;

	DynamicMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DynamicMesh"));
	DynamicMeshComponent->SetupAttachment(RootComponent);

	DynamicMeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
	DynamicMeshComponent->bEnableComplexCollision = true;
}

void ATissueBlock::BeginPlay()
{
	Super::BeginPlay();
	
	if (BaseMesh && BaseMesh->GetStaticMesh() && DynamicMeshComponent)
	{
		UDynamicMesh* TargetDynamicMesh = DynamicMeshComponent->GetDynamicMesh();

		if (TargetDynamicMesh)
		{
			EGeometryScriptOutcomePins Outcome;

			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
				BaseMesh->GetStaticMesh(),
				TargetDynamicMesh,
				FGeometryScriptCopyMeshFromAssetOptions(),
				FGeometryScriptMeshReadLOD(),
				Outcome
			);

			if (Outcome == EGeometryScriptOutcomePins::Success)
			{
				BaseMesh->SetVisibility(false);
				BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

				DynamicMeshComponent->UpdateCollision(true);
			}
		}
	}
}
