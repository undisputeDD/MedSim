#include "Scalpel.h"
#include "TissueBlock.h"
#include "Components/SplineComponent.h"
#include "Kismet/GameplayStatics.h"

AScalpel::AScalpel()
{
    PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.016f;

	BladeEdgeSpline = CreateDefaultSubobject<USplineComponent>(TEXT("BladeEdgeSpline"));
	BladeEdgeSpline->SetupAttachment(InstrumentMesh);
}

void AScalpel::BeginPlay()
{
	Super::BeginPlay();

	if (BladeEdgeSpline && InstrumentMesh)
	{
		BladeEdgeSpline->AttachToComponent(InstrumentMesh, FAttachmentTransformRules::KeepRelativeTransform);
	}

	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATissueBlock::StaticClass(), FoundTissues);
}

void AScalpel::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsHeld)
    {
        PerformCutTrace();
    }
}

void AScalpel::PerformCutTrace()
{
	if (!BladeEdgeSpline) return;

	float BladeLengthCM = BladeEdgeSpline->GetSplineLength();
	float ScanResolutionCM = 0.2f;
	int32 NumSamples = FMath::Max(2, FMath::CeilToInt(BladeLengthCM / ScanResolutionCM));

	TArray<FVector> CurrentBladePoints;
	ATissueBlock* HitTissueThisFrame = nullptr;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		float DistanceAlongSpline = (float)i * ScanResolutionCM;
		FVector SamplePoint = BladeEdgeSpline->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World);
		CurrentBladePoints.Add(SamplePoint);
	}

	for (AActor* TissueActor : FoundTissues)
	{
		ATissueBlock* TissueBlock = Cast<ATissueBlock>(TissueActor);
		if (!TissueBlock) continue;

		FBox TissueBounds = TissueBlock->GetComponentsBoundingBox(true);

		TissueBounds = TissueBounds.ExpandBy(2.0f);

		bool bBladeInsideBounds = false;
		for (const FVector& Point : CurrentBladePoints)
		{
			if (TissueBounds.IsInside(Point))
			{
				bBladeInsideBounds = true;
				break;
			}
		}

		if (bBladeInsideBounds)
		{
			HitTissueThisFrame = TissueBlock;
			break;
		}
	}

	if (HitTissueThisFrame)
	{
		if (CurrentTissue != HitTissueThisFrame)
		{
			CurrentTissue = HitTissueThisFrame;
			UE_LOG(LogTemp, Warning, TEXT("Inside TissueBlock!"));
		}

		if (CurrentTissue && bHasPreviousBladePoints)
		{
			CurrentTissue->ApplyCut(PreviousBladePoints, CurrentBladePoints);
		}

		PreviousBladePoints = CurrentBladePoints;
		bHasPreviousBladePoints = true;
	}
	else
	{
		CurrentTissue = nullptr;
		bHasPreviousBladePoints = false;
		PreviousBladePoints.Reset();
		UE_LOG(LogTemp, Warning, TEXT("Outside TissueBlock!"));
	}
}
