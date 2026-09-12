#include "Tissue/Geometry/TetCutSurface.h"

static void TriangulateConvexPolygon(
	const TArray<int32>& VertexIndices,
	const TArray<FTetCutSurfaceVertex>& SurfaceVertices,
	int32 SourcePatchIndex,
	const FVector3f& PatchNormal,
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

		const FVector3f Cross = FVector3f::CrossProduct(B - A, C - A);

		const float TriangleArea = 0.5f * Cross.Length();

		if (TriangleArea <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FVector3f TriangleNormal = Cross.GetSafeNormal();

		int32 FinalB = IndexB;
		int32 FinalC = IndexC;

		if (FVector3f::DotProduct(TriangleNormal, PatchNormal) < 0.0f)
		{
			Swap(FinalB, FinalC);

			TriangleNormal = -TriangleNormal;
		}

		FTetCutSurfaceTriangle& Triangle = OutTriangles.AddDefaulted_GetRef();

		Triangle.Vertices = FIntVector(IndexA, FinalB, FinalC);
		Triangle.SourcePatchIndex = SourcePatchIndex;
		Triangle.Normal = TriangleNormal;
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

		const float PolygonArea = Patch.Area;

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

		TriangulateConvexPolygon(PolygonVertexIndices, OutSurface.Vertices, PatchIndex, Patch.Normal, OutSurface.Triangles);
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