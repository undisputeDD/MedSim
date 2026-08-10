#include "TissueBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
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

void ATissueBlock::MakeIncision(FVector CutLocation, FVector CutNormal)
{
	if (!DynamicMeshComponent || !DynamicMeshComponent->GetDynamicMesh()) return;

	FTransform BlockTransform = DynamicMeshComponent->GetComponentTransform();

	FVector LocalCutLocation = BlockTransform.InverseTransformPosition(CutLocation);
	FVector LocalCutNormal = BlockTransform.InverseTransformVectorNoScale(CutNormal);

	LocalCutLocation -= LocalCutNormal * 2.0f;

	UDynamicMesh* ToolMesh = NewObject<UDynamicMesh>();

	FTransform ToolTransform;
	ToolTransform.SetLocation(LocalCutLocation);
	ToolTransform.SetRotation(FRotationMatrix::MakeFromZ(LocalCutNormal).ToQuat());

	FGeometryScriptPrimitiveOptions PrimOptions;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
		ToolMesh,
		PrimOptions,
		FTransform::Identity,
		15.0f,
		3.0f,
		5.0f,
		1, 1, 1
	);

	FGeometryScriptMeshBooleanOptions BooleanOptions;
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
		DynamicMeshComponent->GetDynamicMesh(),
		FTransform::Identity,
		ToolMesh,
		ToolTransform,
		EGeometryScriptBooleanOperation::Subtract,
		BooleanOptions
	);

	DynamicMeshComponent->NotifyMeshUpdated();
	DynamicMeshComponent->UpdateCollision(false);
	DynamicMeshComponent->RecreatePhysicsState();
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
