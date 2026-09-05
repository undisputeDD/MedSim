#include "Tissue/Geometry/SweptBladeBroadPhase.h"

namespace SweptBladeBroadPhase
{
	FTissueAABB SweptBladeBroadPhase::ComputeTriangleAABB(const FSweptBladeTriangle& Triangle)
	{
		FTissueAABB AABB;

		AABB.Min = FVector3f(
			FMath::Min3(
				Triangle.A.X,
				Triangle.B.X,
				Triangle.C.X
			),
			FMath::Min3(
				Triangle.A.Y,
				Triangle.B.Y,
				Triangle.C.Y
			),
			FMath::Min3(
				Triangle.A.Z,
				Triangle.B.Z,
				Triangle.C.Z
			)
		);

		AABB.Max = FVector3f(
			FMath::Max3(
				Triangle.A.X,
				Triangle.B.X,
				Triangle.C.X
			),
			FMath::Max3(
				Triangle.A.Y,
				Triangle.B.Y,
				Triangle.C.Y
			),
			FMath::Max3(
				Triangle.A.Z,
				Triangle.B.Z,
				Triangle.C.Z
			)
		);

		return AABB;
	}

	FTissueAABB SweptBladeBroadPhase::ComputeTetAABB(const FTissueTet& Tet, const TArray<FTissueVertex>& Vertices)
	{
		const FVector3f& V0 = Vertices[Tet.Vertices.X].CurrentPosition;
		const FVector3f& V1 = Vertices[Tet.Vertices.Y].CurrentPosition;
		const FVector3f& V2 = Vertices[Tet.Vertices.Z].CurrentPosition;
		const FVector3f& V3 = Vertices[Tet.Vertices.W].CurrentPosition;

		FTissueAABB AABB;

		AABB.Min = FVector3f(
			FMath::Min(V0.X, V1.X, V2.X, V3.X),
			FMath::Min(V0.Y, V1.Y, V2.Y, V3.Y),
			FMath::Min(V0.Z, V1.Z, V2.Z, V3.Z)
		);

		AABB.Max = FVector3f(
			FMath::Max(V0.X, V1.X, V2.X, V3.X),
			FMath::Max(V0.Y, V1.Y, V2.Y, V3.Y),
			FMath::Max(V0.Z, V1.Z, V2.Z, V3.Z)
		);

		return AABB;
	}

	static void BuildTetAABBs(
		const FTissueTopologySnapshot& TissueSnapshot,
		TArray<FTissueTetAABB>& OutTetAABBs)
	{
		OutTetAABBs.Reset();
		OutTetAABBs.Reserve(TissueSnapshot.Tetrahedra.Num());

		for (int32 TetId = 0; TetId < TissueSnapshot.Tetrahedra.Num(); ++TetId)
		{
			FTissueTetAABB& Entry = OutTetAABBs.AddDefaulted_GetRef();

			Entry.TetId = TetId;
			Entry.Bounds = ComputeTetAABB(TissueSnapshot.Tetrahedra[TetId], TissueSnapshot.Vertices);
		}
	}

	void SweptBladeBroadPhase::FindCandidateTetrahedra(
		const TArray<FSweptBladeTriangle>& SweptTriangles,
		const FTissueTopologySnapshot& TissueSnapshot,
		TArray<int32>& OutCandidateTetIds)
	{
		OutCandidateTetIds.Reset();

		if (SweptTriangles.IsEmpty() || TissueSnapshot.Tetrahedra.IsEmpty())
		{
			return;
		}

		TArray<FTissueTetAABB> TetAABBs;
		BuildTetAABBs(TissueSnapshot, TetAABBs);

		TSet<int32> CandidateTetSet;

		for (const FSweptBladeTriangle& Triangle : SweptTriangles)
		{
			const FTissueAABB TriangleAABB = ComputeTriangleAABB(Triangle);

			for (const FTissueTetAABB& TetEntry : TetAABBs)
			{
				if (!TriangleAABB.Intersects(TetEntry.Bounds))
				{
					continue;
				}

				CandidateTetSet.Add(TetEntry.TetId);
			}
		}

		OutCandidateTetIds.Reserve(CandidateTetSet.Num());

		for (const int32 TetId : CandidateTetSet)
		{
			OutCandidateTetIds.Add(TetId);
		}
	}
}