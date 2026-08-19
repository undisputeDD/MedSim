#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TissueBlock.generated.h"

class UDynamicMeshComponent;
class UStaticMeshComponent;
class USplineComponent;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector2D UVCoordinate;

	FIncisionPoint()
		: Location(FVector::ZeroVector), SurfaceNormal(FVector::UpVector), Depth(0.0f), UVCoordinate(FVector2D::ZeroVector) {}

	FIncisionPoint(FVector InLoc, FVector InNormal, float InDepth, FVector2D InUV)
		: Location(InLoc), SurfaceNormal(InNormal), Depth(InDepth), UVCoordinate(InUV) {}
};

UCLASS()
class MEDSIM_API ATissueBlock : public AActor
{
	GENERATED_BODY()
	
public:	
	ATissueBlock();

	void AddIncisionPoint(FVector HitLocation, FVector HitNormal, float CutDepth, FVector2D HitUV);

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Shaders")
	UTextureRenderTarget2D* CutMaskRenderTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Shaders")
	UMaterialInstanceDynamic* DynamicTissueMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "MedSim|Shaders")
	UMaterialInterface* BrushMaterialClass;
};
