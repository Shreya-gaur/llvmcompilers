/**
 * USC Compiler
 * Jianping Zeng (zeng207@purdue.edu)
 * This file implements a LLVM pass to identify and report the variables used before defined.
*/
#include "Passes.h"
#include "ReachingDefinitions.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Module.h"

using namespace llvm;
using namespace std;

namespace llvm {
void initializeUninitializedVariablesPass(PassRegistry&);
}

namespace {
class UninitializedVariables : public FunctionPass {
public:
  static char ID;
  UninitializedVariables() : FunctionPass(ID) {
    initializeUninitializedVariablesPass(*PassRegistry::getPassRegistry());
  }
  bool runOnFunction(llvm::Function &F) override;
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
};
}

char UninitializedVariables::ID = 0;
INITIALIZE_PASS(UninitializedVariables, "uninitialized-vars", "Uninitialized definitions identification", true, true)

void UninitializedVariables::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequired<ReachingDefinitions>();
  AU.addPreserved<ReachingDefinitions>();
  // This function call indicates UninitializedVariables pass does not:
  // (1) add/delete basic blocks;
  // (2) remove terminator instruction at the end of each basic block.
  AU.setPreservesCFG();
}

FunctionPass *llvm::createUninitializedVariablesPass() {
  return new UninitializedVariables();
}

bool UninitializedVariables::runOnFunction(llvm::Function &F) {
  if (F.empty()) return false;
  ReachingDefinitions &rda = getAnalysis<ReachingDefinitions>();

  // PA3
  return false;
}
