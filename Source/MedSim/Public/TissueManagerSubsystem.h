#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TissueManagerSubsystem.generated.h"

class ATissueBlock;
class UProceduralMeshComponent;

UCLASS()
class MEDSIM_API UTissueManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	void RegisterTissueBlock(ATissueBlock* TissueBlock);

private:
	UFUNCTION()
	void HandleTissueSliced(UProceduralMeshComponent* NewMeshComponent);
};
