/**
 * USC Compiler
 * Jianping Zeng (zeng207@purdue.edu)
 * This file implements a LLVM pass to identify and report the variables used before defined.
*/

#ifndef USCC_OPT_REACHINGDEFINITIONS_H
#define USCC_OPT_REACHINGDEFINITIONS_H

#include "Passes.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Instructions.h"
#include <set>
#include <map>

namespace llvm {
class ReachingDefinitions : public FunctionPass {
private:
  // map instruction to its zero-based id
  std::map<Instruction*, uint32_t> inst2Id;
  std::map<Function*, std::vector<Instruction*>> id2Inst;
  // IN[BB] and OUT[BB]
  std::map<BasicBlock *, std::set<uint32_t>> bb2In;
  std::map<BasicBlock *, std::set<uint32_t>> bb2Out;
  std::map<Function*, std::set<StoreInst*>> dummyDefs;
  void dumpReachingDef(Function &F);
  void postOrderTraversal(llvm::BasicBlock *current, std::vector<llvm::BasicBlock*> &visited, std::vector<llvm::BasicBlock*> &preorderVisited);
public:
  static char ID;
  ReachingDefinitions();
  bool runOnFunction(llvm::Function &F) override;
  void releaseMemory() override {
    inst2Id.clear();
    id2Inst.clear();
    bb2In.clear();
    bb2Out.clear();
    dummyDefs.clear();
  }
  /**
   * Check if the given load has reaching definitions.
   * @param ld
   * @return true if the program variable read by the load has reaching definitions.
   */
  bool hasUninitializedDef(LoadInst &ld);
  bool isDummyStore(Instruction &inst);
};
}

#endif //USCC_OPT_REACHINGDEFINITIONS_H
