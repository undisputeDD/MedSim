#include "Scalpel.h"
#include "TissueBlock.h"

AScalpel::AScalpel()
{
    PrimaryActorTick.bCanEverTick = true;
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
    FVector BladeEdge = InstrumentMesh->GetSocketLocation(FName("BladeMiddle"));

    FVector BladeForwardDirection = InstrumentMesh->GetForwardVector();

    FVector RayStart = BladeEdge + (BladeForwardDirection * 3.0f);
    FVector RayEnd = BladeEdge;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(GetOwner());
    Params.bTraceComplex = true;

    FHitResult HitResult;

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        RayStart,
        RayEnd,
        ECC_Visibility,
        Params
    );

    DrawDebugLine(GetWorld(), RayStart, RayEnd, FColor::Cyan, false, -1.0f, 0, 0.5f);

    if (bHit)
    {
        if (!bIsCutting || CurrentTissue != HitResult.GetActor())
        {
            CurrentTissue = Cast<ATissueBlock>(HitResult.GetActor());
            if (CurrentTissue)
            {
                bIsCutting = true;
            }
        }

        if (bIsCutting && CurrentTissue)
        {
            float CutDepthMM = FVector::Distance(HitResult.ImpactPoint, RayEnd) * 10.0f;

            CurrentTissue->AddIncisionPoint(HitResult.ImpactPoint, HitResult.ImpactNormal, CutDepthMM);
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
