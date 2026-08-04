#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TissueBlock.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTissueSlicedSignature, UProceduralMeshComponent*, NewMeshComponent);

UCLASS()
class MEDSIM_API ATissueBlock : public AActor
{
	GENERATED_BODY()
	
public:	
	ATissueBlock();

	void SliceTissue(class UPrimitiveComponent* HitComponent, FVector SliceLocation, FVector SliceNormal);

	UPROPERTY(BlueprintAssignable, Category = "MedSim|Events")
	FOnTissueSlicedSignature OnTissueSliced;

	UPROPERTY()
	bool bIsSlicedPiece = false;

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Tissue")
	UStaticMeshComponent* BaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Tissue")
	UProceduralMeshComponent* ProceduralMesh;
};
