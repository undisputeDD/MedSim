#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TissueBlock.generated.h"

class UDynamicMeshComponent;
class UStaticMeshComponent;
class USplineComponent;

USTRUCT(BlueprintType)
struct FIncisionPoint
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Location;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector SurfaceNormal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Depth;

	FIncisionPoint()
		: Location(FVector::ZeroVector), SurfaceNormal(FVector::UpVector), Depth(0.0f) {}

	FIncisionPoint(FVector InLoc, FVector InNormal, float InDepth)
		: Location(InLoc), SurfaceNormal(InNormal), Depth(InDepth) {}
};

UCLASS()
class MEDSIM_API ATissueBlock : public AActor
{
	GENERATED_BODY()
	
public:	
	ATissueBlock();

	void AddIncisionPoint(FVector HitLocation, FVector HitNormal, float CutDepth);

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Tissue")
	UStaticMeshComponent* BaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Tissue")
	UDynamicMeshComponent* DynamicMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Data")
	TArray<FIncisionPoint> CurrentIncisionPath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Tissue")
	USplineComponent* IncisionSpline;
};
