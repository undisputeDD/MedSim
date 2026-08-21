#include "PBD_Simulator.h"

APBD_Simulator::APBD_Simulator()
{
 	PrimaryActorTick.bCanEverTick = true;

}

void APBD_Simulator::BeginPlay()
{
	Super::BeginPlay();
	
    Particles.Empty();

    FParticle P0;
    P0.Position = GetActorLocation();
    P0.InverseMass = 0.0f;
    Particles.Add(P0);

    FParticle P1;
    P1.Position = GetActorLocation() + FVector(0, 500.f, -5.f);
    P1.InverseMass = 1.0f;
    Particles.Add(P1);
}

void APBD_Simulator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    if (Particles.Num() < 2) return;

    if (GetGameTimeSinceCreation() > 3.0f && !bIsCut)
    {
        bIsCut = true;
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("СКАЛЬПЕЛЬ! Пружина разрезана!"));
    }

    float dt = FMath::Clamp(DeltaTime, 0.01f, 0.05f);

    for (FParticle& P : Particles)
    {
        if (P.InverseMass > 0.0f)
        {
            P.Velocity += Gravity * dt;
        }
    }

    for (FParticle& P : Particles)
    {
        P.PredictedPosition = P.Position + P.Velocity * dt;
    }

    for (int32 Iter = 0; Iter < SolverIterations; Iter++)
    {
        if (bIsCut) break;

        FParticle& P0 = Particles[0];
        FParticle& P1 = Particles[1];

        FVector Direction = P1.PredictedPosition - P0.PredictedPosition;
        float CurrentLength = Direction.Size();

        float Error = CurrentLength - RestLength;

        if (CurrentLength > 0.0001f)
        {
            FVector Gradient = Direction / CurrentLength;

            float TotalInverseMass = P0.InverseMass + P1.InverseMass;

            if (TotalInverseMass > 0.0f)
            {
                float Stiffness = 0.005f;

                FVector DeltaP0 = (P0.InverseMass / TotalInverseMass) * Error * Gradient * Stiffness;
                FVector DeltaP1 = -(P1.InverseMass / TotalInverseMass) * Error * Gradient * Stiffness;

                P0.PredictedPosition += DeltaP0;
                P1.PredictedPosition += DeltaP1;
            }
        }
    }

    for (FParticle& P : Particles)
    {
        P.Velocity = (P.PredictedPosition - P.Position) / dt;
        P.Position = P.PredictedPosition;
    }

    DrawDebugSphere(GetWorld(), Particles[0].Position, 10.0f, 16, FColor::Red, false, -1, 0, 1.0f);
    DrawDebugSphere(GetWorld(), Particles[1].Position, 10.0f, 16, FColor::Green, false, -1, 0, 1.0f);
    DrawDebugLine(GetWorld(), Particles[0].Position, Particles[1].Position, FColor::White, false, -1, 0, 2.0f);
}

