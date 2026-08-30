#include "TissueBlock.h"
#include "ChaosFlesh/FleshComponent.h"
#include "Components/SplineComponent.h"

ATissueBlock::ATissueBlock()
{
    PrimaryActorTick.bCanEverTick = false;

    FleshComponent = CreateDefaultSubobject<UFleshComponent>(TEXT("FleshComponent"));
    RootComponent = FleshComponent;
}

void ATissueBlock::BeginPlay()
{
    Super::BeginPlay();
}

void ATissueBlock::ApplyCut(const TArray<FVector>& BladePoints)
{
    for (const FVector& Point : BladePoints)
    {
        DrawDebugSphere(GetWorld(), Point, 0.5f, 8, FColor::Red, false, 2.0f);
    }
}