#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SurgeonPawn.generated.h"

class UCameraComponent;
class UStaticMeshComponent;

UCLASS()
class MEDSIM_API ASurgeonPawn : public APawn
{
	GENERATED_BODY()

public:
	ASurgeonPawn();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Camera")
	UCameraComponent* CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Hands")
	UStaticMeshComponent* RightHandMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Hands")
	UStaticMeshComponent* LeftHandMesh;
};
