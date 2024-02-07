//===- NaturalLoopInfo.cpp - Natural Loop Detection Analysis -----------------------------===//
//
//
// This file is distributed under the BSD License. 
// See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//

#include <iostream>

#include "Passes.h"
#include "llvm/IR/Function.h" 
#include "llvm/Support/raw_ostream.h" 
#include "llvm/IR/Dominators.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"

#include <vector> 
#include <set>
#include <stack>

using namespace llvm;
using namespace std;
  
namespace uscc{

namespace opt{


void NaturalLoops::printBackEdge(BasicBlock *head, BasicBlock *tail){


    errs() << "Back Edge: " << head->getName() << " <--- " << tail->getName() << "\n";

}

void NaturalLoops::printNaturalLoop(std::vector<BasicBlock*>& naturalLoop){

    // Print the blocks in the natural loop
    errs() << "Natural Loop: ";
    for (BasicBlock *BB : naturalLoop) {
      errs() << BB->getName() << " ";
    }
    errs() << "\n";
    

}

bool NaturalLoops::runOnFunction(Function &F)
{
    //PA2 Implement
	
    //Iterate over all basic blocks in the function  
	mDT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
	
	for(auto &bb : F){
		mHeader = &bb;

		set<BasicBlock *> visited;
		dfsFindBackEdge(mHeader, visited);		
	}

	return false;
}


void NaturalLoops::dfsFindBackEdge(BasicBlock *current, set<BasicBlock *> &visited) {
    
    //PA2 Implement

    //Find whether there is a back edge targeting to the 'mHeader'
	
	visited.insert(current);
	
	for(auto iterator = succ_begin(current); iterator != succ_end(current); ++iterator){
		BasicBlock* currbb = *iterator;
		if(visited.find(*iterator) == visited.end()){
			dfsFindBackEdge(currbb, visited);
		}
		else if(currbb == mHeader){
			bool dom = mDT->dominates(mHeader, current);
			if(dom){
				printBackEdge(mHeader, current);
				findNaturalLoop(current);
			}
		}
	}

}


void NaturalLoops::findNaturalLoop(BasicBlock *tail)
{
    //PA2 Implement

    //Find the corresponding natural loop for the back edge
	
	vector<BasicBlock*> naturalLoopBB;

	naturalLoopBB.push_back(mHeader);

	vector<BasicBlock *> dominatedBlocks;

	getDominatedBlocks(mHeader, dominatedBlocks);

	set<BasicBlock*> visitedBlocks;

	bool canReach = false;
	for( auto iterator : dominatedBlocks){
		canReach = dfsReachable(iterator, tail, mHeader, visitedBlocks);		
		if(canReach) naturalLoopBB.push_back(iterator);
	}

	printNaturalLoop(naturalLoopBB);

}


void NaturalLoops::getDominatedBlocks(BasicBlock* block, vector<BasicBlock *>& dominatedBlocks) {
    // PA2 Implement 
    
    // Use DominatorTree to find all blocks dominated by 'block'
	dominatedBlocks.push_back(block);

	llvm::DomTreeNode* nodes = mDT->getNode(block); 

	for(auto iterator : *nodes){
			getDominatedBlocks(iterator->getBlock(), dominatedBlocks);
	}

}


bool NaturalLoops::dfsReachable(BasicBlock* current, BasicBlock* target, BasicBlock* excludeBlock,set<BasicBlock*> visitedBlocks)
{
      
  //PA2 Implement 

  //Find whether 'current block' can reach 'target block' without visiting 'excludeBlock'.
	visitedBlocks.insert(current);

	if(current == target && visitedBlocks.find(mHeader) == visitedBlocks.end()) return true;

	for(auto iterator = succ_begin(current); iterator != succ_end(current); ++iterator){
		if(visitedBlocks.find(*iterator) == visitedBlocks.end() && dfsReachable(*iterator, target, mHeader, visitedBlocks)){
			return true;
		}
	}

	return false;
}

void NaturalLoops::getAnalysisUsage(AnalysisUsage &Info) const  
{
    Info.addRequired<LoopInfo>();
    Info.addRequired<DominatorTreeWrapperPass>();
    Info.setPreservesAll();
}

} //opt

} //uscc


char uscc::opt::NaturalLoops::ID = 0;




