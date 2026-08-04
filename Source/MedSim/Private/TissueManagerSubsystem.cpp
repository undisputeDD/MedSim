#include "TissueManagerSubsystem.h"
#include "TissueBlock.h"
#include "ProceduralMeshComponent.h"

void UTissueManagerSubsystem::RegisterTissueBlock(ATissueBlock* TissueBlock)
{
	if (TissueBlock)
	{
		TissueBlock->OnTissueSliced.AddUniqueDynamic(this, &UTissueManagerSubsystem::HandleTissueSliced);
	}
}

void UTissueManagerSubsystem::HandleTissueSliced(UProceduralMeshComponent* NewMeshComponent)
{
	if (!NewMeshComponent) return;

	UWorld* World = GetWorld();
	if (!World) return;

	FTransform SpawnTransform = NewMeshComponent->GetComponentTransform();
	ATissueBlock* NewTissuePiece = World->SpawnActorDeferred<ATissueBlock>(ATissueBlock::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (NewTissuePiece)
	{
		NewTissuePiece->bIsSlicedPiece = true;

		NewTissuePiece->FinishSpawning(SpawnTransform);

		if (NewTissuePiece->ProceduralMesh)
		{
			NewTissuePiece->ProceduralMesh->DestroyComponent();
		}

		NewMeshComponent->AttachToComponent(NewTissuePiece->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		NewTissuePiece->ProceduralMesh = NewMeshComponent;

		NewMeshComponent->bUseComplexAsSimpleCollision = false;
		NewMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		NewMeshComponent->SetCollisionProfileName(TEXT("PhysicsActor"));
		NewMeshComponent->SetSimulatePhysics(true);

		NewMeshComponent->ContainsPhysicsTriMeshData(true);
	}
}
