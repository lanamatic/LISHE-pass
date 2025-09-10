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
static bool noOtherAliasingStoresInLoop(StoreInst *SI, Loop *L, AAResults &AA)
{
    // Skip volatile stores
    if (SI->isVolatile())
        return false;
    MemoryLocation Loc = MemoryLocation::get(SI);

    for (BasicBlock *BB : L->blocks())
    {
        for (Instruction &I : *BB)
        {
            if (&I == SI)
                continue;
            if (!I.mayWriteToMemory())
                continue;

            // If the instruction may write to the same memory location → unsafe to hoist
            ModRefInfo MRI = AA.getModRefInfo(&I, Loc);
            if (isModSet(MRI))
            {
                LLVM_DEBUG(dbgs() << "  aliasing writer found: " << I << "\n");
                return false;
            }
        }
    }
    return true;
}

// Loop needs to execute at least once
static bool loopExecutesAtLeastOnce(Loop *L, ScalarEvolution &SE)
{
    unsigned SC = SE.getSmallConstantTripCount(L);
    if (SC >= 1)
        return true;
    unsigned MaxSC = SE.getSmallConstantMaxTripCount(L);
    if (MaxSC >= 1)
        return true;
    return false;
}

PreservedAnalyses LISHEPass::run(Function &F, FunctionAnalysisManager &FAM)
{
    // Get analysis from FAM
    auto &LI = FAM.getResult<LoopAnalysis>(F);            // Loop structure
    auto &AA = FAM.getResult<AAManager>(F);               // Alias analysis
    auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(F); // Trip count

    bool Changed = false; // Did pass change something in IR

    SmallVector<Loop *, 8> Stack(LI.begin(), LI.end()); // Stack with all top-level loops
    while (!Stack.empty())
    {
        Loop *L = Stack.pop_back_val();
        Stack.append(L->begin(), L->end()); // For every loop add inner loop if exists

        // Check is there a preheader
        BasicBlock *Preheader = L->getLoopPreheader();
        if (!Preheader)
        {
            LLVM_DEBUG(dbgs() << "[LISHE] skip: no preheader\n");
            continue;
        }

        // Loop must execute at least once
        if (!loopExecutesAtLeastOnce(L, SE))
        {
            LLVM_DEBUG(dbgs() << "[LISHE] skip: not guaranteed to execute >=1\n");
            continue;
        }

        // Redudant Elimination
        if (EnableRedundantStoreElim)
        {
            for (BasicBlock *BB : L->blocks())
            {
                StoreInst *LastStore = nullptr;
                Value *LastPtr = nullptr;
                Value *LastVal = nullptr;

                auto resetTracker = [&]()
                {
                    LastStore = nullptr;
                    LastPtr = nullptr;
                    LastVal = nullptr;
                };

                for (auto It = BB->begin(); It != BB->end();)
                {
                    Instruction &I = *It++;
                    // If we have a tracked store and encounter any write that may clobber it, reset.
                    if (!isa<StoreInst>(&I) && LastStore && I.mayWriteToMemory())
                    {
                        MemoryLocation Loc = MemoryLocation::get(LastStore);
                        ModRefInfo MRI = AA.getModRefInfo(&I, Loc);
                        if (isModSet(MRI))
                        {
                            resetTracker();
                        }
                    }

                    auto *SI = dyn_cast<StoreInst>(&I);
                    if (!SI)
                        continue;

                    // Never touch atomic/volatile in this peephole
                    if (SI->isVolatile() || SI->isAtomic())
                    {
                        resetTracker();
                        continue;
                    }

                    Value *Ptr = SI->getPointerOperand();
                    Value *Val = SI->getValueOperand();

                    if (LastStore && Ptr == LastPtr && Val == LastVal)
                    {
                        LLVM_DEBUG(dbgs() << "[LISHE] eliminate redundant store: " << *SI << "\n");
                        SI->eraseFromParent();
                        Changed = true;
                        // Tracker continues to refer to the earlier store.
                        continue;
                    }

                    if (LastStore)
                    {
                        MemoryLocation Loc = MemoryLocation::get(LastStore);
                        if (isModSet(AA.getModRefInfo(SI, Loc)))
                        {
                            resetTracker();
                        }
                    }

                    // Begin tracking this store; any clobber resets later.
                    LastStore = SI;
                    LastPtr = Ptr;
                    LastVal = Val;
                    continue;
                }
            }
        }

        // Finding candidates for hoist
        SmallVector<StoreInst *, 8> Candidates;
        for (BasicBlock *BB : L->blocks())
        {
            for (Instruction &I : *BB)
            {
                auto *SI = dyn_cast<StoreInst>(&I);
                if (!SI)
                    continue;
                if (SI->isVolatile())
                    continue; // Skip volatile stores

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

        if (Candidates.empty())
            continue;

        // Hoisting
        IRBuilder<> B(Preheader->getTerminator());
        for (StoreInst *SI : Candidates)
        {
            // If there is already an identical store in preheader, skip adding a new one
            bool PreheaderHasSame = false;
            for (Instruction &PI : *Preheader)
            {
                if (auto *PS = dyn_cast<StoreInst>(&PI))
                {
                    if (!PS->isVolatile() && PS->getPointerOperand() == SI->getPointerOperand() &&
                        PS->getValueOperand() == SI->getValueOperand())
                    {
                        PreheaderHasSame = true;
                        break;
                    }
                }
            }

            if (!PreheaderHasSame)
            {
                // Create a new store in preheader, copy relevant attributes
                auto *NewS = B.CreateStore(SI->getValueOperand(), SI->getPointerOperand());
                NewS->setAlignment(SI->getAlign());
                NewS->setVolatile(SI->isVolatile());
                NewS->setOrdering(SI->getOrdering());
                NewS->setSyncScopeID(SI->getSyncScopeID());
                LLVM_DEBUG(dbgs() << "[LISHE] hoist: " << *SI << "\n");
            }
            else
            {
                LLVM_DEBUG(dbgs() << "[LISHE] preheader already has same store\n");
            }

            // Delete original store from the loop
            SI->eraseFromParent();
            Changed = true;
        }
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

// Plugin registration
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo()
{
    return {LLVM_PLUGIN_API_VERSION, "LISHEPass", "0.1", [](PassBuilder &PB)
            {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager &FPM, ArrayRef<PassBuilder::PipelineElement>)
                    {
                        if (Name == "lishe")
                        {
                            FPM.addPass(LISHEPass());
                            return true;
                        }
                        return false;
                    });
            }};
}