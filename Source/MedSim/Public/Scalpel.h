#pragma once

#include "CoreMinimal.h"
#include "MedicalInstrument.h"
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
	bool bIsCutting = false;

	UPROPERTY()
	ATissueBlock* CurrentTissue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Instrument")
	USplineComponent* BladeEdgeSpline;
};
