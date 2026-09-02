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

    const TManagedArray<FVector3f>& RestVertices =
        RestCollection->GetAttribute<FVector3f>(
            TEXT("Vertex"),
            TEXT("Vertices"));

    const TManagedArray<FIntVector4>& Tetrahedra =
        RestCollection->GetAttribute<FIntVector4>(
            TEXT("Tetrahedron"),
            TEXT("Tetrahedral"));

    const TManagedArray<float>& Mass =
        RestCollection->GetAttribute<float>(
            TEXT("Mass"),
            TEXT("Vertices"));

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

// Möller–Trumbore Algorithm
static bool SegmentIntersectsTriangle(
    const FVector3f& SegmentStart,
    const FVector3f& SegmentEnd,
    const FVector3f& A,
    const FVector3f& B,
    const FVector3f& C,
    FVector3f& OutIntersection,
    float& OutT)
{
    constexpr float Epsilon = 1e-6f;

    const FVector3f Direction = SegmentEnd - SegmentStart;
    const FVector3f Edge1 = B - A;
    const FVector3f Edge2 = C - A;

    const FVector3f PVec = FVector3f::CrossProduct(Direction, Edge2);

    const float Det = FVector3f::DotProduct(Edge1, PVec);

    if (FMath::Abs(Det) < Epsilon)
    {
        return false;
    }

    const float InvDet = 1.0f / Det;

    const FVector3f TVec = SegmentStart - A;

    const float U = FVector3f::DotProduct(TVec, PVec) * InvDet;

    if (U < -Epsilon || U > 1.0f + Epsilon)
    {
        return false;
    }

    const FVector3f QVec = FVector3f::CrossProduct(TVec, Edge1);

    const float V = FVector3f::DotProduct(Direction, QVec) * InvDet;

    if (V < -Epsilon ||
        U + V > 1.0f + Epsilon)
    {
        return false;
    }

    const float T = FVector3f::DotProduct(Edge2, QVec) * InvDet;

    if (T < -Epsilon || T > 1.0f + Epsilon)
    {
        return false;
    }

    OutT = FMath::Clamp(T, 0.0f, 1.0f);

    OutIntersection = SegmentStart + Direction * OutT;

    return true;
}

// Barycentric coords
static bool IsPointInTetrahedron(
    const FVector3f& P,
    const FVector3f& V0,
    const FVector3f& V1,
    const FVector3f& V2,
    const FVector3f& V3)
{
    const FVector3f D0 = V1 - V0;
    const FVector3f D1 = V2 - V0;
    const FVector3f D2 = V3 - V0;
    const FVector3f DP = P - V0;

    const float Det = FVector3f::DotProduct(D0, FVector3f::CrossProduct(D1, D2));

    constexpr float Epsilon = 1e-6f;

    if (FMath::Abs(Det) < Epsilon)
    {
        return false;
    }

    const float InvDet = 1.0f / Det;

    const float U = FVector3f::DotProduct(DP, FVector3f::CrossProduct(D1, D2)) * InvDet;

    const float V = FVector3f::DotProduct(D0, FVector3f::CrossProduct(DP, D2)) * InvDet;

    const float W = FVector3f::DotProduct(D0, FVector3f::CrossProduct(D1, DP)) * InvDet;

    const float X = 1.0f - U - V - W;

    const float Tolerance = 1e-4f;

    return U >= -Tolerance && V >= -Tolerance && W >= -Tolerance && X >= -Tolerance;
}

