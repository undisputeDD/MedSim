#include "Scalpel.h"
#include "TissueBlock.h"
#include "Components/SplineComponent.h"

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

	float ScanResolutionCM = 0.1f;
	int32 NumSamples = FMath::Max(2, FMath::CeilToInt(BladeLengthCM / ScanResolutionCM));

	bool bHitAnything = false;
	float MaxCutDepthMM = -1.0f;
	FVector DeepestHitLocation = FVector::ZeroVector;
	FVector DeepestHitNormal = FVector::UpVector;
	ATissueBlock* HitTissueThisFrame = nullptr;

	FVector BladeForwardDir = InstrumentMesh->GetForwardVector();

	for (int32 i = 0; i < NumSamples; ++i)
	{
		float DistanceAlongSpline = (float)i * ScanResolutionCM;

		FVector SamplePoint = BladeEdgeSpline->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World);

		FVector RayStart = SamplePoint + (BladeForwardDir * 3.0f);
		FVector RayEnd = SamplePoint;

		FHitResult HitResult;
		bool bLocalHit = GetWorld()->LineTraceSingleByChannel(HitResult, RayStart, RayEnd, ECC_Visibility, Params);

		DrawDebugLine(GetWorld(), RayStart, RayEnd, FColor::Cyan, false, -1.0f, 0, 0.2f);

		if (bLocalHit)
		{
			float LocalDepthMM = FVector::Distance(HitResult.ImpactPoint, RayEnd) * 10.0f;

			if (LocalDepthMM > MaxCutDepthMM)
			{
				MaxCutDepthMM = LocalDepthMM;
				DeepestHitLocation = HitResult.ImpactPoint;
				DeepestHitNormal = HitResult.ImpactNormal;
				HitTissueThisFrame = Cast<ATissueBlock>(HitResult.GetActor());
				bHitAnything = true;
			}
		}
	}

	if (bHitAnything && HitTissueThisFrame)
	{
		if (!bIsCutting || CurrentTissue != HitTissueThisFrame)
		{
			CurrentTissue = HitTissueThisFrame;
			bIsCutting = true;
		}

		if (bIsCutting && CurrentTissue)
		{
			CurrentTissue->AddIncisionPoint(DeepestHitLocation, DeepestHitNormal, MaxCutDepthMM);
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
