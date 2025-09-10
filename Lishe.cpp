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

// Loop needs to execute at least once
static bool loopExecutesAtLeastOnce(Loop *L, ScalarEvolution &SE) {
    unsigned SC = SE.getSmallConstantTripCount(L);
    if (SC >= 1) return true;
    unsigned MaxSC = SE.getSmallConstantMaxTripCount(L);
    if (MaxSC >= 1) return true;
    return false; 
}

PreservedAnalyses LISHEPass::run(Function &F, FunctionAnalysisManager &FAM) {
    // Get analysis from FAM
    auto &LI = FAM.getResult<LoopAnalysis>(F); // Loop structure
    auto &AA = FAM.getResult<AAManager>(F); // Alias analysis
    auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(F); // Trip count

    bool Changed = false; // Did pass change something in IR

    SmallVector<Loop *, 8> Stack(LI.begin(), LI.end()); // Stack with all top-level loops
    while (!Stack.empty()) {
        Loop *L = Stack.pop_back_val();
        Stack.append(L->begin(), L->end()); // For every loop add inner loop if exists

        // Check is there a preheader
        BasicBlock *Preheader = L->getLoopPreheader();
        if (!Preheader) {
            LLVM_DEBUG(dbgs() << "[LISHE] skip: no preheader\n");
            continue;
        }

        // Loop must execute at least once
        if (!loopExecutesAtLeastOnce(L, SE)) {
            LLVM_DEBUG(dbgs() << "[LISHE] skip: not guaranteed to execute >=1\n");
            continue;
        }

        // Finding candidates for hoist
        SmallVector<StoreInst *, 8> Candidates;
        for (BasicBlock *BB : L->blocks()) {
            for (Instruction &I : *BB) {
                auto *SI = dyn_cast<StoreInst>(&I);
                if (!SI) continue;
                if (SI->isVolatile()) continue; // Skip volatile stores

                Value *Ptr = SI->getPointerOperand();
                Value *Val = SI->getValueOperand();

                // Both pointer and value must be loop-invariant
                if (!L->isLoopInvariant(Ptr) || !L->isLoopInvariant(Val))
                    continue;

                // No other stores can modify the same memory location
                if (!noOtherAliasingStoresInLoop(SI, L, AA))
                    continue;

                Candidates.push_back(SI);
            }
        }

    } 

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}