static bool SegmentIntersectsTetrahedron(
    const FVector3f& SegmentStart,
    const FVector3f& SegmentEnd,
    const FVector3f& V0,
    const FVector3f& V1,
    const FVector3f& V2,
    const FVector3f& V3,
    TArray<FVector3f>& OutIntersections)
{
    OutIntersections.Reset();

    // ------------------------------------------------------------
    // 1. Start / end inside
    // ------------------------------------------------------------

    if (IsPointInTetrahedron(SegmentStart, V0, V1, V2, V3))
    {
        OutIntersections.Add(SegmentStart);
    }

    if (IsPointInTetrahedron(SegmentEnd, V0, V1, V2, V3))
    {
        OutIntersections.Add(SegmentEnd);
    }

    // ------------------------------------------------------------
    // 2. Segment vs 4 tetrahedron faces
    // ------------------------------------------------------------

    struct FTriangle
    {
        FVector3f A;
        FVector3f B;
        FVector3f C;
    };

    const FTriangle Faces[4] =
    {
        { V0, V1, V2 },
        { V0, V1, V3 },
        { V0, V2, V3 },
        { V1, V2, V3 }
    };

    for (const FTriangle& Face : Faces)
    {
        FVector3f Intersection;
        float T = 0.0f;

        if (SegmentIntersectsTriangle(
            SegmentStart,
            SegmentEnd,
            Face.A,
            Face.B,
            Face.C,
            Intersection,
            T))
        {
            bool bAlreadyExists = false;

            for (const FVector3f& Existing : OutIntersections)
            {
                if (Existing.Equals(Intersection, 0.01f))
                {
                    bAlreadyExists = true;
                    break;
                }
            }

            if (!bAlreadyExists)
            {
                OutIntersections.Add(Intersection);
            }
        }
    }

    return OutIntersections.Num() > 0;
}

void ATissueBlock::FindAffectedTetrahedra(const TArray<FVector>& PreviousBladePoints, const TArray<FVector>& CurrentBladePoints, TArray<FCutTetHit>& OutHits)
{
    OutHits.Reset();

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

    const FTransform TissueTransform = FleshComponent->GetComponentTransform();

    // ------------------------------------------------------------
    // World -> Flesh local
    // ------------------------------------------------------------

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
    // Blade sample trajectories
    // ------------------------------------------------------------

    for (int32 BladeIndex = 0; BladeIndex < PreviousLocalPoints.Num(); ++BladeIndex)
    {
        const FVector3f SegmentStart = PreviousLocalPoints[BladeIndex];
        const FVector3f SegmentEnd = CurrentLocalPoints[BladeIndex];

        // Skip static sample point
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
            const FVector3f& V1 =TissueSnapshot.Vertices[Tet.Vertices.Y].CurrentPosition;
            const FVector3f& V2 =TissueSnapshot.Vertices[Tet.Vertices.Z].CurrentPosition;
            const FVector3f& V3 =TissueSnapshot.Vertices[Tet.Vertices.W].CurrentPosition;

            TArray<FVector3f> Intersections;

            if (!SegmentIntersectsTetrahedron(
                SegmentStart,
                SegmentEnd,
                V0, V1, V2, V3,
                Intersections))
            {
                continue;
            }

            // ----------------------------------------------------
            // Find existing hit for this Tet
            // ----------------------------------------------------

            FCutTetHit* Hit = nullptr;
            for (FCutTetHit& ExistingHit : OutHits)
            {
                if (ExistingHit.TetId == TetId)
                {
                    Hit = &ExistingHit;
                    break;
                }
            }

            if (!Hit)
            {
                FCutTetHit NewHit;
                NewHit.TetId = TetId;

                for (const FVector3f& Point : Intersections)
                {
                    NewHit.IntersectionPoints.Add(Point);
                }

                OutHits.Add(MoveTemp(NewHit));
            }
            else
            {
                for (const FVector3f& Point : Intersections)
                {
                    bool bExists = false;

                    for (const FVector3f& ExistingPoint : Hit->IntersectionPoints)
                    {
                        if (ExistingPoint.Equals(Point, 0.01f))
                        {
                            bExists = true;
                            break;
                        }
                    }

                    if (!bExists)
                    {
                        Hit->IntersectionPoints.Add(Point);
                    }
                }
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
            TEXT("Tet %d, intersections=%d"),
            Hit.TetId,
            Hit.IntersectionPoints.Num());
    }
}