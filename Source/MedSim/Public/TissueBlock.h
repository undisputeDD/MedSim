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
struct FCutTetHit
{
	GENERATED_BODY()

	int32 TetId = INDEX_NONE;

	TArray<FVector3f> IntersectionPoints;

	FVector3f Normal = FVector3f::ZeroVector;
};

UCLASS()
class MEDSIM_API ATissueBlock : public AActor
{
	GENERATED_BODY()
	
public:	
	ATissueBlock();

	void ApplyCut(const TArray<FVector>& BladePoints);

protected:
	virtual void BeginPlay() override;

private:
	bool BuildTissueSnapshot();
	void UpdateCurrentPositions();

	void FindAffectedTetrahedra(const TArray<FVector>& BladePoints, TArray<FCutTetHit>& OutHits);

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
