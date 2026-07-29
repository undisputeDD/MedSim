#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MedicalInstrument.generated.h"

class UStaticMeshComponent;

UCLASS()
class MEDSIM_API AMedicalInstrument : public AActor
{
	GENERATED_BODY()
	
public:	
	AMedicalInstrument();

protected:
	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Instrument")
	UStaticMeshComponent* InstrumentMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Instrument")
	bool bIsHeld{false};
};
