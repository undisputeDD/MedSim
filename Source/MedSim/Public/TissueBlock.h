#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TissueBlock.generated.h"

class UDynamicMeshComponent;
class UStaticMeshComponent;

UCLASS()
class MEDSIM_API ATissueBlock : public AActor
{
	GENERATED_BODY()
	
public:	
	ATissueBlock();

	void MakeIncision(FVector CutLocation, FVector CutNormal);

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Tissue")
	UStaticMeshComponent* BaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Tissue")
	UDynamicMeshComponent* DynamicMeshComponent;
};
