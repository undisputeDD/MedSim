#include "Tissue/TissueBlock.h"
#include "Tissue/Geometry/TissueIntersection.h"

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

void ATissueBlock::FindAffectedTetrahedra(const TArray<FVector>& PreviousBladePoints, const TArray<FVector>& CurrentBladePoints, TArray<FCutTetHit>& OutAffectedTets)
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
    // World -> Flesh local
    // ------------------------------------------------------------

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

    for (int32 BladeIndex = 0; BladeIndex < PreviousLocalPoints.Num(); ++BladeIndex)
    {
        const FVector3f SegmentStart = PreviousLocalPoints[BladeIndex];
        const FVector3f SegmentEnd = CurrentLocalPoints[BladeIndex];

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

    TArray<FCutTetHit> OutHits;
    FindAffectedTetrahedra(PreviousBladePoints, CurrentBladePoints, OutHits);

    UE_LOG(LogTemp, Display, TEXT("Affected tetrahedra: %d"), OutHits.Num());

    for (const FCutTetHit& Hit : OutHits)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Tet %d, Intersections=%d"),
            Hit.TetId,
            Hit.Intersections.Num());

        for (const FCutIntersection& Intersection : Hit.Intersections)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "  Blade=%d T=%.3f Face=%d "
                    "Point=(%.2f %.2f %.2f) "
                    "Normal=(%.2f %.2f %.2f)"),
                Intersection.BladeSampleIndex,
                Intersection.SegmentT,
                Intersection.TetFaceIndex,
                Intersection.Point.X,
                Intersection.Point.Y,
                Intersection.Point.Z,
                Intersection.Normal.X,
                Intersection.Normal.Y,
                Intersection.Normal.Z);
        }
    }
}