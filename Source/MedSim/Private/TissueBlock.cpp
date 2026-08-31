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

bool IsPointInTetrahedron(const FVector3f& P, const FVector3f& V0, const FVector3f& V1, const FVector3f& V2, const FVector3f& V3)
{
    FVector3f d0 = V1 - V0;
    FVector3f d1 = V2 - V0;
    FVector3f d2 = V3 - V0;
    FVector3f dP = P - V0;

    float Det = FVector3f::DotProduct(d0, FVector3f::CrossProduct(d1, d2));

    if (FMath::Abs(Det) < UE_KINDA_SMALL_NUMBER) return false;

    float InvDet = 1.0f / Det;

    float u = FVector3f::DotProduct(dP, FVector3f::CrossProduct(d1, d2)) * InvDet;
    float v = FVector3f::DotProduct(d0, FVector3f::CrossProduct(dP, d2)) * InvDet;
    float w = FVector3f::DotProduct(d0, FVector3f::CrossProduct(d1, dP)) * InvDet;
    float x = 1.0f - u - v - w;

    float Eps = -0.01f;

    return (u >= Eps && v >= Eps && w >= Eps && x >= Eps);
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

    TArray<int32> CutTetrahedronIndices;
    for (int32 i = 0; i < Tetrahedrons.Num(); ++i)
    {
        const FIntVector4& TetIndices = Tetrahedrons[i];

        FVector3f V0 = Vertices[TetIndices.X];
        FVector3f V1 = Vertices[TetIndices.Y];
        FVector3f V2 = Vertices[TetIndices.Z];
        FVector3f V3 = Vertices[TetIndices.W];

        for (const FVector3f& BladePoint : LocalBladePoints)
        {
            if (IsPointInTetrahedron(BladePoint, V0, V1, V2, V3))
            {
                CutTetrahedronIndices.AddUnique(i);
                break;
            }
        }
    }

    if (CutTetrahedronIndices.Num() > 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Cut tetrahedrs: %d"), CutTetrahedronIndices.Num());
    }
}