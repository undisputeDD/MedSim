#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tissue/Data/TissueTopology.h"
#include "TissueBlock.generated.h"

class UFleshComponent;
class UDeformableSolverComponent;
class UDeformableCollisionsComponent;

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
