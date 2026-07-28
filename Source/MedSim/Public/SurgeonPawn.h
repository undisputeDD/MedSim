#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SurgeonPawn.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UPhysicsHandleComponent;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Physics")
	UPhysicsHandleComponent* PhysicsHandle;

	void GrabObject();
	void ReleaseObject();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MedSim|Hands")
	float RightHandDepth = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MedSim|Hands")
	float DepthScrollSpeed = 5.0f;

	void ScrollDepth(float AxisValue);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Camera")
	UCameraComponent* CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Hands")
	UStaticMeshComponent* RightHandMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Hands")
	UStaticMeshComponent* LeftHandMesh;
};
