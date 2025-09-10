#pragma once
#include "llvm/IR/PassManager.h"

namespace llvm
{

/// Loop-Invariant Store Hoisting/Elimination (LISHE)
struct LISHEPass : public PassInfoMixin<LISHEPass>
{
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

} // namespace llvm
