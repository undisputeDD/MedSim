#include "TissueBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SplineComponent.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "UDynamicMesh.h"

ATissueBlock::ATissueBlock()
{
	PrimaryActorTick.bCanEverTick = false;

	DynamicMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DynamicMesh"));
	RootComponent = DynamicMeshComponent;

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(DynamicMeshComponent);

	DynamicMeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
	DynamicMeshComponent->bEnableComplexCollision = true;
	DynamicMeshComponent->bDeferCollisionUpdates = false;
	DynamicMeshComponent->CollisionType = ECollisionTraceFlag::CTF_UseComplexAsSimple;
}

void ATissueBlock::AddIncisionPoint(FVector HitLocation, FVector HitNormal, float CutDepth)
{
	CurrentIncisionPath.Add(FIncisionPoint(HitLocation, HitNormal, CutDepth));

	IncisionSpline->AddSplinePoint(HitLocation, ESplineCoordinateSpace::World, true);

	int32 LastPointIndex = IncisionSpline->GetNumberOfSplinePoints() - 1;
	IncisionSpline->SetSplinePointType(LastPointIndex, ESplinePointType::Curve);

	if (LastPointIndex > 0)
	{
		FVector PrevLocation = IncisionSpline->GetLocationAtSplinePoint(LastPointIndex - 1, ESplineCoordinateSpace::World);
		FVector CurrentLocation = IncisionSpline->GetLocationAtSplinePoint(LastPointIndex, ESplineCoordinateSpace::World);

		DrawDebugLine(GetWorld(), PrevLocation, CurrentLocation, FColor::Red, false, 5.0f, 0, 2.0f);
	}
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
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("TissueBlock: Dynamic Mesh successfully generated!"));

				UMaterialInterface* BaseMaterial = BaseMesh->GetMaterial(0);
				if (BaseMaterial)
				{
					DynamicMeshComponent->SetMaterial(0, BaseMaterial);
				}

				DynamicMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				DynamicMeshComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
				DynamicMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);

				DynamicMeshComponent->bEnableComplexCollision = true;
				DynamicMeshComponent->CollisionType = ECollisionTraceFlag::CTF_UseComplexAsSimple;

				DynamicMeshComponent->NotifyMeshUpdated();
				DynamicMeshComponent->UpdateCollision(true);
				DynamicMeshComponent->RecreatePhysicsState();

				BaseMesh->SetVisibility(false);
				BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}
}
