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
namespace llvm {
/**
 * This function overloads operator - to perform subtract over two sets.
 * Note that this function does not change the contents of the two input sets.
 * @param lhs The left input vector.
 * @param rhs The right input vector.
 * @return Resultant vector.
 */
 set<uint32_t> operator-(set<uint32_t> lhs, set<uint32_t> &rhs) {
  for (auto inst : rhs) {
    auto pos = std::find(lhs.begin(), lhs.end(), inst);
    if (pos != lhs.end())
      lhs.erase(pos);
  }
  return lhs;
}

/**
 * This function overloads operator + to perform union over two sets.
 * Note that this function does not change the contents of the two input sets.
 * @param lhs The left input vector.
 * @param rhs The right input vector.
 * @return Resultant vector.
 */
set<uint32_t> operator+(set<uint32_t> lhs, set<uint32_t> &rhs) {
  for (auto &inst : rhs) {
    if (std::find(lhs.begin(), lhs.end(), inst) == lhs.end())
      lhs.insert(inst);
  }
  return lhs;
}

bool isSame(const set<uint32_t> &lhs, const set<uint32_t> &rhs) {
  if (lhs.size() != rhs.size())
    return false;
  for (auto elt : lhs)
    if (std::find(rhs.begin(), rhs.end(), elt) == rhs.end())
      return false;
  return true;
}

/***
 * Compute a list of basic blocks traversed from the entry in post order.
 * @param entry The entry basic block of a function
 * @param visited A set tracking visited basic blocks
 * @param order Return a list of ordered basic blocks.
 */
void computePostOrder2(BasicBlock *entry, set<BasicBlock *> &visited, deque<BasicBlock *> &order) {
  visited.insert(entry);
  auto succItr = succ_begin(entry), end = succ_end(entry);
  for (; succItr != end; ++succItr) {
    if (!visited.count(*succItr))
      computePostOrder2(*succItr, visited, order);
  }
  order.push_back(entry);
}
}

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
  Function *F = BB->getParent();
  if (!ld.getPointerOperand()->hasName())
    return false;

  std::set<uint32_t> defs = bb2In[BB];
  for (auto &inst : *BB) {
    if (&inst == &ld)
      break;
    StoreInst *si;
    if ((si = dyn_cast_or_null<StoreInst>(&inst)) &&
        si->getPointerOperand()->hasName()) {
      for (auto itr = defs.begin(); itr != defs.end(); ) {
        StoreInst *si2;
        if ((si2 = dyn_cast_or_null<StoreInst>(id2Inst[F][*itr])) != nullptr &&
            si2->getPointerOperand()->getName() == si->getPointerOperand()->getName())
          itr = defs.erase(itr);
        else
          ++itr;
      }
      defs.insert(inst2Id[si]);
    }
  }
  for (auto def : defs) {
    StoreInst *si2 = dyn_cast_or_null<StoreInst>(id2Inst[F][def]);
    assert(si2);
    if (si2->getPointerOperand() == ld.getPointerOperand() && dummyDefs[F].count(si2))
      return true;
  }
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

  // Step #1: identify program variables.
  set<Value*> namedDefs;
  for (auto &inst : frontBB) {
    // AllocaInst
    AllocaInst *ai;
    if ((ai = dyn_cast_or_null<AllocaInst>(&inst)) != nullptr)
      namedDefs.insert(ai);
  }

  // Step #2: insert dummy definitions for each local variable
  std::for_each(namedDefs.begin(), namedDefs.end(), [&](Value *ptr) {
    StoreInst *si = new StoreInst(Constant::getNullValue(ptr->getType()->getPointerElementType()), ptr);
    dummyDefs[&F].insert(si);
    frontBB.getInstList().insert(pos, si);
  });

  // Step #3: find out all definitions of namedDefs variables.
  map<Value*, set<uint32_t>> var2Defs;
  unsigned id = 0;
  for (auto &bb : F) {
    for (auto &inst : bb) {
      StoreInst *si;
      if ((si = dyn_cast_or_null<StoreInst>(&inst)) &&
          si->getPointerOperand() != nullptr &&
          namedDefs.find(si->getPointerOperand()) != namedDefs.end()) {
        Value *var = si->getPointerOperand();
        if (!var2Defs.count(var))
          var2Defs[var] = set<uint32_t>();
        assert(!var2Defs[var].count(id));
        var2Defs[si->getPointerOperand()].insert(id);
      }
      id2Inst[&F].push_back(&inst);
      inst2Id[&inst] = id++;
    }
  }

  // Step #4: calculate GEN/KILL set for each basic block
  map<BasicBlock *, set<uint32_t>> bb2Kill, bb2Gen;
  for (auto &bb : F) {
    set<uint32_t> kill;
    set<uint32_t> gen;
    for (auto &inst : bb) {
      StoreInst *si;
      if ((si = dyn_cast_or_null<StoreInst>(&inst)) &&
          si->getPointerOperand() != nullptr &&
          namedDefs.find(si->getPointerOperand()) != namedDefs.end()) {
        // gen
        for (auto itr = gen.begin(); itr != gen.end(); ) {
          StoreInst *si2;
          if ((si2 = dyn_cast_or_null<StoreInst>(id2Inst[&F][*itr])) != nullptr &&
          si2->getPointerOperand()->getName() == si->getPointerOperand()->getName())
            itr = gen.erase(itr);
          else
            ++itr;
        }

        if (!gen.count(inst2Id[si]))
          gen.insert(inst2Id[si]);

        // kill
        set<uint32_t> others = var2Defs[si->getPointerOperand()];
        for (auto def : others) {
          if (def != inst2Id[si])
            kill.insert(def);
        }
      }
    }
    for (auto &value : gen) {
      auto itr = std::find(kill.begin(), kill.end(), value);
      if (itr != kill.end())
        kill.erase(itr);
    }
    bb2Gen[&bb] = gen;
    bb2Kill[&bb] = kill;
  }

  // The set of reaching definitions of the font basic block is empty.
  bb2In[&frontBB] = set<uint32_t>();

  // Step #5: compute post order traversal
  set<BasicBlock *> visited;
  std::deque<BasicBlock *> worklist;
  computePostOrder2(&F.front(), visited, worklist);

  // Step #6: iterate over control flow graph of the input function until the fixed point.
  bool repeat;
  do {
    repeat = false;
    // visit the basic blocks reversely to compute IN/OUT of each basic block
    std::for_each(worklist.rbegin(), worklist.rend(), [&](BasicBlock *bb) {
      // OUT[BB] = (IN[BB] - KILL[BB]) + GEN[BB]
      bb2Out[bb] = (bb2In[bb] - bb2Kill[bb]) + bb2Gen[bb];

      auto &rdOut = bb2Out[bb];
      for (auto succItr = succ_begin(bb), end = succ_end(bb);
           succItr != end; ++succItr) {
        auto old = bb2In[*succItr];
        bb2In[*succItr] = bb2In[*succItr] + rdOut;
        repeat |= !isSame(old, bb2In[*succItr]);
      }
    });
  } while (repeat);

  // Step #7: print out IN and OUT set of each basic block.
  if (enableRDA)
    dumpReachingDef(F);

  // ReachingDefinitions does not change the input function at all.
  return false;
}
