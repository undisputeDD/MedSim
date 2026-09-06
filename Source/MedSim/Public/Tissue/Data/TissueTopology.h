#pragma once

#include "CoreMinimal.h"

struct FTissueVertex
{
	FVector3f RestPosition;
	FVector3f CurrentPosition;

	float Mass = 0.0f;
};

struct FTissueTet
{
	FIntVector4 Vertices;
};

struct FTissueTopologySnapshot
{
	TArray<FTissueVertex> Vertices;
	TArray<FTissueTet> Tetrahedra;
};