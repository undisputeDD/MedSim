#include "Scalpel.h"

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
    TArray<FName> BladeSockets = {
        FName("BladeStart"),
        FName("BladeBottom1"),
        FName("BladeBottom2"),
        FName("BladeMiddle"),
        FName("BladeUp2"),
        FName("BladeUp1"),
        FName("BladeEnd")
    };

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(GetOwner());
    Params.bTraceComplex = true;

    for (int32 i = 0; i < BladeSockets.Num() - 1; ++i)
    {
        FVector SegmentStart = InstrumentMesh->GetSocketLocation(BladeSockets[i]);
        FVector SegmentEnd = InstrumentMesh->GetSocketLocation(BladeSockets[i + 1]);

        FHitResult HitResult;

        bool bHit = GetWorld()->LineTraceSingleByChannel(
            HitResult,
            SegmentStart,
            SegmentEnd,
            ECC_Visibility,
            Params
        );

        DrawDebugLine(GetWorld(), SegmentStart, SegmentEnd, FColor::Green, false, -1.0f, 0, 0.5f);

        if (bHit)
        {
            FVector CutPoint = HitResult.ImpactPoint;

            DrawDebugSphere(GetWorld(), CutPoint, 1.0f, 8, FColor::Red, false, 2.0f);
        }
    }
}
