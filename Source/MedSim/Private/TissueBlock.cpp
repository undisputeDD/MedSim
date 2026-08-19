#include "TissueBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SplineComponent.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "UDynamicMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Canvas.h"
#include "Kismet/KismetRenderingLibrary.h"

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

	IncisionSpline = CreateDefaultSubobject<USplineComponent>(TEXT("IncisionSpline"));
	IncisionSpline->SetupAttachment(RootComponent);
	IncisionSpline->ClearSplinePoints();
}

void ATissueBlock::AddIncisionPoint(FVector HitLocation, FVector HitNormal, float CutDepth, FVector2D HitUV)
{
	CurrentIncisionPath.Add(FIncisionPoint(HitLocation, HitNormal, CutDepth, HitUV));

	if (!IncisionSpline) return;

	IncisionSpline->AddSplinePoint(HitLocation, ESplineCoordinateSpace::World, true);
	int32 LastPointIndex = IncisionSpline->GetNumberOfSplinePoints() - 1;
	IncisionSpline->SetSplinePointType(LastPointIndex, ESplinePointType::Curve);

	if (CutMaskRenderTarget && BrushMaterialClass)
	{
		UCanvas* Canvas = nullptr;
		FVector2D Size;
		FDrawToRenderTargetContext Context;

		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(GetWorld(), CutMaskRenderTarget, Canvas, Size, Context);

		if (Canvas)
		{
			float BrushSize = FMath::Clamp(CutDepth * 0.8f, 2.0f, 8.0f);

			FVector2D DrawPos = FVector2D((HitUV.X * Size.X) - (BrushSize / 2.0f), (HitUV.Y * Size.Y) - (BrushSize / 2.0f));
			FVector2D DrawSize = FVector2D(BrushSize, BrushSize);

			Canvas->K2_DrawMaterial(BrushMaterialClass, DrawPos, DrawSize, FVector2D(0.f, 0.f), FVector2D(1.f, 1.f), 0.0f, FVector2D(0.5f, 0.5f));
		}

		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(GetWorld(), Context);
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

				DynamicMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				DynamicMeshComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
				DynamicMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);

				DynamicMeshComponent->bEnableComplexCollision = true;
				DynamicMeshComponent->CollisionType = ECollisionTraceFlag::CTF_UseComplexAsSimple;

				DynamicMeshComponent->NotifyMeshUpdated();
				DynamicMeshComponent->UpdateCollision(true);
				DynamicMeshComponent->RecreatePhysicsState();

				CutMaskRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, 1024, 1024, RTF_RGBA16f);
				if (CutMaskRenderTarget)
				{
					UKismetRenderingLibrary::ClearRenderTarget2D(this, CutMaskRenderTarget, FLinearColor::Black);
				}

				UMaterialInterface* BaseMaterial = BaseMesh->GetMaterial(0);
				if (BaseMaterial)
				{
					DynamicTissueMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);

					if (DynamicTissueMaterial && CutMaskRenderTarget)
					{
						DynamicTissueMaterial->SetTextureParameterValue(FName("CutMask"), CutMaskRenderTarget);
					}

					DynamicMeshComponent->SetMaterial(0, DynamicTissueMaterial);
				}

				BaseMesh->SetVisibility(false);
				BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}
}
