#include "Tissue/Geometry/TetCutBoundary.h"

struct FRawFaceSegment
{
    FVector3f A = FVector3f::ZeroVector;
    FVector3f B = FVector3f::ZeroVector;

    int32 TetFaceIndex = INDEX_NONE;

    int32 SourcePatchIndex = INDEX_NONE;
};

static int32 FindOrAddBoundaryVertex(
	const FVector3f& Position,
	TArray<FTetCutBoundaryVertex>& Vertices,
	float MergeTolerance,
    // Debug
    const FRawFaceSegment& Segment)
{
	const float ToleranceSquared = FMath::Square(MergeTolerance);

	for (int32 Index = 0; Index < Vertices.Num(); ++Index)
	{
        const float DistSq = FVector3f::DistSquared(Position, Vertices[Index].Position);

		if (DistSq <= ToleranceSquared)
		{
            Vertices[Index].TetFaceIndices.AddUnique(Segment.TetFaceIndex);

            UE_LOG(
                LogTemp,
                VeryVerbose,
                TEXT(
                    "Boundary vertex merge: "
                    "New=(%.6f %.6f %.6f) "
                    "Existing=%d=(%.6f %.6f %.6f) "
                    "Dist=%.6f"
                ),
                Position.X,
                Position.Y,
                Position.Z,
                Index,
                Vertices[Index].Position.X,
                Vertices[Index].Position.Y,
                Vertices[Index].Position.Z,
                FMath::Sqrt(DistSq)
            );

			return Index;
		}
	}

	FTetCutBoundaryVertex& Vertex = Vertices.AddDefaulted_GetRef();

	Vertex.Position = Position;

    // Debug
    Vertex.TetFaceIndices.AddUnique(Segment.TetFaceIndex);

	return Vertices.Num() - 1;
}

static bool IsPointOnTriangle(
	const FVector3f& Point,
	const FVector3f& A,
	const FVector3f& B,
	const FVector3f& C,
	float Tolerance)
{
    const FVector3f AB = B - A;
    const FVector3f AC = C - A;

    const FVector3f Normal = FVector3f::CrossProduct(AB, AC);

    const float NormalSizeSquared = Normal.SizeSquared();

    if (NormalSizeSquared <= SMALL_NUMBER)
    {
        return false;
    }

    const float NormalSize = FMath::Sqrt(NormalSizeSquared);

    // Distance from point to triangle plane.
    const float PlaneDistance = FMath::Abs(FVector3f::DotProduct(Point - A, Normal)) / NormalSize;

    if (PlaneDistance > Tolerance)
    {
        return false;
    }

    const FVector3f AP = Point - A;

    const float Dot00 = FVector3f::DotProduct(AC, AC);

    const float Dot01 = FVector3f::DotProduct(AC, AB);

    const float Dot02 = FVector3f::DotProduct(AC, AP);

    const float Dot11 = FVector3f::DotProduct(AB, AB);

    const float Dot12 = FVector3f::DotProduct(AB, AP);

    const float Denominator = Dot00 * Dot11 - Dot01 * Dot01;

    if (FMath::Abs(Denominator) <= SMALL_NUMBER)
    {
        return false;
    }

    const float InvDenominator = 1.0f / Denominator;

    const float U = (Dot11 * Dot02 - Dot01 * Dot12) * InvDenominator;

    const float V = (Dot00 * Dot12 - Dot01 * Dot02) * InvDenominator;

    return U >= -Tolerance && V >= -Tolerance && (U + V) <= 1.0f + Tolerance;
}

static uint64 MakeEdgeKey(int32 VertexA, int32 VertexB)
{
    const uint32 A = static_cast<uint32>(FMath::Min(VertexA, VertexB));

    const uint32 B = static_cast<uint32>(FMath::Max(VertexA, VertexB));

    return (static_cast<uint64>(A) << 32) | static_cast<uint64>(B);
}

struct FTetFace
{
    int32 Index = INDEX_NONE;

    FIntVector Vertices;
};

static void BuildTetFaces(const FTissueTet & Tet, TArray<FTetFace>&OutFaces)
{
    OutFaces.Reset();
    OutFaces.Reserve(4);

    // Face opposite V0
    OutFaces.Add({
        0,
        FIntVector(
        Tet.Vertices.Y,
        Tet.Vertices.Z,
        Tet.Vertices.W)
    });

    // Face opposite V1
    OutFaces.Add({
        1,
        FIntVector(
        Tet.Vertices.X,
        Tet.Vertices.W,
        Tet.Vertices.Z)
    });

    // Face opposite V2
    OutFaces.Add({
        2,
        FIntVector(
        Tet.Vertices.X,
        Tet.Vertices.Y,
        Tet.Vertices.W)
    });

    // Face opposite V3
    OutFaces.Add({
        3,
        FIntVector(
        Tet.Vertices.X,
        Tet.Vertices.Z,
        Tet.Vertices.Y)
    });
}

