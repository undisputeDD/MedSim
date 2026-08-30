#include "Scalpel.h"
#include "TissueBlock.h"
#include "Components/SplineComponent.h"
#include "Kismet/GameplayStatics.h"

AScalpel::AScalpel()
{
    PrimaryActorTick.bCanEverTick = true;

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

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(GetOwner());
	Params.bTraceComplex = true;

	float BladeLengthCM = BladeEdgeSpline->GetSplineLength();

	float ScanResolutionCM = 0.2f;
	int32 NumSamples = FMath::Max(2, FMath::CeilToInt(BladeLengthCM / ScanResolutionCM));

	ATissueBlock* HitTissueThisFrame = nullptr;
	TArray<FVector> CurrentBladePoints;

	FVector BladeForwardDir = InstrumentMesh->GetForwardVector();

	for (int32 i = 0; i < NumSamples; ++i)
	{
		float DistanceAlongSpline = (float)i * ScanResolutionCM;

		FVector SamplePoint = BladeEdgeSpline->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World);

		CurrentBladePoints.Add(SamplePoint);

		FVector RayStart = SamplePoint + (BladeForwardDir * 3.0f);
		FVector RayEnd = SamplePoint;

		FHitResult HitResult;
		bool bLocalHit = GetWorld()->LineTraceSingleByChannel(HitResult, RayStart, RayEnd, ECC_Visibility, Params);

		DrawDebugLine(GetWorld(), RayStart, RayEnd, FColor::Cyan, false, -1.0f, 0, 0.2f);

		if (bLocalHit)
		{
			ATissueBlock* HitBlock = Cast<ATissueBlock>(HitResult.GetActor());
			if (HitBlock)
			{
				HitTissueThisFrame = HitBlock;
			}
		}
	}

	if (HitTissueThisFrame)
	{
		if (!bIsCutting || CurrentTissue != HitTissueThisFrame)
		{
			CurrentTissue = HitTissueThisFrame;
			bIsCutting = true;
		}

		if (bIsCutting && CurrentTissue)
		{
			CurrentTissue->ApplyCut(CurrentBladePoints);
		}
	}
	else
	{
		if (bIsCutting)
		{
			bIsCutting = false;
			CurrentTissue = nullptr;
		}
	}
}
