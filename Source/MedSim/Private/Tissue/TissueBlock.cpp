#include "Tissue/TissueBlock.h"
#include "Tissue/Geometry/SweptBlade.h"
#include "Tissue/Geometry/SweptBladeIntersection.h"
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
    // 5. Build FTetCutData
    // --------------------------------------------------------

    TArray<FTetCutData> TetCutData;
    TetCutSurface::BuildTetCutData(Intersections, TetCutData);

    // --------------------------------------------------------

    // --------------------------------------------------------
    // 6. Build FTetCutSurface and FTetCutBoundary -> FTetCutGeometry
    // --------------------------------------------------------

    TArray<FTetCutGeometry> CutGeometry;
    for (const FTetCutData& TetCut : TetCutData)
    {
        if (!TetCut.bNeedsCut)
        {
            continue;
        }

        FTetCutGeometry Geometry;
        Geometry.TetId = TetCut.TetId;

        // ----------------------------------------------------
        // 6.1 Build cut surface
        // ----------------------------------------------------

        constexpr float SurfaceVertexMergeTolerance = 0.01f;

        if (!TetCutSurface::Build(TetCut, SurfaceVertexMergeTolerance, Geometry.Surface))
        {
            continue;
        }

        // ----------------------------------------------------
        // 6.2 Compute barycentric coordinates
        // ----------------------------------------------------

        if (!TissueSnapshot.Tetrahedra.IsValidIndex(TetCut.TetId))
        {
            UE_LOG(LogTemp, Error, TEXT("ApplyCut: invalid TetId=%d"), TetCut.TetId);
            continue;
        }

        const FTissueTet& Tet = TissueSnapshot.Tetrahedra[TetCut.TetId];

        if (!TissueSnapshot.Vertices.IsValidIndex(Tet.Vertices.X) ||
            !TissueSnapshot.Vertices.IsValidIndex(Tet.Vertices.Y) ||
            !TissueSnapshot.Vertices.IsValidIndex(Tet.Vertices.Z) ||
            !TissueSnapshot.Vertices.IsValidIndex(Tet.Vertices.W))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "ApplyCut: invalid tetrahedron "
                    "vertex indices. Tet=%d"
                ),
                TetCut.TetId
            );

            continue;
        }

        const FVector3f V0 = TissueSnapshot.Vertices[Tet.Vertices.X].CurrentPosition;
        const FVector3f V1 = TissueSnapshot.Vertices[Tet.Vertices.Y].CurrentPosition;
        const FVector3f V2 = TissueSnapshot.Vertices[Tet.Vertices.Z].CurrentPosition;
        const FVector3f V3 = TissueSnapshot.Vertices[Tet.Vertices.W].CurrentPosition;

        bool bBarycentricValid = true;

        constexpr float BarycentricTolerance = 0.01f;
        for (FTetCutSurfaceVertex& Vertex : Geometry.Surface.Vertices)
        {
            if (!TetGeometry::ComputeTetBarycentric(
                Vertex.Position,
                V0,
                V1,
                V2,
                V3,
                Vertex.Barycentric,
                BarycentricTolerance))
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT(
                        "ApplyCut: failed to compute "
                        "barycentric coordinates. "
                        "Tet=%d Position=(%.6f %.6f %.6f)"
                    ),
                    TetCut.TetId,
                    Vertex.Position.X,
                    Vertex.Position.Y,
                    Vertex.Position.Z
                );

                bBarycentricValid = false;
                break;
            }
        }

        if (!bBarycentricValid)
        {
            continue;
        }

        // ----------------------------------------------------
        // 6.3 Build boundary from surface topology
        // ----------------------------------------------------

        constexpr float TetFaceClassificationTolerance = 0.01f;
        if (!TetCutBoundary::Build(Geometry.Surface, TetFaceClassificationTolerance, Geometry.Boundary))
        {
            continue;
        }

        // ----------------------------------------------------
        // 6.4 Debug
        // ----------------------------------------------------

        if (bDebugSingleTet && TetCut.TetId == DebugTetId)
        {
            const FVector3f TetA = V0;
            const FVector3f TetB = V1;
            const FVector3f TetC = V2;
            const FVector3f TetD = V3;

            // ------------------------------------------------
            // Tetrahedron
            // ------------------------------------------------

            const TArray<TPair<FVector3f, FVector3f>> TetEdges =
            {
                { TetA, TetB },
                { TetA, TetC },
                { TetA, TetD },
                { TetB, TetC },
                { TetB, TetD },
                { TetC, TetD }
            };

            for (const TPair<FVector3f, FVector3f>& Edge : TetEdges)
            {
                DrawDebugLine(
                    GetWorld(),
                    TissueTransform.TransformPosition(FVector(Edge.Key)),
                    TissueTransform.TransformPosition(FVector(Edge.Value)),
                    FColor::Yellow,
                    true,
                    20.0f,
                    0,
                    0.08f
                );
            }

            // ------------------------------------------------
            // Cut surface
            // ------------------------------------------------

            for (const FTetCutSurfaceTriangle& Triangle : Geometry.Surface.Triangles)
            {
                const FVector3f A = Geometry.Surface.Vertices[Triangle.Vertices.X].Position;
                const FVector3f B = Geometry.Surface.Vertices[Triangle.Vertices.Y].Position;
                const FVector3f C = Geometry.Surface.Vertices[Triangle.Vertices.Z].Position;

                const FVector WorldA = TissueTransform.TransformPosition(FVector(A));
                const FVector WorldB = TissueTransform.TransformPosition(FVector(B));
                const FVector WorldC = TissueTransform.TransformPosition(FVector(C));

                DrawDebugLine(
                    GetWorld(),
                    WorldA,
                    WorldB,
                    FColor::Green,
                    true,
                    20.0f,
                    0,
                    0.06f
                );

                DrawDebugLine(
                    GetWorld(),
                    WorldB,
                    WorldC,
                    FColor::Green,
                    true,
                    20.0f,
                    0,
                    0.06f
                );

                DrawDebugLine(
                    GetWorld(),
                    WorldC,
                    WorldA,
                    FColor::Green,
                    true,
                    20.0f,
                    0,
                    0.06f
                );
            }

            // ------------------------------------------------
            // Boundary
            // ------------------------------------------------

            for (const FTetCutBoundaryEdge& Edge : Geometry.Boundary.Edges)
            {
                const FVector3f& A = Geometry.Boundary.Vertices[Edge.VertexA].Position;
                const FVector3f& B = Geometry.Boundary.Vertices[Edge.VertexB].Position;

                DrawDebugLine(
                    GetWorld(),
                    TissueTransform.TransformPosition(FVector(A)),
                    TissueTransform.TransformPosition(FVector(B)),
                    FColor::Red,
                    true,
                    20.0f,
                    0,
                    0.12f
                );
            }

            // ------------------------------------------------
            // Boundary vertices
            // ------------------------------------------------

            for (int32 BoundaryVertexIndex = 0; BoundaryVertexIndex < Geometry.Boundary.Vertices.Num(); ++BoundaryVertexIndex)
            {
                const FTetCutBoundaryVertex& Vertex = Geometry.Boundary.Vertices[BoundaryVertexIndex];
                const FVector WorldPosition = TissueTransform.TransformPosition(FVector(Vertex.Position));

                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BoundaryVertex[%d] "
                        "Surface=%d "
                        "Position=(%.6f %.6f %.6f) "
                        "Bary=(%.6f %.6f %.6f %.6f) "
                        "Faces=%d"
                    ),
                    BoundaryVertexIndex,
                    Vertex.SurfaceVertexIndex,
                    Vertex.Position.X,
                    Vertex.Position.Y,
                    Vertex.Position.Z,
                    Vertex.Barycentric.X,
                    Vertex.Barycentric.Y,
                    Vertex.Barycentric.Z,
                    Vertex.Barycentric.W,
                    Vertex.TetFaceIndices.Num()
                );

                DrawDebugSphere(
                    GetWorld(),
                    WorldPosition,
                    0.12f,
                    8,
                    FColor::Blue,
                    true,
                    20.0f,
                    0,
                    0.04f
                );

                DrawDebugString(
                    GetWorld(),
                    WorldPosition + FVector(0, 0, 0.15f),
                    FString::Printf(
                        TEXT("B%d / S%d"),
                        BoundaryVertexIndex,
                        Vertex.SurfaceVertexIndex
                    ),
                    nullptr,
                    FColor::White,
                    20.0f
                );
            }

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "DEBUG Tet=%d "
                    "SurfaceVertices=%d "
                    "SurfaceTriangles=%d "
                    "BoundaryVertices=%d "
                    "BoundaryEdges=%d "
                    "Chains=%d"
                ),
                TetCut.TetId,
                Geometry.Surface.Vertices.Num(),
                Geometry.Surface.Triangles.Num(),
                Geometry.Boundary.Vertices.Num(),
                Geometry.Boundary.Edges.Num(),
                Geometry.Boundary.Chains.Num()
            );

            for (int32 EdgeIndex = 0; EdgeIndex < Geometry.Boundary.Edges.Num(); ++EdgeIndex)
            {
                const FTetCutBoundaryEdge& Edge = Geometry.Boundary.Edges[EdgeIndex];

                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BoundaryEdge[%d] "
                        "V=(%d,%d) "
                        "Faces=%d"
                    ),
                    EdgeIndex,
                    Edge.VertexA,
                    Edge.VertexB,
                    Edge.TetFaceIndices.Num()
                );
            }

            for (int32 ChainIndex = 0; ChainIndex < Geometry.Boundary.Chains.Num(); ++ChainIndex)
            {
                FString ChainString;

                for (const int32 VertexIndex : Geometry.Boundary.Chains[ChainIndex])
                {
                    if (!ChainString.IsEmpty())
                    {
                        ChainString += TEXT(" -> ");
                    }

                    ChainString += FString::FromInt(VertexIndex);
                }

                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BoundaryChain[%d]: %s"
                    ),
                    ChainIndex,
                    *ChainString
                );
            }
        }

        // ----------------------------------------------------
        // 6.5 Store
        // ----------------------------------------------------

        CutGeometry.Add(MoveTemp(Geometry));
    }

    // --------------------------------------------------------
}