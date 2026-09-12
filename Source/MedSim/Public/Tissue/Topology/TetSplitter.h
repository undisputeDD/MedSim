#pragma once

#include "CoreMinimal.h"
#include "Tissue/Topology/TetSplitData.h"

namespace TetSplitter
{
    bool ValidateInput(const FTetSplitInput& Input);

    bool Split(const FTetSplitInput& Input, FTetSplitResult& OutResult);
}