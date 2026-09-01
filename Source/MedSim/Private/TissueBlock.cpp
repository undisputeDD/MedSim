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
    if (!FleshComponent) return;

    const UFleshAsset* RestAsset = Cast<UFleshAsset>(FleshComponent->GetRestCollection());
    if (!RestAsset) return;

    const FFleshCollection* RestCollPtr = RestAsset->GetCollection();
    if (!RestCollPtr) return;
    const FManagedArrayCollection& RestCollection = *RestCollPtr;

    UFleshDynamicAsset* DynAsset = FleshComponent->GetDynamicCollection();
    if (!DynAsset) return;

    FManagedArrayCollection* DynCollPtr = DynAsset->GetCollection();
    if (!DynCollPtr) return;
    FManagedArrayCollection& DynCollection = *DynCollPtr;

    USimulationAsset* SimulationAsset = FleshComponent->GetSimulationCollection();
    if (!DynAsset) return;

    FManagedArrayCollection* SimCollPtr = SimulationAsset->GetCollection();
    if (!DynCollPtr) return;
    FManagedArrayCollection& SimCollection = *SimCollPtr;

    UE_LOG(LogTemp, Warning, TEXT("--- FLESH ASSET(Dynamic) STRUCTURE ---"));
    for (const FName& GroupName : SimCollection.GroupNames())
    {
        UE_LOG(LogTemp, Warning, TEXT("Group: %s"), *GroupName.ToString());

        for (const FName& AttrName : SimCollection.AttributeNames(GroupName))
        {
            UE_LOG(LogTemp, Warning, TEXT("  -> Attribute: %s"), *AttrName.ToString());
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("-----------------------------"));

    if (!RestCollection.HasGroup(FName("Tetrahedral")) || !DynCollection.HasGroup(FName("Vertices"))) return;

    const TManagedArray<FIntVector4>& Tetrahedrons = RestCollection.GetAttribute<FIntVector4>(FName("Tetrahedron"), FName("Tetrahedral"));

    const TManagedArray<FVector3f>& DynamicVertices = DynCollection.GetAttribute<FVector3f>(FName("Vertex"), FName("Vertices"));

    TManagedArray<float>& Activation = DynCollection.ModifyAttribute<float>(FName("Activation"), FName("Vertices"));

    FTransform FleshTransform = FleshComponent->GetComponentTransform();
    TArray<FVector3f> LocalBladePoints;
    for (const FVector& Point : BladePoints)
    {
        LocalBladePoints.Add((FVector3f)FleshTransform.InverseTransformPosition(Point));
    }

    bool bHasCut = false;

    for (int32 i = 0; i < Tetrahedrons.Num(); ++i)
    {
        const FIntVector4& TetIndices = Tetrahedrons[i];

        if (Activation[TetIndices.X] == 0.0f && Activation[TetIndices.Y] == 0.0f &&
            Activation[TetIndices.Z] == 0.0f && Activation[TetIndices.W] == 0.0f)
        {
            continue;
        }

        FVector3f V0 = DynamicVertices[TetIndices.X];
        FVector3f V1 = DynamicVertices[TetIndices.Y];
        FVector3f V2 = DynamicVertices[TetIndices.Z];
        FVector3f V3 = DynamicVertices[TetIndices.W];

        for (const FVector3f& BladePoint : LocalBladePoints)
        {
            if (IsPointInTetrahedron(BladePoint, V0, V1, V2, V3))
            {
                Activation[TetIndices.X] = 0.0f;
                Activation[TetIndices.Y] = 0.0f;
                Activation[TetIndices.Z] = 0.0f;
                Activation[TetIndices.W] = 0.0f;

                bHasCut = true;
                break;
            }
        }
    }

    if (bHasCut)
    {
        UE_LOG(LogTemp, Warning, TEXT("РАЗРЕЗ ВЫПОЛНЕН: Изменены флаги Activation в DynamicCollection!"));

        FleshComponent->MarkRenderStateDirty();
    }
}