static void TraceChain(
    int32 StartVertex,
    const TArray<TArray<int32>>& VertexToEdges,
    TArray<bool>& VisitedEdges,
    const FTetCutBoundary& OutBoundary,
    TArray<int32>& Chain)
{
    Chain.Reset();

    int32 CurrentVertex = StartVertex;

    TArray<bool> LocalVisitedVertices;
    LocalVisitedVertices.Init(false, OutBoundary.Vertices.Num());

    while (true)
    {
        if (!OutBoundary.Vertices.IsValidIndex(CurrentVertex))
        {
            break;
        }

        if (LocalVisitedVertices[CurrentVertex])
        {
            break;
        }

        LocalVisitedVertices[CurrentVertex] = true;
        Chain.Add(CurrentVertex);

        const int32 Degree = VertexToEdges[CurrentVertex].Num();

        // Endpoint or branch.
        // A valid manifold boundary continues only through degree 2.
        if (CurrentVertex != StartVertex && Degree != 2)
        {
            break;
        }

        int32 NextEdge = INDEX_NONE;

        for (const int32 EdgeIndex : VertexToEdges[CurrentVertex])
        {
            if (VisitedEdges[EdgeIndex])
            {
                continue;
            }

            NextEdge = EdgeIndex;
            break;
        }

        if (NextEdge == INDEX_NONE)
        {
            break;
        }

        VisitedEdges[NextEdge] = true;

        const FTetCutBoundaryEdge& Edge = OutBoundary.Edges[NextEdge];

        const int32 NextVertex = Edge.VertexA == CurrentVertex ? Edge.VertexB : Edge.VertexA;

        CurrentVertex = NextVertex;

        // Closed loop.
        if (CurrentVertex == StartVertex)
        {
            break;
        }
    }
}

static bool BuildBoundaryChains(FTetCutBoundary& OutBoundary)
{
    OutBoundary.Chains.Reset();

    TArray<TArray<int32>> VertexToEdges;
    VertexToEdges.SetNum(OutBoundary.Vertices.Num());
    for (int32 EdgeIndex = 0; EdgeIndex < OutBoundary.Edges.Num(); ++EdgeIndex)
    {
        const FTetCutBoundaryEdge& Edge = OutBoundary.Edges[EdgeIndex];

        if (!OutBoundary.Vertices.IsValidIndex(Edge.VertexA) || !OutBoundary.Vertices.IsValidIndex(Edge.VertexB))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "Invalid boundary edge %d: "
                    "V=(%d,%d)"
                ),
                EdgeIndex,
                Edge.VertexA,
                Edge.VertexB
            );

            return false;
        }

        VertexToEdges[Edge.VertexA].Add(EdgeIndex);
        VertexToEdges[Edge.VertexB].Add(EdgeIndex);
    }

    // ------------------------------------------------------------
    // Validate topology
    // ------------------------------------------------------------

    bool bHasBranch = false;

    for (int32 VertexIndex = 0; VertexIndex < VertexToEdges.Num(); ++VertexIndex)
    {
        const int32 Degree = VertexToEdges[VertexIndex].Num();

        if (Degree <= 2)
        {
            continue;
        }

        bHasBranch = true;

        const FVector3f& P = OutBoundary.Vertices[VertexIndex].Position;

        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "Boundary branch: "
                "Vertex=%d Degree=%d "
                "Position=(%.6f %.6f %.6f)"
            ),
            VertexIndex,
            Degree,
            P.X,
            P.Y,
            P.Z
        );
    }

    if (bHasBranch)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "TetCutBoundary: invalid/non-manifold "
                "boundary graph. Skip topology split for this tet."
            )
        );

        return false;
    }

    // ------------------------------------------------------------
    // Trace open chains
    // ------------------------------------------------------------

    TArray<bool> VisitedEdges;
    VisitedEdges.Init(false, OutBoundary.Edges.Num());

    for (int32 VertexIndex = 0; VertexIndex < VertexToEdges.Num(); ++VertexIndex)
    {
        if (VertexToEdges[VertexIndex].Num() != 1)
        {
            continue;
        }

        const int32 EdgeIndex = VertexToEdges[VertexIndex][0];

        if (VisitedEdges[EdgeIndex])
        {
            continue;
        }

        TArray<int32> Chain;

        TraceChain(
            VertexIndex,
            VertexToEdges,
            VisitedEdges,
            OutBoundary,
            Chain
        );

        if (Chain.Num() >= 2)
        {
            OutBoundary.Chains.Add(MoveTemp(Chain));
        }
    }

    // ------------------------------------------------------------
    // Trace remaining loops
    // ------------------------------------------------------------

    for (int32 EdgeIndex = 0; EdgeIndex < OutBoundary.Edges.Num(); ++EdgeIndex)
    {
        if (VisitedEdges[EdgeIndex])
        {
            continue;
        }

        TArray<int32> Chain;

        TraceChain(
            OutBoundary.Edges[EdgeIndex].VertexA,
            VertexToEdges,
            VisitedEdges,
            OutBoundary,
            Chain
        );

        if (Chain.Num() >= 2)
        {
            OutBoundary.Chains.Add(MoveTemp(Chain));
        }
    }

    // ------------------------------------------------------------
    // Verify every edge was consumed
    // ------------------------------------------------------------

    for (int32 EdgeIndex = 0; EdgeIndex < VisitedEdges.Num(); ++EdgeIndex)
    {
        if (!VisitedEdges[EdgeIndex])
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "Boundary edge %d was not included "
                    "in any chain."
                ),
                EdgeIndex
            );
        }
    }

    return OutBoundary.Chains.Num() > 0;
}

