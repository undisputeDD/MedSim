#include "Tissue/TissueBlock.h"
#include "Tissue/Geometry/SweptBlade.h"
#include "Tissue/Geometry/SweptBladeIntersection.h"
#include "Tissue/Geometry/TetCutData.h"
#include "Tissue/Geometry/TetCutSurface.h"
#include "Tissue/Geometry/SweptBladeBroadPhase.h"
#include "Tissue/Geometry/TetCutBoundary.h"
#include "Tissue/Geometry/TetCutGeometry.h"
#include "Tissue/Geometry/TetGeometry.h"

#include "ChaosFlesh/FleshComponent.h"
#include "ChaosFlesh/ChaosDeformableSolverComponent.h"
#include "ChaosFlesh/ChaosDeformableCollisionsComponent.h"
#include "ChaosFlesh/FleshAsset.h"

#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"

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

    BuildTissueSnapshot();
}

bool ATissueBlock::BuildTissueSnapshot()
{
    TissueSnapshot.Vertices.Reset();
    TissueSnapshot.Tetrahedra.Reset();

    if (!FleshComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("BuildTissueSnapshot: FleshComponent is null"));
        return false;
    }

    // ------------------------------------------------------------
    // REST
    // ------------------------------------------------------------

    const UFleshAsset* RestAsset = FleshComponent->GetRestCollection();
    if (!RestAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("BuildTissueSnapshot: RestAsset is null"));
        return false;
    }

    const FFleshCollection* RestCollection = RestAsset->GetCollection();
    if (!RestCollection)
    {
        UE_LOG(LogTemp, Error, TEXT("BuildTissueSnapshot: RestCollection is null"));
        return false;
    }

    if (!RestCollection->HasAttribute(TEXT("Vertex"), TEXT("Vertices")))
    {
        UE_LOG(LogTemp, Error, TEXT("BuildTissueSnapshot: Rest Vertex attribute missing"));
        return false;
    }

    if (!RestCollection->HasAttribute(TEXT("Tetrahedron"), TEXT("Tetrahedral")))
    {
        UE_LOG(LogTemp, Error, TEXT("BuildTissueSnapshot: Rest Tetrahedron attribute missing"));
        return false;
    }

    if (!RestCollection->HasAttribute(TEXT("Mass"), TEXT("Vertices")))
    {
        UE_LOG(LogTemp, Error, TEXT("BuildTissueSnapshot: Rest Mass attribute missing"));
        return false;
    }

    const TManagedArray<FVector3f>& RestVertices = RestCollection->GetAttribute<FVector3f>(TEXT("Vertex"), TEXT("Vertices"));

    const TManagedArray<FIntVector4>& Tetrahedra = RestCollection->GetAttribute<FIntVector4>(TEXT("Tetrahedron"), TEXT("Tetrahedral"));

    const TManagedArray<float>& Mass = RestCollection->GetAttribute<float>(TEXT("Mass"), TEXT("Vertices"));

    // ------------------------------------------------------------
    // DYNAMIC
    // ------------------------------------------------------------

    UFleshDynamicAsset* DynamicAsset = FleshComponent->GetDynamicCollection();

    if (!DynamicAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("BuildTissueSnapshot: DynamicAsset is null"));
        return false;
    }

    FManagedArrayCollection* DynamicCollection = DynamicAsset->GetCollection();

    if (!DynamicCollection)
    {
        UE_LOG(LogTemp, Error, TEXT("BuildTissueSnapshot: DynamicCollection is null"));
        return false;
    }

    if (!DynamicCollection->HasAttribute(TEXT("Vertex"), TEXT("Vertices")))
    {
        UE_LOG(LogTemp, Error, TEXT("BuildTissueSnapshot: Dynamic Vertex attribute missing"));
        return false;
    }

    const TManagedArray<FVector3f>& CurrentVertices =
        DynamicCollection->GetAttribute<FVector3f>(
            TEXT("Vertex"),
            TEXT("Vertices"));

    // ------------------------------------------------------------
    // IMPORTANT VALIDATION
    // ------------------------------------------------------------

    if (RestVertices.Num() != CurrentVertices.Num())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("BuildTissueSnapshot: Vertex count mismatch! Rest=%d Dynamic=%d"),
            RestVertices.Num(),
            CurrentVertices.Num());

        return false;
    }

    if (RestVertices.Num() != Mass.Num())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("BuildTissueSnapshot: Vertex count and Mass count mismatch! Rest=%d Mass=%d"),
            RestVertices.Num(),
            Mass.Num());

        return false;
    }

    // ------------------------------------------------------------
    // COPY VERTICES
    // ------------------------------------------------------------

    TissueSnapshot.Vertices.Reserve(RestVertices.Num());

    for (int32 i = 0; i < RestVertices.Num(); ++i)
    {
        FTissueVertex& V = TissueSnapshot.Vertices.AddDefaulted_GetRef();

        V.RestPosition = RestVertices[i];
        V.CurrentPosition = CurrentVertices[i];
        V.Mass = Mass[i];
    }

    // ------------------------------------------------------------
    // COPY TETRAHEDRA
    // ------------------------------------------------------------

    TissueSnapshot.Tetrahedra.Reserve(Tetrahedra.Num());

    auto IsValidVertexIndex = [NumVertices = RestVertices.Num()](int32 Index)
        {
            return Index >= 0 && Index < NumVertices;
        };

    for (int32 i = 0; i < Tetrahedra.Num(); ++i)
    {
        FTissueTet& Tet = TissueSnapshot.Tetrahedra.AddDefaulted_GetRef();

        Tet.Vertices = Tetrahedra[i];

        const FIntVector4& TetVec = Tetrahedra[i];

        if (!IsValidVertexIndex(TetVec.X) ||
            !IsValidVertexIndex(TetVec.Y) ||
            !IsValidVertexIndex(TetVec.Z) ||
            !IsValidVertexIndex(TetVec.W))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Invalid tetrahedron %d: (%d,%d,%d,%d)"),
                i,
                TetVec.X, TetVec.Y, TetVec.Z, TetVec.W);

            TissueSnapshot.Tetrahedra.Reset();
            return false;
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Tissue snapshot built: Vertices=%d Tetrahedra=%d"),
        TissueSnapshot.Vertices.Num(),
        TissueSnapshot.Tetrahedra.Num());

    return true;
}

