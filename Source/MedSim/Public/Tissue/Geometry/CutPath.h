#pragma once

#include "CoreMinimal.h"
#include "Tissue/Data/TissueCutTypes.h"

namespace CutPath
{
	// DEPRECATED
	// Reconstructs cut path from individual blade trajectories.
	// Not used by final swept-surface cut geometry.
	void BuildOrderedCutPoints(const TArray<FCutTetHit>& CutHits, TArray<FCutPathPoint>& OutPoints);
}