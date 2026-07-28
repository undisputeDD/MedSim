#include "SurgeonPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"

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
			FVector NewHandLocation = WorldLocation + (WorldDirection * RightHandDepth);

			FVector CurrentLocation = RightHandMesh->GetComponentLocation();
			RightHandMesh->SetWorldLocation(FMath::VInterpTo(CurrentLocation, NewHandLocation, DeltaTime, 15.0f));
		}
	}
}

void ASurgeonPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("ScrollDepth", this, &ASurgeonPawn::ScrollDepth);
}

void ASurgeonPawn::ScrollDepth(float AxisValue)
{
	if (AxisValue != 0.0f)
	{
		RightHandDepth += AxisValue * DepthScrollSpeed;
		RightHandDepth = FMath::Clamp(RightHandDepth, 20.0f, 300.0f);
	}
}
