#include "MedicalInstrument.h"
#include "Components/StaticMeshComponent.h"

AMedicalInstrument::AMedicalInstrument()
{
 	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	InstrumentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InstrumentMesh"));
	InstrumentMesh->SetupAttachment(RootComponent);

	InstrumentMesh->SetSimulatePhysics(true);
	InstrumentMesh->SetCollisionProfileName(TEXT("PhysicsActor"));

	InstrumentMesh->SetLinearDamping(0.5f);
	InstrumentMesh->SetAngularDamping(0.5f);
}

void AMedicalInstrument::SetHeldStatus(bool bStatus, AActor* NewOwner)
{
	bIsHeld = bStatus;

	if (bStatus && NewOwner)
	{
		SetOwner(NewOwner);
	}
	else
	{
		SetOwner(nullptr);
	}
}

void AMedicalInstrument::BeginPlay()
{
	Super::BeginPlay();
	
}
