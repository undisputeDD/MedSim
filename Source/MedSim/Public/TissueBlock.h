#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	void ApplyCut(const TArray<FVector>& BladePoints);

protected:
	virtual void BeginPlay() override;

public:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Tissue")
	UFleshComponent* FleshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Tissue")
	UDeformableSolverComponent* DeformableSolverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Tissue")
	UDeformableCollisionsComponent* DeformableCollisionsComponent;
};
