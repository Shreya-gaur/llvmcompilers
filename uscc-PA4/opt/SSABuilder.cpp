//
//  SSABuilder.cpp
//  uscc
//
//  Implements SSABuilder class
//  
//---------------------------------------------------------
//  Copyright (c) 2014, Sanjay Madhav
//  All rights reserved.
//
//  This file is distributed under the BSD license.
//  See LICENSE.TXT for details.
//---------------------------------------------------------

#include "SSABuilder.h"
#include "../parse/Symbols.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#pragma clang diagnostic pop

#include <list>

using namespace uscc::opt;
using namespace uscc::parse;
using namespace llvm;

// Called when a new function is started to clear out all the data
void SSABuilder::reset()
{
	// PA4: Implement
	for (auto& key : mVarDefs) 
	{  
		delete key.second;
	}
	mVarDefs.clear();

	for (auto& key : mIncompletePhis) 
	{  
		delete key.second;
	}
	mIncompletePhis.clear();
}

// For a specific variable in a specific basic block, write its value
void SSABuilder::writeVariable(Identifier* var, BasicBlock* block, Value* value)
{
	(*mVarDefs[block])[var] = value;
}

// Read the value assigned to the variable in the requested basic block
// Will recursively search predecessor blocks if it was not written in this block
Value* SSABuilder::readVariable(Identifier* var, BasicBlock* block)
{
	// PA4: Implement
	if(mVarDefs[block] != nullptr && mVarDefs.find(block) != mVarDefs.end()){
		if(mVarDefs[block]->find(var) != mVarDefs[block]->end()){
			auto variable = mVarDefs[block];
			return variable->at(var);
		}
	}
	return readVariableRecursive(var, block);
}

// This is called to add a new block to the maps
void SSABuilder::addBlock(BasicBlock* block, bool isSealed /* = false */)
{
	// PA4: Implement
	SubMap* vardefs = new SubMap();
	mVarDefs[block] = vardefs;

	SubPHI* incphis = new SubPHI();
	mIncompletePhis[block] = incphis;

	if(isSealed){
		sealBlock(block);
	}
}

// This is called when a block is "sealed" which means it will not have any
// further predecessors added. It will complete any PHI nodes (if necessary)
void SSABuilder::sealBlock(llvm::BasicBlock* block)
{
	// PA4: Implement
	for(auto &var : *mIncompletePhis[block]){
			addPhiOperands(var.first, var.second);
	}
	mSealedBlocks.insert(block);
}

// Recursively search predecessor blocks for a variable
Value* SSABuilder::readVariableRecursive(Identifier* var, BasicBlock* block)
{
	Value* retVal = nullptr;
	
	// PA4: Implement
	PHINode* phiNode;

	//if block is not in sealedBlocks
	if(mSealedBlocks.find(block) == mSealedBlocks.end()){
		if(block->getFirstNonPHI() != block->end()){
			phiNode = PHINode::Create(var->llvmType(), 0, "Phi", block->getFirstNonPHI());
		}
		else{
			phiNode = PHINode::Create(var->llvmType(), 0, "Phi", block);
		}
		retVal = phiNode;
		
		(*mIncompletePhis[block])[var] = phiNode;
	}
	else if (block->getSinglePredecessor()){ 
		retVal = readVariable(var, block->getSinglePredecessor());
	}
	else{
		if(block->getFirstNonPHI() != block->end()){
			phiNode = PHINode::Create(var->llvmType(), 0, "Phi", block->getFirstNonPHI());
		}
		else{
			phiNode = PHINode::Create(var->llvmType(), 0, "Phi", block);
		}
		writeVariable(var, block, phiNode);
		retVal = addPhiOperands(var, phiNode);
	}
	writeVariable(var, block, retVal);
	
	return retVal; //Where to get the return value from
}

// Adds phi operands based on predecessors of the containing block
Value* SSABuilder::addPhiOperands(Identifier* var, PHINode* phi)
{
	// PA4: Implement
	BasicBlock* block = phi->getParent();
	if(pred_begin(block) != pred_end(block)){
		for(auto it = pred_begin(block); it != pred_end(block); ++it){
			phi->addIncoming(readVariable(var, *it), *it);
		}
	}

	return tryRemoveTrivialPhi(phi);
}

// Removes trivial phi nodes
Value* SSABuilder::tryRemoveTrivialPhi(llvm::PHINode* phi)
{
	Value* same = nullptr;
	unsigned numOfIncomingValues = phi->getNumIncomingValues();

	// PA4: Implement
	for(unsigned i=0; i<numOfIncomingValues; i++){
		if(phi->getIncomingValue(i) == same || phi->getIncomingValue(i) == phi){
			continue;
		}
		if(same != nullptr){
			return phi;
		}
		same = phi->getIncomingValue(i);
	}
	if(same == nullptr){
		same = UndefValue::get(phi->getType());
	}	

	phi->replaceAllUsesWith(same);
	//update the variable definition map to use the same
	for(auto &blk : mVarDefs){
		if(blk.second != nullptr){
			for(auto &idf : *blk.second){
				if(idf.second == phi){
					idf.second = same;
				}
			}
		}
	}

	phi->eraseFromParent();

	for(auto use = phi->user_begin(); use != phi->user_end(); ++use){
		if(cast<PHINode>(*use) == phi)
			tryRemoveTrivialPhi(cast<PHINode>(*use));
	}
	return same;
}
