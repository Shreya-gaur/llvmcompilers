/**
 * USC Compiler
 * Jianping Zeng (zeng207@purdue.edu)
 * An iterative forward reaching definition analysis.
 * This pass intends to compute a set of reaching definitions for each LLVM instruction,
 * which is used by UnintializedVariables pass.
*/

#include "Passes.h"
#include "ReachingDefinitions.h"

using namespace std;
using namespace llvm;

bool enableRDA;

char ReachingDefinitions::ID = 0;
INITIALIZE_PASS(ReachingDefinitions, "rda", "reaching definition analysis", true, true)

ReachingDefinitions::ReachingDefinitions() : FunctionPass(ID), inst2Id(), id2Inst(), bb2In(), bb2Out() {
  initializeReachingDefinitionsPass(*PassRegistry::getPassRegistry());
}

FunctionPass *llvm::createRDAPass() {
  return new ReachingDefinitions();
}

bool ReachingDefinitions::hasUninitializedDef(LoadInst &ld) {
  BasicBlock *BB = ld.getParent();
  assert(BB && bb2In.count(BB) && inst2Id.count(&ld) && "load must be attached to a basic block!");
  if (!ld.getPointerOperand()->hasName())
    return false;

  // PA3
  return false;
}

void ReachingDefinitions::dumpReachingDef(Function &F) {
  outs() << "Reaching definition analysis for Function '" << F.getName() << "':\n";
  int id = 0;
  for (auto &bb : F) {
    outs()<<bb.getName()<<":\n";
    for (Instruction& inst : bb) {
      outs() << id++ << " " << inst;
      if (std::find(dummyDefs[&F].begin(), dummyDefs[&F].end(), &inst) != dummyDefs[&F].end())
        outs()<<"; DUMMY";
      outs() << "\n";
    }
    outs() << "\n";
  }

  for (auto &bb : F) {
    outs() << bb.getName() << ":\n";
    outs() << "  IN:";
    for (auto &id : bb2In[&bb])
      outs() << " " << id;
    outs() << "\n";
    outs() << "  OUT:";
    for (auto &id : bb2Out[&bb])
      outs() << " " << id;
    outs() << "\n";
  }
  outs()<<"\n";
}

bool ReachingDefinitions::isDummyStore(Instruction &inst) {
  StoreInst *si;
  return inst.getParent() && inst.getParent()->getParent() &&
      (si = dyn_cast_or_null<StoreInst>(&inst)) &&
      dummyDefs[inst.getParent()->getParent()].count(si);
}

bool ReachingDefinitions::runOnFunction(Function &F) {
  if (F.empty())
    return false;
  BasicBlock &frontBB = F.front();
  BasicBlock &endBB = F.back();
  assert(!frontBB.empty() && !endBB.empty() && "the front/end basic block must not be empty!");

  // Move all AllocaInst to be at the beginning of the front basic block for easy implementation
  auto pos = frontBB.begin();
  while (pos != frontBB.end() && dyn_cast_or_null<AllocaInst>(&*pos))
    ++pos;

  auto Iter = pos;
  while (Iter != frontBB.end()) {
    if (dyn_cast_or_null<AllocaInst>(&*Iter))
      Iter->moveBefore(pos);
    ++Iter;
  }

  // PA3
  // Step #1: identify program variables.
  set<Value*> namedDefs;

  // Step #2: insert dummy definitions for each local variable

  // Step #3: find out all definitions of namedDefs variables.
  map<Value*, set<uint32_t>> var2Defs;

  // Step #4: calculate GEN/KILL set for each basic block
  map<BasicBlock *, set<uint32_t>> bb2Kill, bb2Gen;

  // Step #5: compute post order traversal

  // Step #6: iterate over control flow graph of the input function until the fixed point.

  // Step #7: print out IN and OUT set of each basic block.
  if (enableRDA)
    dumpReachingDef(F);

  // ReachingDefinitions does not change the input function at all.
  return false;
}
