#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TissueBlock.generated.h"

class UFleshComponent;
class UDeformableSolverComponent;
class UDeformableCollisionsComponent;

USTRUCT()
struct FTissueVertex
{
	GENERATED_BODY()

public:
	FVector3f RestPosition;
	FVector3f CurrentPosition;

	float Mass = 0.0f;
};

USTRUCT()
struct FTissueTet
{
	GENERATED_BODY()

	FIntVector4 Vertices;
};

USTRUCT()
struct FTissueTopologySnapshot
{
	GENERATED_BODY()

	TArray<FTissueVertex> Vertices;
	TArray<FTissueTet> Tetrahedra;
};

USTRUCT()
struct FCutIntersection
{
	GENERATED_BODY()

	// Exact intersection point in Tissue local space
	FVector3f Point = FVector3f::ZeroVector;

	// Index of blade sample trajectory:
	// PreviousBladePoints[BladeSampleIndex]
	// ->
	// CurrentBladePoints[BladeSampleIndex]
	int32 BladeSampleIndex = INDEX_NONE;

	// Position along that trajectory:
	// 0 = PreviousBladePoints[BladeSampleIndex]
	// 1 = CurrentBladePoints[BladeSampleIndex]
	float SegmentT = 0.0f;

	// Which tetrahedron face was intersected
	int32 TetFaceIndex = INDEX_NONE;

	FVector3f Normal = FVector3f::ZeroVector;
};

USTRUCT()
struct FCutTetHit
{
	GENERATED_BODY()

	// Which tetrahedron was hit
	int32 TetId = INDEX_NONE;

	// All intersection events with this tetrahedron
	TArray<FCutIntersection> Intersections;
};

UCLASS()
class MEDSIM_API ATissueBlock : public AActor
{
	GENERATED_BODY()
	
public:	
	ATissueBlock();

	void ApplyCut(const TArray<FVector>& PreviousBladePoints, const TArray<FVector>& CurrentBladePoints);

protected:
	virtual void BeginPlay() override;

private:
	bool BuildTissueSnapshot();
	void UpdateCurrentPositions();

	void FindAffectedTetrahedra(const TArray<FVector>& PreviousBladePoints, const TArray<FVector>& CurrentBladePoints, TArray<FCutTetHit>& OutAffectedTets);

public:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Tissue")
	UFleshComponent* FleshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Tissue")
	UDeformableSolverComponent* DeformableSolverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Tissue")
	UDeformableCollisionsComponent* DeformableCollisionsComponent;

private:
	FTissueTopologySnapshot TissueSnapshot;
};
