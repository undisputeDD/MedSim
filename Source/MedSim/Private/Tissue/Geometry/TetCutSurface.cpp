#include "Tissue/Geometry/TetCutSurface.h"
#include "Tissue/Geometry/Utility.h"

constexpr float CutAreaEpsilon = 0.001f;

float TetCutSurface::ComputePolygonArea(const TArray<FVector3f>& Polygon)
{
	if (Polygon.Num() < 3)
	{
		return 0.0f;
	}

	const FVector3f& P0 = Polygon[0];

	float Area = 0.0f;

	for (int32 i = 1; i < Polygon.Num() - 1; ++i)
	{
		const FVector3f A = Polygon[i] - P0;
		const FVector3f B = Polygon[i + 1] - P0;

		const float TriangleArea = 0.5f * FVector3f::CrossProduct(A, B).Length();

		Area += TriangleArea;
	}

	return Area;
}

void TetCutSurface::BuildTetCutData(const TArray<FTriangleTetIntersection>& Intersections, TArray<FTetCutData>& OutTetCuts)
{
	OutTetCuts.Reset();

	TMap<int32, int32> TetIdToCutIndex;

	for (const FTriangleTetIntersection& Intersection : Intersections)
	{
		if (Intersection.TetId == INDEX_NONE)
		{
			continue;
		}

		int32* ExistingIndex = TetIdToCutIndex.Find(Intersection.TetId);
		int32 CutIndex;

		if (ExistingIndex)
		{
			CutIndex = *ExistingIndex;
		}
		else
		{
			CutIndex = OutTetCuts.AddDefaulted();

			OutTetCuts[CutIndex].TetId = Intersection.TetId;

			TetIdToCutIndex.Add(Intersection.TetId, CutIndex);
		}

		FTetCutData& TetCut = OutTetCuts[CutIndex];

		FTetCutPatch Patch;

		Patch.BladeTriangleIndex = Intersection.BladeTriangleIndex;
		Patch.Polygon = Intersection.Polygon;

		Patch.Normal = Intersection.Normal;

		Patch.Area = ComputePolygonArea(Patch.Polygon);

		if (Patch.Area <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// Maybe need for update - connected patch
		TetCut.TotalIntersectionArea += Patch.Area;

		TetCut.Patches.Add(MoveTemp(Patch));
	}

	for (FTetCutData& TetCut : OutTetCuts)
	{
		TetCut.bNeedsCut = TetCut.TotalIntersectionArea > CutAreaEpsilon;
	}
}

static void TriangulateConvexPolygon(
	const TArray<int32>& VertexIndices,
	const TArray<FTetCutSurfaceVertex>& SurfaceVertices,
	int32 SourcePatchIndex,
	TArray<FTetCutSurfaceTriangle>& OutTriangles)
{
	if (VertexIndices.Num() < 3)
	{
		return;
	}

	for (int32 i = 1; i < VertexIndices.Num() - 1; ++i)
	{
		const int32 IndexA = VertexIndices[0];
		const int32 IndexB = VertexIndices[i];
		const int32 IndexC = VertexIndices[i + 1];

		const FVector3f& A = SurfaceVertices[IndexA].Position;

		const FVector3f& B = SurfaceVertices[IndexB].Position;

		const FVector3f& C = SurfaceVertices[IndexC].Position;

		const float TriangleArea = 0.5f * FVector3f::CrossProduct(B - A, C - A).Length();

		if (TriangleArea <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FTetCutSurfaceTriangle& Triangle = OutTriangles.AddDefaulted_GetRef();

		Triangle.Vertices = FIntVector(IndexA, IndexB, IndexC);

		Triangle.SourcePatchIndex = SourcePatchIndex;
	}
}

static int32 FindOrAddVertex(
	const FVector3f& Position,
	TArray<FTetCutSurfaceVertex>& Vertices,
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

	FTetCutSurfaceVertex& Vertex = Vertices.AddDefaulted_GetRef();

	Vertex.Position = Position;

	return Vertices.Num() - 1;
}

static void RemoveDuplicateVertexIndices(TArray<int32>& Indices)
{
	TSet<int32> Seen;
	TArray<int32> Result;

	Result.Reserve(Indices.Num());

	for (const int32 Index : Indices)
	{
		if (Seen.Contains(Index))
		{
			continue;
		}

		Seen.Add(Index);
		Result.Add(Index);
	}

	Indices = MoveTemp(Result);
}

bool TetCutSurface::Build(
	const FTetCutData& TetCutData,
	float VertexMergeTolerance,
	FTetCutSurface& OutSurface)
{
	OutSurface = FTetCutSurface{};
	OutSurface.TetId = TetCutData.TetId;

	if (!TetCutData.bNeedsCut)
	{
		return false;
	}

	for (int32 PatchIndex = 0; PatchIndex < TetCutData.Patches.Num(); ++PatchIndex)
	{
		const FTetCutPatch& Patch = TetCutData.Patches[PatchIndex];

		if (Patch.Polygon.Num() < 3)
		{
			continue;
		}

		const float PolygonArea = TetCutSurface::ComputePolygonArea(Patch.Polygon);

		if (PolygonArea <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		TArray<int32> PolygonVertexIndices;
		PolygonVertexIndices.Reserve(Patch.Polygon.Num());

		for (const FVector3f& Position : Patch.Polygon)
		{
			const int32 VertexIndex = FindOrAddVertex(Position, OutSurface.Vertices, VertexMergeTolerance);

			PolygonVertexIndices.Add(VertexIndex);
		}

		RemoveDuplicateVertexIndices(PolygonVertexIndices);

		if (PolygonVertexIndices.Num() < 3)
		{
			continue;
		}

		TriangulateConvexPolygon(PolygonVertexIndices, OutSurface.Vertices, PatchIndex, OutSurface.Triangles);
	}

	// Compute final area from generated triangles.
	for (const FTetCutSurfaceTriangle& Triangle : OutSurface.Triangles)
	{
		const FVector3f& A = OutSurface.Vertices[Triangle.Vertices.X].Position;

		const FVector3f& B = OutSurface.Vertices[Triangle.Vertices.Y].Position;

		const FVector3f& C = OutSurface.Vertices[Triangle.Vertices.Z].Position;

		OutSurface.Area += 0.5f * FVector3f::CrossProduct(B - A, C - A).Length();
	}

	const float RawArea = TetCutData.TotalIntersectionArea;

	const float RelativeAreaError =
		RawArea > SMALL_NUMBER
		? FMath::Abs(OutSurface.Area - RawArea) / RawArea
		: 0.0f;

	UE_LOG(
		LogTemp,
		Display,
		TEXT(
			"TetCutSurface::Build "
			"Tet=%d "
			"Patches=%d "
			"Vertices=%d "
			"Triangles=%d "
			"Area=%.6f "
			"RawArea=%.6f "
			"RelativeError=%.2f%%"
		),
		OutSurface.TetId,
		TetCutData.Patches.Num(),
		OutSurface.Vertices.Num(),
		OutSurface.Triangles.Num(),
		OutSurface.Area,
		TetCutData.TotalIntersectionArea,
		RelativeAreaError * 100.f
	);

	return OutSurface.IsValid();
}

struct FTetFace
{
	int32 Index = INDEX_NONE;

	FIntVector Vertices;
};

static void BuildTetFaces(const FTissueTet& Tet, TArray<FTetFace>& OutFaces)
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

static void AddUniquePoint(
	const FVector3f& Point,
	TArray<FVector3f>& Points,
	float Tolerance)
{
	const float ToleranceSquared = FMath::Square(Tolerance);

	for (const FVector3f& Existing : Points)
	{
		if (FVector3f::DistSquared(Existing, Point) <= ToleranceSquared)
		{
			return;
		}
	}

	Points.Add(Point);
}

struct FTriangle3f
{
	FVector3f A;
	FVector3f B;
	FVector3f C;
};

static void GetTriangleEdges(
	const FTriangle3f& Triangle,
	FVector3f OutA[3],
	FVector3f OutB[3])
{
	OutA[0] = Triangle.A;
	OutB[0] = Triangle.B;

	OutA[1] = Triangle.B;
	OutB[1] = Triangle.C;

	OutA[2] = Triangle.C;
	OutB[2] = Triangle.A;
}

static bool IntersectTriangles(
	const FTriangle3f& A,
	const FTriangle3f& B,
	float PointTolerance,
	TArray<FVector3f>& OutPoints)
{
	OutPoints.Reset();

	FVector3f A0[3];
	FVector3f A1[3];

	FVector3f B0[3];
	FVector3f B1[3];

	GetTriangleEdges(A, A0, A1);
	GetTriangleEdges(B, B0, B1);

	// A edges against B.
	for (int32 Edge = 0; Edge < 3; ++Edge)
	{
		FVector3f Hit;
		float T;
		FVector3f Normal;

		if (Utility::SegmentIntersectsTriangle(
			A0[Edge],
			A1[Edge],
			B.A,
			B.B,
			B.C,
			Hit,
			T,
			Normal))
		{
			AddUniquePoint(
				Hit,
				OutPoints,
				PointTolerance
			);
		}
	}

	// B edges against A.
	for (int32 Edge = 0; Edge < 3; ++Edge)
	{
		FVector3f Hit;
		float T;
		FVector3f Normal;

		if (Utility::SegmentIntersectsTriangle(
			B0[Edge],
			B1[Edge],
			A.A,
			A.B,
			A.C,
			Hit,
			T,
			Normal))
		{
			AddUniquePoint(
				Hit,
				OutPoints,
				PointTolerance
			);
		}
	}

	return OutPoints.Num() >= 2;
}

static void KeepFarthestPair(TArray<FVector3f>& Points)
{
	if (Points.Num() <= 2)
	{
		return;
	}

	float MaxDistanceSquared = -1.0f;
	int32 BestA = 0;
	int32 BestB = 1;

	for (int32 i = 0; i < Points.Num(); ++i)
	{
		for (int32 j = i + 1; j < Points.Num(); ++j)
		{
			const float DistanceSquared = FVector3f::DistSquared(Points[i], Points[j]);

			if (DistanceSquared > MaxDistanceSquared)
			{
				MaxDistanceSquared = DistanceSquared;
				BestA = i;
				BestB = j;
			}
		}
	}

	const FVector3f A = Points[BestA];
	const FVector3f B = Points[BestB];

	Points.Reset();
	Points.Add(A);
	Points.Add(B);
}

void TetCutSurface::FindTetFaceCutSegments(const FTetCutSurface& CutSurface, const FTissueTopologySnapshot& TissueSnapshot, float PointTolerance, TArray<FTetFaceCutSegment>& OutSegments)
{
	OutSegments.Reset();

	const FTissueTet& Tet = TissueSnapshot.Tetrahedra[CutSurface.TetId];
	const TArray<FTissueVertex>& TissueVertices = TissueSnapshot.Vertices;

	TArray<FTetFace> Faces;
	BuildTetFaces(Tet, Faces);

	for (const FTetFace& Face : Faces)
	{
		const FVector3f& FaceA = TissueVertices[Face.Vertices.X].CurrentPosition;

		const FVector3f& FaceB = TissueVertices[Face.Vertices.Y].CurrentPosition;

		const FVector3f& FaceC = TissueVertices[Face.Vertices.Z].CurrentPosition;

		const FTriangle3f FaceTriangle{ FaceA, FaceB, FaceC };

		for (int32 CutTriangleIndex = 0; CutTriangleIndex < CutSurface.Triangles.Num(); ++CutTriangleIndex)
		{
			const FTetCutSurfaceTriangle& CutTriangle = CutSurface.Triangles[CutTriangleIndex];

			const FVector3f& A = CutSurface.Vertices[CutTriangle.Vertices.X].Position;

			const FVector3f& B = CutSurface.Vertices[CutTriangle.Vertices.Y].Position;

			const FVector3f& C = CutSurface.Vertices[CutTriangle.Vertices.Z].Position;

			const FTriangle3f CutTriangleGeometry{ A, B, C };

			TArray<FVector3f> Points;

			if (!IntersectTriangles(CutTriangleGeometry, FaceTriangle, PointTolerance, Points))
			{
				continue;
			}

			if (Points.Num() < 2)
			{
				continue;
			}

			KeepFarthestPair(Points);

			FTetFaceCutSegment& Segment = OutSegments.AddDefaulted_GetRef();

			Segment.TetFaceIndex = Face.Index;

			Segment.A = Points[0];
			Segment.B = Points[1];

			Segment.SourceCutTriangleIndex = CutTriangleIndex;
		}
	}
}