// Normalize overlapping boundary segments ->
// FRawFaceSegment

static bool IsPointOnSegment(
    const FVector3f& Point,
    const FVector3f& A,
    const FVector3f& B,
    float Tolerance)
{
    const FVector3f AB = B - A;
    const float LengthSquared = AB.SizeSquared();

    if (LengthSquared <= SMALL_NUMBER)
    {
        return false;
    }

    const float Length = FMath::Sqrt(LengthSquared);

    const FVector3f AP = Point - A;

    const float T = FVector3f::DotProduct(AP, AB) / LengthSquared;

    const float Distance = FVector3f::CrossProduct(AP, AB).Length() / Length;

    const float ParamTolerance = Tolerance / Length;

    return Distance <= Tolerance && T >= -ParamTolerance && T <= 1.0f + ParamTolerance;
}

static bool AreCollinearSegments(
    const FVector3f& A0,
    const FVector3f& A1,
    const FVector3f& B0,
    const FVector3f& B1,
    float Tolerance)
{
    const FVector3f Direction = A1 - A0;

    if (Direction.SizeSquared() <= SMALL_NUMBER)
    {
        return false;
    }

    const float Length = Direction.Length();

    const float DistanceB0 = FVector3f::CrossProduct(B0 - A0, Direction).Length() / Length;
    const float DistanceB1 = FVector3f::CrossProduct(B1 - A0, Direction).Length() / Length;

    return DistanceB0 <= Tolerance && DistanceB1 <= Tolerance;
}

static float SegmentParameter(
    const FVector3f& Point,
    const FVector3f& A,
    const FVector3f& B)
{
    const FVector3f AB = B - A;

    const float LengthSquared = AB.SizeSquared();

    if (LengthSquared <= SMALL_NUMBER)
    {
        return 0.0f;
    }

    return FVector3f::DotProduct(Point - A, AB) / LengthSquared;
}

