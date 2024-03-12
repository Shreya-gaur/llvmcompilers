//
//  LocalConstantPropagation.cpp
//  uscc
//
//  Implements constant propagation --
//  Current ConstantOps is more like constant folding optimization.
//  This constantPropagation doesn't assume phi node.
//
#include "Passes.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#pragma clang diagnostic pop
#include <set>
#include <list>
#include <unordered_map>

using namespace llvm;

namespace uscc
{
namespace opt
{

// is fromBB predecessor of toBB?
bool isPredecessor(BasicBlock* fromBB, BasicBlock* toBB) {
  std::list<BasicBlock*> BBList;
  std::set<BasicBlock*> visited;
  BBList.push_back(fromBB); 
  while (!BBList.empty()){
    BasicBlock* currBB = *BBList.begin();
    BBList.erase(BBList.begin());
    visited.insert(currBB);
    // 1) current store overwrites the definition from the previous basic block -> this is ok
    if (currBB == toBB) {
      return true;
    }
    for (succ_iterator succ = succ_begin(currBB), succ_endBB = succ_end(currBB); succ != succ_endBB; ++succ) {
      if (visited.find(*succ) == visited.end())
        BBList.push_back(*succ);
    }
  }
  // 2) Current store overwrites the definition from other successor blocks of the current block's predecessor -> cannot be constant
  return false;
}

bool LocalConstantPropagation::runOnFunction(Function& F) {
  bool changed = false;
  // constantDef = map = { (x.addr, constant0) ... }
  std::unordered_map<llvm::Value*, llvm::ConstantInt*> constantDef;
  // %x = load (x.addr)
  // constantLoad = map = { (%x, constant0) ... }
  std::unordered_map<llvm::Value*, llvm::ConstantInt*> constantLoad;

  // Traversed blocks for the optimization
  std::list<BasicBlock*> WorkList;
  // All the variables (allocaInst)
  std::set<llvm::Value*> Defs;
  std::set<llvm::Instruction*> RemoveSet;
  std::set<BasicBlock*> visited;

  WorkList.push_back(F.begin());
  while (!WorkList.empty()) {
    constantDef.clear();
    constantLoad.clear();
    BasicBlock* currBB = *WorkList.begin();
    WorkList.erase(WorkList.begin());
    visited.insert(currBB);
    for (auto &I : *currBB) {
      if (AllocaInst* allocaInst = dyn_cast<llvm::AllocaInst>(&I)) {
        Defs.insert(dyn_cast<llvm::Value>(allocaInst));
      }
      else if (StoreInst* storeInst = dyn_cast<llvm::StoreInst>(&I)) {
        if (ConstantInt* constValue = dyn_cast<ConstantInt>(storeInst->getOperand(0))) {
          if (constValue->getValue().isSignedIntN(32) && Defs.find(storeInst->getOperand(1)) != Defs.end()) {
            constantDef[storeInst->getOperand(1)] = constValue;
          }
        }
      }
      else if (LoadInst* loadInst = dyn_cast<llvm::LoadInst>(&I)) {
        if (constantDef.find(loadInst->getOperand(0)) != constantDef.end()) {
          ConstantInt* constValue = constantDef[I.getOperand(0)];
          constantLoad[dyn_cast<llvm::Value>(loadInst)] = constValue;
          RemoveSet.insert(loadInst);
        }
      }
      else if (I.getNumOperands() > 0) {
        for (int i = 0; i < I.getNumOperands(); i++) {
          if (constantLoad.find(I.getOperand(i)) != constantLoad.end()) {
            I.setOperand(i, constantLoad[I.getOperand(i)]);
            changed = true;
          }
        }
      }
    }
    for (succ_iterator succ = succ_begin(currBB), succ_endBB = succ_end(currBB); succ != succ_endBB; ++succ) {
      if (visited.find(*succ) == visited.end())
        WorkList.push_back(*succ);
    }
  }

  for (auto & I : RemoveSet) {
    I->eraseFromParent();
  }
  return changed;
}

void LocalConstantPropagation::getAnalysisUsage(AnalysisUsage& Info) const
{
	// This pass does not alter the CFG
	Info.setPreservesCFG();
}

} // opt
} // uscc

char uscc::opt::LocalConstantPropagation::ID = 0;
