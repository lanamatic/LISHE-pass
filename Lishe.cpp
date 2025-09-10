#include "Lishe.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "lishe"

// Elimination of multiple identical stores inside the loop
static constexpr bool EnableRedundantStoreElim = true;

// Ensure no other stores can modify the same memory location
static bool noOtherAliasingStoresInLoop(StoreInst *SI, Loop *L, AAResults &AA) {
    // Skip volatile stores
    if (SI->isVolatile()) return false;
    MemoryLocation Loc = MemoryLocation::get(SI);
    
    for (BasicBlock *BB : L->blocks()) {
        for (Instruction &I : *BB) {
            if (&I == SI) continue;
            if (!I.mayWriteToMemory()) continue;

            // If the instruction may write to the same memory location → unsafe to hoist
            ModRefInfo MRI = AA.getModRefInfo(&I, Loc);
            if (isModSet(MRI)) {
                LLVM_DEBUG(dbgs() << "  aliasing writer found: " << I << "\n");
                return false;
            }
        }
    }
    return true;
}