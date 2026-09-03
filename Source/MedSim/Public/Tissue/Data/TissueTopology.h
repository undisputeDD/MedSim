#pragma once

#include "CoreMinimal.h"
#include "TissueTopology.generated.h"

USTRUCT()
struct FTissueVertex
{
	GENERATED_BODY()

public:
	FVector3f RestPosition;
	FVector3f CurrentPosition;

	float Mass = 0.0f;
};

USTRUCT()
struct FTissueTet
{
	GENERATED_BODY()

	FIntVector4 Vertices;
};

USTRUCT()
struct FTissueTopologySnapshot
{
	GENERATED_BODY()

	TArray<FTissueVertex> Vertices;
	TArray<FTissueTet> Tetrahedra;
};