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