void ATissueBlock::UpdateCurrentPositions()
{
    UFleshDynamicAsset* DynamicAsset = FleshComponent->GetDynamicCollection();
    if (!DynamicAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateCurrentPositions: DynamicAsset is null"));
        return;
    }
    FManagedArrayCollection* DynamicCollection = DynamicAsset->GetCollection();
    if (!DynamicCollection)
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateCurrentPositions: DynamicCollection is null"));
        return;
    }
    if (!DynamicCollection->HasGroup(FName("Vertices")) || !DynamicCollection->HasAttribute(FName("Vertex"), FName("Vertices")))
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateCurrentPositions: Dynamic Vertex attribute missing"));
        return;
    }

    const TManagedArray<FVector3f>& CurrentVertices = DynamicCollection->GetAttribute<FVector3f>(TEXT("Vertex"), TEXT("Vertices"));

    if (TissueSnapshot.Vertices.Num() != CurrentVertices.Num())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("UpdateCurrentPositions: Vertex count mismatch! Snapshot=%d Dynamic=%d"),
            TissueSnapshot.Vertices.Num(),
            CurrentVertices.Num());

        return;
    }

    for (int32 i = 0; i < CurrentVertices.Num(); ++i)
    {
        TissueSnapshot.Vertices[i].CurrentPosition = CurrentVertices[i];
    }
}

