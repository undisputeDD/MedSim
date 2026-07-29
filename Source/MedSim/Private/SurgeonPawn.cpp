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
			FVector CursorPoint = WorldLocation + (WorldDirection * RightHandDepth);

			FVector NewHandLocation = CursorPoint + (RightHandMesh->GetUpVector() * HAND_OFFSET);

			FVector CurrentLocation = RightHandMesh->GetComponentLocation();

			RightHandMesh->SetWorldLocation(FMath::VInterpTo(CurrentLocation, NewHandLocation, DeltaTime, 15.0f));
		}
	}

	if (PhysicsHandle && PhysicsHandle->GetGrabbedComponent())
	{
		FVector HandTipLocation = RightHandMesh->GetComponentLocation() - (RightHandMesh->GetUpVector() * HAND_OFFSET);
		PhysicsHandle->SetTargetLocation(HandTipLocation);
	}
}

void ASurgeonPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("ScrollDepth", this, &ASurgeonPawn::ScrollDepth);

	PlayerInputComponent->BindAction("Grab", IE_Pressed, this, &ASurgeonPawn::GrabObject);
	PlayerInputComponent->BindAction("Grab", IE_Released, this, &ASurgeonPawn::ReleaseObject);
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
					PhysicsHandle->GrabComponentAtLocationWithRotation(HitComp, NAME_None, Start, HitComp->GetComponentRotation());
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
