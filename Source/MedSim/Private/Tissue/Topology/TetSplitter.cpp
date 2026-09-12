#include "Tissue/Topology/TetSplitter.h"

namespace TetSplitter
{
    bool ValidateInput(const FTetSplitInput& Input)
    {
        if (Input.TetId == INDEX_NONE)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "TetSplitter::ValidateInput: "
                    "invalid TetId"
                )
            );

            return false;
        }

        if (!Input.CutSurface.IsValid())
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "TetSplitter::ValidateInput: "
                    "invalid cut surface"
                )
            );

            return false;
        }

        if (Input.CutBoundary.Vertices.Num() == 0)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "TetSplitter::ValidateInput: "
                    "empty cut boundary vertices"
                )
            );

            return false;
        }

        if (Input.CutSurface.TetId != Input.TetId)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "TetSplitter::ValidateInput: "
                    "TetId mismatch. "
                    "Input=%d Surface=%d"
                ),
                Input.TetId,
                Input.CutSurface.TetId
            );

            return false;
        }

        return true;
    }

    bool Split(const FTetSplitInput& Input, FTetSplitResult& OutResult)
    {
        OutResult = FTetSplitResult{};

        if (!ValidateInput(Input))
        {
            return false;
        }

        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "TetSplitter::Split: "
                "topology split algorithm "
                "not implemented yet. Tet=%d"
            ),
            Input.TetId
        );

        return false;
    }
}