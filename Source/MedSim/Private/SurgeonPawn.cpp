#include "SurgeonPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

const float HAND_OFFSET = 30.f;
const float GRAB_DEPTH = 10.f;

ASurgeonPawn::ASurgeonPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(RootComponent);
	CameraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 170.0f));

	RightHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightHandMesh"));
	RightHandMesh->SetupAttachment(CameraComponent);
	RightHandMesh->SetRelativeLocation(FVector(60.0f, 30.0f, -20.0f));
	RightHandMesh->SetCollisionProfileName(TEXT("NoCollision"));

	LeftHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftHandMesh"));
	LeftHandMesh->SetupAttachment(CameraComponent);
	LeftHandMesh->SetRelativeLocation(FVector(60.0f, -30.0f, -20.0f));
	LeftHandMesh->SetCollisionProfileName(TEXT("NoCollision"));

	PhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PhysicsHandle"));
}

void ASurgeonPawn::BeginPlay()
{
	Super::BeginPlay();
	
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;

		FVector LeftHandTipLocation = LeftHandMesh->GetComponentLocation() - (LeftHandMesh->GetUpVector() * HAND_OFFSET);

		LeftHandTipLocation.Y += 60.f;

		FVector2D ScreenPosition;
		if (PC->ProjectWorldLocationToScreen(LeftHandTipLocation, ScreenPosition))
		{
			InitialMouseX = abs(ScreenPosition.X);
			InitialMouseY = abs(ScreenPosition.Y);
		}

		RightHandDepth = InitialRightHandDepth;

		InitialRightHandRotation = RightHandMesh->GetRelativeRotation();
	}
}

void ASurgeonPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FVector WorldLocation, WorldDirection;

		if (PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
		{
			if (!bIsRotatingHand)
			{
				FVector CursorPoint = WorldLocation + (WorldDirection * RightHandDepth);
				TargetHandLocation = CursorPoint + (RightHandMesh->GetUpVector() * HAND_OFFSET);
			}

			FVector CurrentLocation = RightHandMesh->GetComponentLocation();
			RightHandMesh->SetWorldLocation(FMath::VInterpTo(CurrentLocation, TargetHandLocation, DeltaTime, 15.0f));
		}
	}

	if (PhysicsHandle && PhysicsHandle->GetGrabbedComponent())
	{
		FVector HandTipLocation = RightHandMesh->GetComponentLocation() - (RightHandMesh->GetUpVector() * HAND_OFFSET);
		PhysicsHandle->SetTargetLocationAndRotation(HandTipLocation, RightHandMesh->GetComponentRotation());
	}
}

void ASurgeonPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("ScrollDepth", this, &ASurgeonPawn::ScrollDepth);

	PlayerInputComponent->BindAction("Grab", IE_Pressed, this, &ASurgeonPawn::GrabObject);
	PlayerInputComponent->BindAction("Grab", IE_Released, this, &ASurgeonPawn::ReleaseObject);

	PlayerInputComponent->BindAction("RotateAction", IE_Pressed, this, &ASurgeonPawn::StartHandRotation);
	PlayerInputComponent->BindAction("RotateAction", IE_Released, this, &ASurgeonPawn::StopHandRotation);

	PlayerInputComponent->BindAxis("TurnHand", this, &ASurgeonPawn::RotateHandTwist);
	PlayerInputComponent->BindAxis("TiltHand", this, &ASurgeonPawn::RotateHandTilt);
	PlayerInputComponent->BindAxis("RollHand", this, &ASurgeonPawn::RotateHandRoll);

	PlayerInputComponent->BindAction("ReturnHand", IE_Pressed, this, &ASurgeonPawn::ReturnHand);
}

void ASurgeonPawn::ReturnHand()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetMouseLocation(FMath::RoundToInt(InitialMouseX), FMath::RoundToInt(InitialMouseY));

		RightHandDepth = InitialRightHandDepth;

		RightHandMesh->SetRelativeRotation(InitialRightHandRotation);
	}
}

void ASurgeonPawn::GrabObject()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FVector WorldLocation, WorldDirection;
		if (PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
		{
			FVector Start = RightHandMesh->GetComponentLocation() - (RightHandMesh->GetUpVector() * HAND_OFFSET);
			FVector End = Start + (WorldDirection * GRAB_DEPTH);

			FHitResult HitResult;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(this);

			if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params))
			{
				UPrimitiveComponent* HitComp = HitResult.GetComponent();

				if (HitComp && HitComp->IsSimulatingPhysics())
				{
					PhysicsHandle->GrabComponentAtLocationWithRotation(HitComp, NAME_None, HitResult.ImpactPoint, HitComp->GetComponentRotation());
				}
			}
		}
	}
}

void ASurgeonPawn::ReleaseObject()
{
	if (PhysicsHandle && PhysicsHandle->GetGrabbedComponent())
	{
		PhysicsHandle->ReleaseComponent();
	}
}

void ASurgeonPawn::ScrollDepth(float AxisValue)
{
	if (AxisValue != 0.0f)
	{
		RightHandDepth += AxisValue * DepthScrollSpeed;
		RightHandDepth = FMath::Clamp(RightHandDepth, 20.0f, 300.0f);
	}
}

void ASurgeonPawn::StartHandRotation()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->GetMousePosition(SavedMouseX, SavedMouseY);
	}

	bIsRotatingHand = true;
}

void ASurgeonPawn::StopHandRotation()
{
	bIsRotatingHand = false;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetMouseLocation(FMath::RoundToInt(SavedMouseX), FMath::RoundToInt(SavedMouseY));
	}
}

void ASurgeonPawn::RotateHandTwist(float AxisValue)
{
	if (bIsRotatingHand && AxisValue != 0.0f)
	{
		RightHandMesh->AddLocalRotation(FRotator(0.0f, AxisValue * 3.0f, 0.0f));
	}
}

void ASurgeonPawn::RotateHandTilt(float AxisValue)
{
	if (bIsRotatingHand && AxisValue != 0.0f)
	{
		RightHandMesh->AddLocalRotation(FRotator(AxisValue * 3.0f, 0.0f, 0.0f));
	}
}

void ASurgeonPawn::RotateHandRoll(float AxisValue)
{
	if (bIsRotatingHand && AxisValue != 0.0f)
	{
		RightHandMesh->AddLocalRotation(FRotator(0.0f, 0.0f, AxisValue * 3.0f));
	}
}