static bool WasMotion(const TArray<FVector>& PreviousBladePoints, const TArray<FVector>& CurrentBladePoints)
{
    if (PreviousBladePoints.Num() != CurrentBladePoints.Num())
    {
        return false; // Invalid Motion
    }

    constexpr float MotionEpsilon = 0.001f;

    for (int32 i = 0; i < PreviousBladePoints.Num(); ++i)
    {
        if (!PreviousBladePoints[i].Equals(CurrentBladePoints[i], MotionEpsilon))
        {
            return true;
        }
    }

    return false;
}

void ATissueBlock::ApplyCut(const TArray<FVector>& PreviousBladePoints, const TArray<FVector>& CurrentBladePoints)
{
    // --------------------------------------------------------
    // 0. No motion -> No need to check
    // --------------------------------------------------------

    if (PreviousBladePoints.Num() < 2 || CurrentBladePoints.Num() < 2)
    {
        return;
    }

    if (PreviousBladePoints.Num() != CurrentBladePoints.Num())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "ApplyCut: Blade sample count mismatch. "
                "Previous=%d Current=%d"
            ),
            PreviousBladePoints.Num(),
            CurrentBladePoints.Num()
        );

        return;
    }

    if (!WasMotion(PreviousBladePoints, CurrentBladePoints))
    {
        return;
    }
    // --------------------------------------------------------

    // Update topology position
    UpdateCurrentPositions();

    // --------------------------------------------------------
    // 1. World -> Local Blade points
    // --------------------------------------------------------

    const FTransform TissueTransform = FleshComponent->GetComponentTransform();

    TArray<FVector3f> PreviousLocalPoints;
    TArray<FVector3f> CurrentLocalPoints;

    PreviousLocalPoints.Reserve(PreviousBladePoints.Num());
    CurrentLocalPoints.Reserve(CurrentBladePoints.Num());

    for (int32 i = 0; i < PreviousBladePoints.Num(); ++i)
    {
        PreviousLocalPoints.Add(FVector3f(TissueTransform.InverseTransformPosition(PreviousBladePoints[i])));
        CurrentLocalPoints.Add(FVector3f(TissueTransform.InverseTransformPosition(CurrentBladePoints[i])));
    }
    // --------------------------------------------------------

    // --------------------------------------------------------
    // 2. Build swept surface
    // --------------------------------------------------------

    TArray<FSweptBladeTriangle> SweptTriangles;
    SweptBlade::BuildSurface(PreviousLocalPoints, CurrentLocalPoints, SweptTriangles);

    UE_LOG(LogTemp, Log, TEXT("Swept blade: %d triangles"), SweptTriangles.Num());

    // --------------------------------------------------------

    // --------------------------------------------------------
    // 3. AABB Broad phase
    // --------------------------------------------------------

    TArray<int32> CandidateTetIds;
    SweptBladeBroadPhase::FindCandidateTetrahedra(SweptTriangles, TissueSnapshot, CandidateTetIds);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("AABB Broad Phase: CandidateTets=%d"),
        CandidateTetIds.Num()
    );

    // --------------------------------------------------------

    // --------------------------------------------------------
    // 4. Narrow phase
    // --------------------------------------------------------

    TArray<FTriangleTetIntersection> Intersections;
    SweptBladeIntersection::FindIntersections(SweptTriangles, CandidateTetIds, TissueSnapshot, Intersections);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("TriangleTet Intersections=%d"),
        Intersections.Num()
    );

    // --------------------------------------------------------

    // --------------------------------------------------------
    // 5. Build FTetCutData
    // --------------------------------------------------------

    TArray<FTetCutData> CutData;
    TetCutData::Build(Intersections, CutData);

    // --------------------------------------------------------

    // --------------------------------------------------------
    // 6. Build FTetCutSurface and FTetCutBoundary -> FTetCutGeometry
    // --------------------------------------------------------

    TArray<FTetCutGeometry> CutGeometry;
    TetCutGeometry::Build(CutData, TissueSnapshot, CutGeometry, TissueTransform, bDebugSingleTet, DebugTetId, GetWorld());

    // --------------------------------------------------------
}