#include "MedicalInstrument.h"
#include "Components/StaticMeshComponent.h"

AMedicalInstrument::AMedicalInstrument()
{
 	PrimaryActorTick.bCanEverTick = false;

	InstrumentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InstrumentMesh"));
	RootComponent = InstrumentMesh;

	InstrumentMesh->SetSimulatePhysics(true);
	InstrumentMesh->SetCollisionProfileName(TEXT("PhysicsActor"));

	InstrumentMesh->SetLinearDamping(0.5f);
	InstrumentMesh->SetAngularDamping(0.5f);
}

void AMedicalInstrument::BeginPlay()
{
	Super::BeginPlay();
	
}
