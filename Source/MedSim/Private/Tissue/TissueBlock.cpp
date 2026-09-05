#include "Tissue/TissueBlock.h"
#include "Tissue/Geometry/SweptBlade.h"
#include "Tissue/Geometry/SweptBladeIntersection.h"
#include "Tissue/Geometry/TissueCutSurface.h"
#include "Tissue/Geometry/SweptBladeBroadPhase.h"

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
    UpdateCurrentPositions();

    // --------------------------------------------------------
    // 0. No motion -> No need to check
    // --------------------------------------------------------

    if (!WasMotion(PreviousBladePoints, CurrentBladePoints))
    {
        return;
    }
    // --------------------------------------------------------

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

    /*for (int32 TriangleIndex = 0; TriangleIndex < SweptTriangles.Num(); ++TriangleIndex)
    {
        const FSweptBladeTriangle& Triangle = SweptTriangles[TriangleIndex];

        UE_LOG(
            LogTemp,
            Log,
            TEXT(
                "SweptTriangle[%d] Samples=(%d,%d) "
                "A=(%.3f, %.3f, %.3f) "
                "B=(%.3f, %.3f, %.3f) "
                "C=(%.3f, %.3f, %.3f)"
            ),
            TriangleIndex,
            Triangle.BladeSampleA,
            Triangle.BladeSampleB,
            Triangle.A.X,
            Triangle.A.Y,
            Triangle.A.Z,
            Triangle.B.X,
            Triangle.B.Y,
            Triangle.B.Z,
            Triangle.C.X,
            Triangle.C.Y,
            Triangle.C.Z
        );
    }

    for (const FSweptBladeTriangle& Triangle : SweptTriangles)
    {
        const FVector WorldA = TissueTransform.TransformPosition(FVector(Triangle.A));
        const FVector WorldB = TissueTransform.TransformPosition(FVector(Triangle.B));
        const FVector WorldC = TissueTransform.TransformPosition(FVector(Triangle.C));

        DrawDebugLine(
            GetWorld(),
            WorldA,
            WorldB,
            FColor::Green,
            false,
            0.1f,
            0,
            1.5f
        );

        DrawDebugLine(
            GetWorld(),
            WorldB,
            WorldC,
            FColor::Green,
            false,
            0.1f,
            0,
            1.5f
        );

        DrawDebugLine(
            GetWorld(),
            WorldC,
            WorldA,
            FColor::Green,
            false,
            0.1f,
            0,
            1.5f
        );
    }*/

    // --------------------------------------------------------

    // --------------------------------------------------------
    // 3. AABB Broad phase
    // --------------------------------------------------------

    TArray<int32> CandidateTetIds;

    SweptBladeBroadPhase::FindCandidateTetrahedra(
        SweptTriangles,
        TissueSnapshot,
        CandidateTetIds
    );

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

    SweptBladeIntersection::FindIntersections(
        SweptTriangles,
        CandidateTetIds,
        TissueSnapshot,
        Intersections
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT("TriangleTet Intersections=%d"),
        Intersections.Num()
    );

    // --------------------------------------------------------

    // --------------------------------------------------------
    // 5. Debug FTriangleTetIntersection
    // --------------------------------------------------------

    for (const FTriangleTetIntersection& Intersection : Intersections)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "Tet=%d Triangle=%d PolygonVertices=%d"
            ),
            Intersection.TetId,
            Intersection.BladeTriangleIndex,
            Intersection.Polygon.Num()
        );

        for (int32 i = 0; i < Intersection.Polygon.Num(); ++i)
        {
            FVector WorldA = TissueTransform.TransformPosition(FVector(Intersection.Polygon[i]));
            FVector WorldB = TissueTransform.TransformPosition(FVector(Intersection.Polygon[(i + 1) % Intersection.Polygon.Num()]));

            DrawDebugLine(
                GetWorld(),
                WorldA,
                WorldB,
                FColor::Green,
                false,
                20.f,
                0,
                0.02f
            );
        }
    }

    // --------------------------------------------------------

    // --------------------------------------------------------
    // 6. Build FTetCutData
    // --------------------------------------------------------

    TArray<FTetCutData> TetCutData;
    TissueCutSurface::BuildTetCutData(Intersections, TetCutData);

    /*for (const FTetCutData& TetCut : TetCutData)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("TetId = %d PatchesSize = %d BoundaryIntersections = %d Area = %f NeedsCut = %s"),
            TetCut.TetId,
            TetCut.Patches.Num(),
            TetCut.BoundaryIntersections.Num(),
            TetCut.TotalIntersectionArea,
            TetCut.bNeedsCut ? TEXT("Yes") : TEXT("No")
        );
    }*/

    // --------------------------------------------------------

    for (FTetCutData& TetCut : TetCutData)
    {
        TissueCutSurface::FindTetEdgeIntersections(
            SweptTriangles,
            TissueSnapshot,
            TetCut
        );

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "TetId=%d "
                "Patches=%d "
                "Area=%.6f "
                "BoundaryIntersections=%d "
                "NeedsCut=%s"
            ),
            TetCut.TetId,
            TetCut.Patches.Num(),
            TetCut.TotalIntersectionArea,
            TetCut.BoundaryIntersections.Num(),
            TetCut.bNeedsCut
            ? TEXT("Yes")
            : TEXT("No")
        );

        for (const FTetBoundaryIntersection&
            Intersection :
            TetCut.BoundaryIntersections)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "    Edge=%d "
                    "T=%.4f "
                    "Point=(%.3f %.3f %.3f) "
                    "BladeTriangle=%d"
                ),
                Intersection.TetEdgeIndex,
                Intersection.EdgeT,
                Intersection.Point.X,
                Intersection.Point.Y,
                Intersection.Point.Z,
                Intersection.BladeTriangleIndex
            );

            DrawDebugSphere(
                GetWorld(),
                TissueTransform.TransformPosition(FVector(Intersection.Point)),
                0.15f,
                8,
                FColor::Red,
                false,
                20.0f,
                0,
                0.04f
            );
        }
    }
}