static void NormalizeFaceSegments(
    const TArray<FRawFaceSegment>& RawSegments,
    float Tolerance,
    TArray<FRawFaceSegment>& OutSegments)
{
    OutSegments.Reset();

    for (int32 SegmentIndex = 0; SegmentIndex < RawSegments.Num(); ++SegmentIndex)
    {
        const FRawFaceSegment& Segment = RawSegments[SegmentIndex];

        const FVector3f A = Segment.A;
        const FVector3f B = Segment.B;

        const FVector3f AB = B - A;

        const float SegmentLength = AB.Length();

        if (SegmentLength <= Tolerance)
        {
            continue;
        }

        TArray<float> SplitParameters;
        SplitParameters.Reserve(RawSegments.Num() * 2);

        SplitParameters.Add(0.0f);
        SplitParameters.Add(1.0f);

        // Find all overlap endpoints on this segment.
        for (int32 OtherIndex = 0; OtherIndex < RawSegments.Num(); ++OtherIndex)
        {
            if (OtherIndex == SegmentIndex)
            {
                continue;
            }

            const FRawFaceSegment& Other = RawSegments[OtherIndex];

            if (Other.TetFaceIndex != Segment.TetFaceIndex)
            {
                continue;
            }

            if (!AreCollinearSegments(
                A,
                B,
                Other.A,
                Other.B,
                Tolerance))
            {
                continue;
            }

            const float TA = SegmentParameter(Other.A, A, B);

            const float TB = SegmentParameter(Other.B, A, B);

            if (TA >= 0.0f && TA <= 1.0f)
            {
                SplitParameters.Add(TA);
            }

            if (TB >= 0.0f && TB <= 1.0f)
            {
                SplitParameters.Add(TB);
            }
        }

        SplitParameters.Sort();

        const float ParameterTolerance = Tolerance / SegmentLength;

        // Remove duplicate split parameters.
        TArray<float> UniqueParameters;
        UniqueParameters.Reserve(SplitParameters.Num());

        for (const float T : SplitParameters)
        {
            if (UniqueParameters.IsEmpty() || FMath::Abs(T - UniqueParameters.Last()) > ParameterTolerance)
            {
                UniqueParameters.Add(FMath::Clamp(T, 0.0f, 1.0f));
            }
        }

        // Analyze every atomic interval.
        for (int32 i = 0; i + 1 < UniqueParameters.Num(); ++i)
        {
            const float T0 = UniqueParameters[i];
            const float T1 = UniqueParameters[i + 1];

            if (T1 - T0 <= ParameterTolerance)
            {
                continue;
            }

            const float MidT = 0.5f * (T0 + T1);

            const FVector3f Mid = FMath::Lerp(A, B, MidT);

            int32 CoverageCount = 0;

            for (const FRawFaceSegment& Other : RawSegments)
            {
                if (Other.TetFaceIndex != Segment.TetFaceIndex)
                {
                    continue;
                }

                if (!AreCollinearSegments(
                    A,
                    B,
                    Other.A,
                    Other.B,
                    Tolerance))
                {
                    continue;
                }

                if (IsPointOnSegment(
                    Mid,
                    Other.A,
                    Other.B,
                    Tolerance))
                {
                    ++CoverageCount;
                }
            }

            // Even number = internal/shared.
            if ((CoverageCount & 1) == 0)
            {
                continue;
            }

            FRawFaceSegment& Result = OutSegments.AddDefaulted_GetRef();

            Result.A = FMath::Lerp(A, B, T0);

            Result.B = FMath::Lerp(A, B, T1);

            Result.TetFaceIndex = Segment.TetFaceIndex;

            Result.SourcePatchIndex = Segment.SourcePatchIndex;
        }
    }
}
// Normalize overlapping boundary segments <-

