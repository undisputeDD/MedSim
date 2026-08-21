#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PBD_Simulator.generated.h"

USTRUCT()
struct FParticle
{
	GENERATED_BODY()

	FVector Position = FVector::ZeroVector;
	FVector PredictedPosition = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float InverseMass = 1.0f;
};

UCLASS()
class MEDSIM_API APBD_Simulator : public AActor
{
	GENERATED_BODY()
	
public:	
	APBD_Simulator();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

protected:
	UPROPERTY(EditAnywhere, Category = "MedSim|PBDSettings")
	int32 SolverIterations = 5;

	UPROPERTY(EditAnywhere, Category = "MedSim|PBDSettings")
	float RestLength = 100.0f;

	UPROPERTY(EditAnywhere, Category = "MedSim|PBDSettings")
	FVector Gravity = FVector(0.0f, 0.0f, -980.0f);

	TArray<FParticle> Particles;
	
	bool bIsCut = false;
};
