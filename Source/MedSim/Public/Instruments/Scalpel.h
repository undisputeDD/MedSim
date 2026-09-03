#pragma once

#include "CoreMinimal.h"
#include "Instruments/MedicalInstrument.h"
#include "Scalpel.generated.h"

class ATissueBlock;
class USplineComponent;

UCLASS()
class MEDSIM_API AScalpel : public AMedicalInstrument
{
	GENERATED_BODY()
	
public:
	AScalpel();

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

private:
	void PerformCutTrace();

protected:
	UPROPERTY()
	ATissueBlock* CurrentTissue;

	UPROPERTY()
	TArray<AActor*> FoundTissues;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Instrument")
	USplineComponent* BladeEdgeSpline;

private:
	TArray<FVector> PreviousBladePoints;
	bool bHasPreviousBladePoints = false;
};
