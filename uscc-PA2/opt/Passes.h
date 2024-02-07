//
//  Passes.h
//  uscc
//
//  Declares the opt passes supported by USCC
//
//  At the moment, there are four passes:
//     * Constant op removal
//     * Constant branch folding
//     * Removal of dead blocks from CFG
//     * Loop Invariant Code Motion (LICM)
//
//  These passes will execute if uscc is ran with -O
//
//---------------------------------------------------------
//  Copyright (c) 2014, Sanjay Madhav
//  All rights reserved.
//
//  This file is distributed under the BSD license.
//  See LICENSE.TXT for details.
//---------------------------------------------------------
#pragma once
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <llvm/Pass.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Analysis/CFGPrinter.h>
#pragma clang diagnostic pop
#include <vector>
#include<set>


using llvm::FunctionPass;
using llvm::LoopPass;

namespace uscc
{

namespace opt
{

// Helper function for registering the opt passes
void registerOptPasses(llvm::legacy::PassManager& pm);
void registerNaturalLoopPasses(llvm::legacy::PassManager& pm);


struct LocalConstantPropagation : public FunctionPass
{
	static char ID;
	LocalConstantPropagation() : FunctionPass(ID) {}
	
	virtual bool runOnFunction(llvm::Function& F) override;
	
	virtual void getAnalysisUsage(llvm::AnalysisUsage& Info) const override;
};

// Declares the Constant Folding Pass
struct ConstantOps : public FunctionPass
{
	static char ID;
	ConstantOps() : FunctionPass(ID) {}
	
	virtual bool runOnFunction(llvm::Function& F) override;
	
	virtual void getAnalysisUsage(llvm::AnalysisUsage& Info) const override;
};

// Declares the Constant Branch Folding Pass
struct ConstantBranch : public FunctionPass
{
	static char ID;
	ConstantBranch() : FunctionPass(ID) {}
	
	virtual bool runOnFunction(llvm::Function& F) override;
	
	virtual void getAnalysisUsage(llvm::AnalysisUsage& Info) const override;
};

// Declares the Dead Block Removal Pass
struct DeadBlocks : public FunctionPass
{
	static char ID;
	DeadBlocks() : FunctionPass(ID) {}
	
	virtual bool runOnFunction(llvm::Function& F) override;
	
	virtual void getAnalysisUsage(llvm::AnalysisUsage& Info) const override;
};



// PA2 Declare Natural Loop Detection Pass
struct NaturalLoops : public FunctionPass {
  static char ID;
  NaturalLoops() : FunctionPass(ID) {}

  virtual bool runOnFunction(llvm::Function &F) override;
  void dfsFindBackEdge(llvm::BasicBlock *current, std::set<llvm::BasicBlock *> &visited);
  void findNaturalLoop(llvm::BasicBlock *tail);
  bool dfsReachable(llvm::BasicBlock* current, llvm::BasicBlock* target, llvm::BasicBlock* excludeBlock,std::set<llvm::BasicBlock*> visitedBlocks);
  void getDominatedBlocks(llvm::BasicBlock* block, std::vector<llvm::BasicBlock *>& dominatedBlocks);
  virtual void getAnalysisUsage(llvm::AnalysisUsage &Info) const override;
 // void printBackEdge(llvm::BasicBlock *head, llvm::BasicBlock *tail);
 // void printNaturalLoop(std::vector<llvm::BasicBlock*>& naturalLoop);
  void printBackEdge(std::string head, std::string tail);                                                      
  void printNaturalLoop(std::set<std::string>& naturalLoop); 
  void mapOutput(); 
  // save the current basic block you are visiting assuming it is a loop header
  llvm::BasicBlock *mHeader;
  // dominator tree
  llvm::DominatorTree* mDT;
};
	
// Loop invariant code motion
struct LICM : public LoopPass
{
	static char ID;
	LICM() : LoopPass(ID) {}
	
	virtual bool runOnLoop(llvm::Loop* L, llvm::LPPassManager& LPM) override;
	
	virtual void getAnalysisUsage(llvm::AnalysisUsage& Info) const override;

	// Data regarding the current loop
	llvm::Loop* mCurrLoop;

	// The dominator tree for this loop
	llvm::DominatorTree* mDomTree;

	// Loop information for this loop
	llvm::LoopInfo* mLoopInfo;

	// Denotes whether or not loop has been modified
	bool mChanged;
};
	
} // opt
} // uscc
