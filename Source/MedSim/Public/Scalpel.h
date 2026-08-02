#pragma once

#include "CoreMinimal.h"
#include "MedicalInstrument.h"
#include "Scalpel.generated.h"

/**
 * 
 */
UCLASS()
class MEDSIM_API AScalpel : public AMedicalInstrument
{
	GENERATED_BODY()
	
public:
	AScalpel();

protected:
	virtual void Tick(float DeltaTime) override;

private:
	void PerformCutTrace();
};
