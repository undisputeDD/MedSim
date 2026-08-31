#include "TissueBlock.h"
#include "ChaosFlesh/FleshComponent.h"
#include "ChaosFlesh/ChaosDeformableSolverComponent.h"
#include "ChaosFlesh/ChaosDeformableCollisionsComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "ChaosFlesh/FleshAsset.h"
#include "GeometryCollection/ManagedArrayCollection.h"

ATissueBlock::ATissueBlock()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    FleshComponent = CreateDefaultSubobject<UFleshComponent>(TEXT("FleshComponent"));
    FleshComponent->SetupAttachment(RootComponent);

    DeformableSolverComponent = CreateDefaultSubobject<UDeformableSolverComponent>(TEXT("DeformableSolverComponent"));
    DeformableSolverComponent->SetupAttachment(RootComponent);

    DeformableCollisionsComponent = CreateDefaultSubobject<UDeformableCollisionsComponent>(TEXT("DeformableCollisions"));
    DeformableCollisionsComponent->SetupAttachment(RootComponent);

    FleshComponent->PrimarySolverComponent = DeformableSolverComponent;
    DeformableCollisionsComponent->PrimarySolverComponent = DeformableSolverComponent;
}

void ATissueBlock::BeginPlay()
{
    Super::BeginPlay();

    FleshComponent->EnableSimulation(DeformableSolverComponent);

    TArray<AActor*> FoundTables;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Table"), FoundTables);

    for (AActor* TableActor : FoundTables)
    {
        if (UStaticMeshComponent* TableMesh = TableActor->FindComponentByClass<UStaticMeshComponent>())
        {
            DeformableCollisionsComponent->AddStaticMeshComponent(TableMesh);
        }
    }
    DeformableCollisionsComponent->EnableSimulation(DeformableSolverComponent);
}

void ATissueBlock::ApplyCut(const TArray<FVector>& BladePoints)
{
    if (!FleshComponent || !FleshComponent->GetRestCollection()) return;

    const UFleshAsset* FleshAsset = Cast<UFleshAsset>(FleshComponent->GetRestCollection());
    if (!FleshAsset) return;

    const FFleshCollection* CollectionPtr = FleshAsset->GetFleshCollection().Get();
    if (!CollectionPtr)
    {
        UE_LOG(LogTemp, Error, TEXT("Flesh Collection is null!"));
        return;
    }

    const FManagedArrayCollection& Collection = *CollectionPtr;

    /*UE_LOG(LogTemp, Warning, TEXT("--- FLESH ASSET STRUCTURE ---"));
    for (const FName& GroupName : Collection.GroupNames())
    {
        UE_LOG(LogTemp, Warning, TEXT("Group: %s"), *GroupName.ToString());

        for (const FName& AttrName : Collection.AttributeNames(GroupName))
        {
            UE_LOG(LogTemp, Warning, TEXT("  -> Attribute: %s"), *AttrName.ToString());
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("-----------------------------"));*/

    if (!Collection.HasGroup(FName("Tetrahedral")) || !Collection.HasGroup(FName("Vertices")))
    {
        UE_LOG(LogTemp, Error, TEXT("Flesh Asset does not contain Tetrahedral or Vertices groups!"));
        return;
    }

    const TManagedArray<FVector3f>& Vertices = Collection.GetAttribute<FVector3f>(FName("Vertex"), FName("Vertices"));
    const TManagedArray<FIntVector4>& Tetrahedrons = Collection.GetAttribute<FIntVector4>(FName("Tetrahedron"), FName("Tetrahedral"));

    FTransform FleshTransform = FleshComponent->GetComponentTransform();
    TArray<FVector3f> LocalBladePoints;

    for (const FVector& Point : BladePoints)
    {
        FVector LocalPoint = FleshTransform.InverseTransformPosition(Point);
        LocalBladePoints.Add((FVector3f)LocalPoint);
    }

    UE_LOG(LogTemp, Warning, TEXT("Ready to cut! Total Tetrahedrons: %d, Total Blade Points: %d"), Tetrahedrons.Num(), LocalBladePoints.Num());
}