bool TetCutBoundary::BuildTetCutBoundary(
    const FTetCutData& TetCutData,
    const FTissueTopologySnapshot& TissueSnapshot,
    float VertexMergeTolerance,
    FTetCutBoundary& OutBoundary)
{
    OutBoundary = FTetCutBoundary{};

    if (!TetCutData.bNeedsCut || TetCutData.Patches.IsEmpty())
    {
        return false;
    }

    // Build raw segments
    if (!TissueSnapshot.Tetrahedra.IsValidIndex(TetCutData.TetId))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BuildTetCutBoundary: "
                "Invalid TetId=%d"
            ),
            TetCutData.TetId
        );

        return false;
    }
    const FTissueTet& Tet = TissueSnapshot.Tetrahedra[TetCutData.TetId];
    const TArray<FTissueVertex>& TissueVertices = TissueSnapshot.Vertices;

    const FIntVector4& V = Tet.Vertices;

    if (!TissueSnapshot.Vertices.IsValidIndex(V.X) ||
        !TissueSnapshot.Vertices.IsValidIndex(V.Y) ||
        !TissueSnapshot.Vertices.IsValidIndex(V.Z) ||
        !TissueSnapshot.Vertices.IsValidIndex(V.W))
    {
        return false;
    }

    TArray<FTetFace> Faces;
    BuildTetFaces(Tet, Faces);

    TArray<FRawFaceSegment> RawSegments;
    for (int32 PatchIndex = 0; PatchIndex < TetCutData.Patches.Num(); ++PatchIndex)
    {
        const FTetCutPatch& Patch = TetCutData.Patches[PatchIndex];

        const TArray<FVector3f>& Polygon = Patch.Polygon;

        if (Polygon.Num() < 3)
        {
            continue;
        }

        for (int32 PointIndex = 0; PointIndex < Polygon.Num(); ++PointIndex)
        {
            const FVector3f& A = Polygon[PointIndex];
            const FVector3f& B = Polygon[(PointIndex + 1) % Polygon.Num()];

            for (const FTetFace& Face : Faces)
            {
                const FVector3f& FaceA = TissueVertices[Face.Vertices.X].CurrentPosition;

                const FVector3f& FaceB = TissueVertices[Face.Vertices.Y].CurrentPosition;

                const FVector3f& FaceC = TissueVertices[Face.Vertices.Z].CurrentPosition;

                const bool bAOnFace = IsPointOnTriangle(A, FaceA, FaceB, FaceC, VertexMergeTolerance);

                if (!bAOnFace)
                {
                    continue;
                }

                const bool bBOnFace = IsPointOnTriangle(B, FaceA, FaceB, FaceC, VertexMergeTolerance);

                if (!bBOnFace)
                {
                    continue;
                }

                FRawFaceSegment& RawSegment = RawSegments.AddDefaulted_GetRef();
                RawSegment.A = A;
                RawSegment.B = B;
                RawSegment.TetFaceIndex = Face.Index;
                RawSegment.SourcePatchIndex = PatchIndex;
            }
        }
    }

    TArray<FRawFaceSegment> NormalizedSegments;
    NormalizeFaceSegments(RawSegments, VertexMergeTolerance, NormalizedSegments);

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Boundary normalization: RawSegments=%d NormalizedSegments=%d"
        ),
        RawSegments.Num(),
        NormalizedSegments.Num()
    );

    // Build FTetCutBoundary.Vertices and FTetCutBoundary.Edges
    TMap<uint64, int32> EdgeToIndex;
    for (const FRawFaceSegment& Segment : NormalizedSegments)
    {
        const int32 VertexA = FindOrAddBoundaryVertex(Segment.A, OutBoundary.Vertices, VertexMergeTolerance, Segment);
        const int32 VertexB = FindOrAddBoundaryVertex(Segment.B, OutBoundary.Vertices, VertexMergeTolerance, Segment);
        if (VertexA == VertexB)
        {
            continue;
        }

        const uint64 EdgeKey = MakeEdgeKey(VertexA, VertexB);
        if (EdgeToIndex.Contains(EdgeKey))
        {
            const int32 ExistingEdgeIndex = EdgeToIndex[EdgeKey];

            OutBoundary.Edges[ExistingEdgeIndex].TetFaceIndices.AddUnique(Segment.TetFaceIndex);

            const TArray<int32>& ExistingFaces = OutBoundary.Edges[ExistingEdgeIndex].TetFaceIndices;

            UE_LOG(
                LogTemp,
                VeryVerbose,
                TEXT(
                    "Duplicate boundary edge: "
                    "V=(%d,%d), "
                    "OldFacesNum=%d NewFace=%d"
                ),
                VertexA,
                VertexB,
                ExistingFaces.Num(),
                Segment.TetFaceIndex
            );

            continue;
        }

        FTetCutBoundaryEdge& Edge = OutBoundary.Edges.AddDefaulted_GetRef();

        Edge.VertexA = VertexA;
        Edge.VertexB = VertexB;
        Edge.TetFaceIndices.AddUnique(Segment.TetFaceIndex);

        Edge.SourcePatchIndex = Segment.SourcePatchIndex;

        EdgeToIndex.Add(EdgeKey, OutBoundary.Edges.Num() - 1);
    }

    // Build FTetCutBoundary.Chains
    const bool bChainsValid = BuildBoundaryChains(OutBoundary);
    if (!bChainsValid)
    {
        return false;
    }

    return OutBoundary.Edges.Num() > 0 && OutBoundary.Chains.Num() > 0;
}

static int32 FindClosestSurfaceVertex(
    const FVector3f& Position,
    const FTetCutSurface& Surface,
    float MatchTolerance)
{
    const float MaxDistanceSquared = FMath::Square(MatchTolerance);

    float BestDistanceSquared = MaxDistanceSquared;

    int32 BestIndex = INDEX_NONE;

    for (int32 Index = 0; Index < Surface.Vertices.Num(); ++Index)
    {
        const float DistanceSquared = FVector3f::DistSquared(Position, Surface.Vertices[Index].Position);

        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestIndex = Index;
        }
    }

    return BestIndex;
}

bool TetCutBoundary::MapToSurface(const FTetCutSurface& Surface, FTetCutBoundary& Boundary, float MatchTolerance)
{
    for (FTetCutBoundaryVertex& BoundaryVertex : Boundary.Vertices)
    {
        BoundaryVertex.SurfaceVertexIndex = FindClosestSurfaceVertex(BoundaryVertex.Position, Surface, MatchTolerance);

        if (BoundaryVertex.SurfaceVertexIndex == INDEX_NONE)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "TetCutBoundary::MapToSurface "
                    "failed to map boundary vertex"
                )
            );

            return false;
        }
    }

    return true;
}
