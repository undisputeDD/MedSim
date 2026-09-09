#include "Tissue/Geometry/TetCutBoundary.h"

static int32 FindOrAddBoundaryVertex(
	const FVector3f& Position,
	TArray<FTetCutBoundaryVertex>& Vertices,
	float MergeTolerance)
{
	const float ToleranceSquared = FMath::Square(MergeTolerance);

	for (int32 Index = 0; Index < Vertices.Num(); ++Index)
	{
		if (FVector3f::DistSquared(Vertices[Index].Position, Position) <= ToleranceSquared)
		{
			return Index;
		}
	}

	FTetCutBoundaryVertex& Vertex = Vertices.AddDefaulted_GetRef();

	Vertex.Position = Position;

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
    const FTetCutBoundaryEdge& StartEdge,
    const TArray<TArray<int32>>& VertexToEdges,
    TArray<bool>& VisitedEdges,
    const FTetCutBoundary& OutBoundary,
    TArray<int32>& Chain)
{
    Chain.Reset();

    int32 StartVertex = VertexToEdges[StartEdge.VertexA].Num() == 1 ? StartEdge.VertexA : StartEdge.VertexB;
    int32 PreviousEdge = INDEX_NONE;
    int32 CurrentVertex = StartVertex;

    Chain.Add(CurrentVertex);

    while (true)
    {
        int32 NextEdge = INDEX_NONE;

        for (const int32 EdgeIndex : VertexToEdges[CurrentVertex])
        {
            if (VisitedEdges[EdgeIndex])
            {
                continue;
            }

            if (EdgeIndex == PreviousEdge)
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

        PreviousEdge = NextEdge;
        CurrentVertex = NextVertex;

        if (CurrentVertex == StartVertex)
        {
            break;
        }

        Chain.Add(CurrentVertex);
    }
}

static void BuildBoundaryChains(FTetCutBoundary& OutBoundary)
{
    // Build adjacency
    TArray<TArray<int32>> VertexToEdges;
    VertexToEdges.SetNum(OutBoundary.Vertices.Num());
    for (int32 EdgeIndex = 0; EdgeIndex < OutBoundary.Edges.Num(); ++EdgeIndex)
    {
        const FTetCutBoundaryEdge& Edge = OutBoundary.Edges[EdgeIndex];

        VertexToEdges[Edge.VertexA].Add(EdgeIndex);
        VertexToEdges[Edge.VertexB].Add(EdgeIndex);
    }

    // Main algo
    TArray<bool> VisitedEdges;
    VisitedEdges.Init(false, OutBoundary.Edges.Num());

    for (int32 VertexIndex = 0; VertexIndex < OutBoundary.Vertices.Num(); ++VertexIndex)
    {
        // Start from Vertex with 1 edge
        if (VertexToEdges[VertexIndex].Num() != 1)
        {
            continue;
        }

        int32 EdgeIndex = VertexToEdges[VertexIndex][0];
        if (VisitedEdges[EdgeIndex])
        {
            continue;
        }

        const FTetCutBoundaryEdge& StartEdge = OutBoundary.Edges[EdgeIndex];
        TArray<int32> Chain;

        TraceChain(StartEdge, VertexToEdges, VisitedEdges, OutBoundary, Chain);

        if (Chain.Num() >= 2)
        {
            OutBoundary.Chains.Add(MoveTemp(Chain));
        }
    }

    // If there is a loop somewhere -> check it
    for (int32 EdgeIndex = 0; EdgeIndex < OutBoundary.Edges.Num(); ++EdgeIndex)
    {
        if (VisitedEdges[EdgeIndex])
        {
            continue;
        }
        const FTetCutBoundaryEdge& StartEdge = OutBoundary.Edges[EdgeIndex];
        TArray<int32> Chain;
        TraceChain(StartEdge, VertexToEdges, VisitedEdges, OutBoundary, Chain);
        if (Chain.Num() >= 2)
        {
            OutBoundary.Chains.Add(MoveTemp(Chain));
        }
    }

    for (int32 VertexIndex = 0; VertexIndex < VertexToEdges.Num(); ++VertexIndex)
    {
        const int32 Degree = VertexToEdges[VertexIndex].Num();

        if (Degree <= 2)
        {
            continue;
        }

        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "Boundary branch: "
                "Vertex=%d Degree=%d"
            ),
            VertexIndex,
            Degree
        );

        for (const int32 EdgeIndex : VertexToEdges[VertexIndex])
        {
            const FTetCutBoundaryEdge& Edge = OutBoundary.Edges[EdgeIndex];

            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "    Edge=%d Patch=%d "
                    "V=(%d,%d) "
                    "Face=%d"
                ),
                EdgeIndex,
                Edge.SourcePatchIndex,
                Edge.VertexA,
                Edge.VertexB,
                Edge.TetFaceIndex
            );
        }
    }
}

// Normalize raw segments ->
struct FRawFaceSegment
{
    FVector3f A = FVector3f::ZeroVector;
    FVector3f B = FVector3f::ZeroVector;

    int32 TetFaceIndex = INDEX_NONE;

    int32 SourcePatchIndex = INDEX_NONE;
};

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
        }
    }
}
// Normalize raw segments <-

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
    const FTissueTet& Tet = TissueSnapshot.Tetrahedra[TetCutData.TetId];
    const TArray<FTissueVertex>& TissueVertices = TissueSnapshot.Vertices;

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

    // Build FTetCutBoundary.Vertices and FTetCutBoundary.Edges
    TMap<uint64, int32> EdgeToIndex;
    for (const FRawFaceSegment& Segment : NormalizedSegments)
    {
        const int32 VertexA = FindOrAddBoundaryVertex(Segment.A, OutBoundary.Vertices, VertexMergeTolerance);
        const int32 VertexB = FindOrAddBoundaryVertex(Segment.B, OutBoundary.Vertices, VertexMergeTolerance);
        if (VertexA == VertexB)
        {
            continue;
        }

        const uint64 EdgeKey = MakeEdgeKey(VertexA, VertexB);
        if (EdgeToIndex.Contains(EdgeKey))
        {
            continue;
        }

        FTetCutBoundaryEdge& Edge = OutBoundary.Edges.AddDefaulted_GetRef();

        Edge.VertexA = VertexA;
        Edge.VertexB = VertexB;
        Edge.TetFaceIndex = Segment.TetFaceIndex;

        Edge.SourcePatchIndex = Segment.SourcePatchIndex;

        EdgeToIndex.Add(EdgeKey, OutBoundary.Edges.Num() - 1);
    }

    // Build FTetCutBoundary.Chains
    BuildBoundaryChains(OutBoundary);

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
