#include "Tissue/TissueBlock.h"
#include "Tissue/Geometry/TissueIntersection.h"
#include "Tissue/Geometry/CutPath.h"
#include "Tissue/Geometry/SweptBlade.h"

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

void ATissueBlock::FindAffectedTetrahedra(const TArray<FVector3f>& PreviousBladePoints, const TArray<FVector3f>& CurrentBladePoints, TArray<FCutTetHit>& OutAffectedTets)
{
    OutAffectedTets.Reset();

    if (!FleshComponent)
        return;

    if (PreviousBladePoints.Num() != CurrentBladePoints.Num())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Blade sample count mismatch: Previous=%d Current=%d"),
            PreviousBladePoints.Num(),
            CurrentBladePoints.Num());

        return;
    }

    if (PreviousBladePoints.Num() == 0) // No segment to create: do nothing
        return;

    if (TissueSnapshot.Vertices.Num() == 0 || TissueSnapshot.Tetrahedra.Num() == 0)
    {
        return;
    }

    // ------------------------------------------------------------
    // Map:
    // TetId -> index inside OutAffectedTets
    //
    // This allows us to visit a Tet multiple times from
    // different blade trajectories but still keep ONE FCutTetHit.
    // ------------------------------------------------------------

    TMap<int32, int32> TetIdToHitIndex;

    // ------------------------------------------------------------
    // Blade sample trajectories
    // ------------------------------------------------------------

    for (int32 BladeIndex = 0; BladeIndex < PreviousBladePoints.Num(); ++BladeIndex)
    {
        const FVector3f SegmentStart = PreviousBladePoints[BladeIndex];
        const FVector3f SegmentEnd = CurrentBladePoints[BladeIndex];

        // No movement -> no trajectory
        if (SegmentStart.Equals(SegmentEnd, 0.001f))
        {
            continue;
        }

        // --------------------------------------------------------
        // Test against every tetrahedron
        // --------------------------------------------------------

        for (int32 TetId = 0; TetId < TissueSnapshot.Tetrahedra.Num(); ++TetId)
        {
            const FTissueTet& Tet = TissueSnapshot.Tetrahedra[TetId];

            const FVector3f& V0 = TissueSnapshot.Vertices[Tet.Vertices.X].CurrentPosition;
            const FVector3f& V1 = TissueSnapshot.Vertices[Tet.Vertices.Y].CurrentPosition;
            const FVector3f& V2 = TissueSnapshot.Vertices[Tet.Vertices.Z].CurrentPosition;
            const FVector3f& V3 = TissueSnapshot.Vertices[Tet.Vertices.W].CurrentPosition;

            TArray<FCutIntersection> OutIntersections;

            const bool bAffected = TissueIntersection::SegmentIntersectsTetrahedron(SegmentStart,
                                                                                    SegmentEnd,
                                                                                    BladeIndex,
                                                                                    V0,
                                                                                    V1,
                                                                                    V2,
                                                                                    V3,
                                                                                    OutIntersections);
            if (!bAffected)
            {
                continue;
            }

            // ----------------------------------------------------
            // Get existing FCutTetHit or create a new one
            // ----------------------------------------------------

            int32* ExistingHitIndex = TetIdToHitIndex.Find(TetId);
            if (!ExistingHitIndex)
            {
                const int32 NewHitIndex = OutAffectedTets.AddDefaulted();

                FCutTetHit& NewHit = OutAffectedTets[NewHitIndex];

                NewHit.TetId = TetId;

                TetIdToHitIndex.Add(TetId, NewHitIndex);

                ExistingHitIndex = TetIdToHitIndex.Find(TetId);
            }

            FCutTetHit& TetHit = OutAffectedTets[*ExistingHitIndex];

            // ----------------------------------------------------
            // Add all exact intersection events found for this
            // blade trajectory
            // ----------------------------------------------------

            for (const FCutIntersection& Intersection : OutIntersections)
            {
                TetHit.Intersections.Add(Intersection);
            }
        }
    }
}

void ATissueBlock::ApplyCut(const TArray<FVector>& PreviousBladePoints, const TArray<FVector>& CurrentBladePoints)
{
    UpdateCurrentPositions();

    // --------------------------------------------------------
    // World -> Local Blade points
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
    // Build swept surface
    // --------------------------------------------------------
    TArray<FSweptBladeTriangle> SweptTriangles;
    SweptBlade::BuildSurface(PreviousLocalPoints, CurrentLocalPoints, SweptTriangles);

    UE_LOG(LogTemp, Log, TEXT("Swept blade: %d triangles"), SweptTriangles.Num());
    for (int32 TriangleIndex = 0; TriangleIndex < SweptTriangles.Num(); ++TriangleIndex)
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
    }

    // --------------------------------------------------------

    // --------------------------------------------------------
    TArray<FCutTetHit> OutHits;
    FindAffectedTetrahedra(PreviousLocalPoints, CurrentLocalPoints, OutHits);

    int32 TotalRawIntersections = 0;

    for (const FCutTetHit& Hit : OutHits)
    {
        TotalRawIntersections += Hit.Intersections.Num();
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("AffectedTets=%d RawIntersections=%d"),
        OutHits.Num(),
        TotalRawIntersections);

    // --------------------------------------------------------
    TArray<FCutPathPoint> OutCutPathPoints;
    CutPath::BuildOrderedCutPoints(OutHits, OutCutPathPoints);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("OrderedCutPoints=%d"),
        OutCutPathPoints.Num());

    for (const FCutPathPoint CutPathPoint : OutCutPathPoints)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "  TetId=%d "
                "Blade=%d T=%.3f "
                "Point=(%.2f %.2f %.2f) "
                "Normal=(%.2f %.2f %.2f)"),
            CutPathPoint.TetId,
            CutPathPoint.BladeSampleIndex,
            CutPathPoint.SegmentT,
            CutPathPoint.Point.X,
            CutPathPoint.Point.Y,
            CutPathPoint.Point.Z,
            CutPathPoint.Normal.X,
            CutPathPoint.Normal.Y,
            CutPathPoint.Normal.Z);
    }

    // --------------------------------------------------------
}