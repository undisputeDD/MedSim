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

	// -> Instrument rotation
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* GrabPoint;

	bool bIsRotatingInstrument = false;

	void StartInstrumentRotation();
	void StopInstrumentRotation();
	void RotateInstrumentPitch(float AxisValue);
	void RotateInstrumentYaw(float AxisValue);
	void RotateInstrumentRoll(float AxisValue);
	// Instrument rotation <-

	// -> Initial settings
	void ReturnHand();

	UPROPERTY()
	FRotator InitialRightHandRotation;

	UPROPERTY()
	float InitialRightHandDepth{ 50.0f };

	UPROPERTY()
	float InitialMouseX{};

	UPROPERTY()
	float InitialMouseY{};

	UPROPERTY()
	float SavedMouseX{};

	UPROPERTY()
	float SavedMouseY{};
	// Initial settings <-

	// -> Hand rotation
	FVector TargetHandLocation{};

	UPROPERTY()
	bool bIsRotatingHand{};

	void StartHandRotation();
	void StopHandRotation();

	void RotateHandTwist(float AxisValue);
	void RotateHandTilt(float AxisValue);
	void RotateHandRoll(float AxisValue);
	// Hand rotation <-

	// -> Physics
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Physics")
	UPhysicsHandleComponent* PhysicsHandle;

	void GrabObject();
	void ReleaseObject();
	// Physics <-

	// -> 3D movement modelling
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MedSim|Hands")
	float RightHandDepth{ InitialRightHandDepth };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MedSim|Hands")
	float DepthScrollSpeed{ 3.0f };

	void ScrollDepth(float AxisValue);
	// 3D movement modelling <-

	// -> Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Camera")
	UCameraComponent* CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Hands")
	UStaticMeshComponent* RightHandMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MedSim|Hands")
	UStaticMeshComponent* LeftHandMesh;
	// Components <-
};
