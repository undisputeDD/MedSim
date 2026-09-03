#pragma once

#include "CoreMinimal.h"
#include "Tissue/Data/TissueCutTypes.h"

namespace CutPath
{
	void BuildOrderedCutPoints(const TArray<FCutTetHit>& CutHits, TArray<FCutPathPoint>& OutPoints);
}