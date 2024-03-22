//
//  ASTNodes.cpp
//  uscc
//
//  Implements the emitIR function for every AST node.
//
//---------------------------------------------------------
//  Copyright (c) 2014, Sanjay Madhav
//  All rights reserved.
//
//  This file is distributed under the BSD license.
//  See LICENSE.TXT for details.
//---------------------------------------------------------

#include "ASTNodes.h"
#include "Emitter.h"

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
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/CFG.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <set>
#include <deque>
#pragma clang diagnostic pop

#include <vector>

using namespace uscc::parse;
using namespace llvm;

#define AST_EMIT(a) llvm::Value* a::emitIR(CodeContext& ctx) noexcept \
{ \
this->llvmBasicBlock = ctx.mBlock;

// Program/Functions
AST_EMIT(ASTProgram)
	ctx.mModule = new Module("main", ctx.mGlobal);
	
	// Write the global string table
	ctx.mStrings.emitIR(ctx);
	
	// Emit declaration for stdlib "printf", if we need it
	if (ctx.mPrintfIdent != nullptr)
	{
		std::vector<llvm::Type*> printfArgs;
		printfArgs.push_back(llvm::Type::getInt8PtrTy(ctx.mGlobal));
		
		FunctionType* printfType = FunctionType::get(llvm::Type::getInt32Ty(ctx.mGlobal),
													 printfArgs, true);
		
		Function* func = Function::Create(printfType, GlobalValue::LinkageTypes::ExternalLinkage,
										  "printf", ctx.mModule);
		func->setCallingConv(CallingConv::C);
		
		// Map the printf ident to this function
		ctx.mPrintfIdent->setAddress(func);
	}
	
	// Emit code for all the functions
	for (auto f : mFuncs)
	{
		f->emitIR(ctx);
        // Once the peeling happens, AST reconstruction is applied (reconstrust function)
        f->reconstruct(ctx);
        ctx.ASTConstMap.clear();
	}
	// A program actually doesn't have a value to return, since everything
	// is stored in Module
	return nullptr;
}

AST_EMIT(ASTFunction)
	FunctionType* funcType = nullptr;
	
	// First get the return type (there's only three choices)
	llvm::Type* retType = nullptr;
	if (mReturnType == Type::Int)
	{
		retType = llvm::Type::getInt32Ty(ctx.mGlobal);
	}
	else if (mReturnType == Type::Char)
	{
		retType = llvm::Type::getInt8Ty(ctx.mGlobal);
	}
	else
	{
		retType = llvm::Type::getVoidTy(ctx.mGlobal);
	}
	
	if (mArgs.size() == 0)
	{
		funcType = FunctionType::get(retType, false);
	}
	else
	{

		std::vector<llvm::Type*> args;
		for (auto arg : mArgs)
		{
			args.push_back(arg->getIdent().llvmType());
		}
		
		funcType = FunctionType::get(retType, args, false);
	}
	
	// Create the function, and make it the current one
	ctx.mFunc = Function::Create(funcType,
								 GlobalValue::LinkageTypes::ExternalLinkage,
								 mIdent.getName(), ctx.mModule);
	
	// Now that we have a new function, reset our SSA builder
	ctx.mSSA.reset();
	
	// Map the ident to this function
	mIdent.setAddress(ctx.mFunc);
	
	// Create the entry basic block
	ctx.mBlock = BasicBlock::Create(ctx.mGlobal, "entry", ctx.mFunc);
	// Add and seal this block
	ctx.mSSA.addBlock(ctx.mBlock, true);
	
	// If we have arguments, we need to set the name/value of them
	if (mArgs.size() > 0)
	{
		Function::arg_iterator iter = ctx.mFunc->arg_begin();
		Function::arg_iterator end = ctx.mFunc->arg_end();
		int i = 0;
		while (iter != end)
		{
			Identifier& argIdent = mArgs[i]->getIdent();
			iter->setName(argIdent.getName());
			
			// Later PA: Remove the setAddress call
			// (Technically, iter actually has the value of the
			// arg, not its address...but we will use the address
			// member for this value)
			//	argIdent.setAddress(iter);
			
			// Later PA: Write to this identifier
			argIdent.writeTo(ctx, iter);
			
			++i;
			++iter;
		}
	}
	
	ctx.mFunc->setCallingConv(CallingConv::C);
	
	// Add all the declarations for variables created in this function
	mScopeTable.emitIR(ctx);
	
	// Now emit the body
	mBody->emitIR(ctx);
	
	return ctx.mFunc;
}

AST_EMIT(ASTArgDecl)
	// This node actually doesn't have anything to emit
	return nullptr;
}

AST_EMIT(ASTArraySub)
	// Evaluate the sub expression to get the desired index
	Value* arrayIdx = mExpr->emitIR(ctx);
	
	// This address should already be saved
	Value* addr = mIdent.readFrom(ctx);
	
	// GEP from the array address
	IRBuilder<> build(ctx.mBlock);
	return build.CreateInBoundsGEP(addr, arrayIdx);
}

// Expressions

AST_EMIT(ASTBadExpr)
	// This node will never be emitted
	return nullptr;
}

AST_EMIT(ASTLogicalAnd)
	// This is extremely similar to logical or
	
	// Create the block for the RHS
	BasicBlock* rhsBlock = BasicBlock::Create(ctx.mGlobal, "and.rhs", ctx.mFunc);
	// Add the rhs block to SSA (not sealed)
	ctx.mSSA.addBlock(rhsBlock);
	
	// In both "true" and "false" condition, we'll jump to and.end
	// This is because we'll insert a phi node that assume false
	// if the and.end jump was from the lhs block
	BasicBlock* endBlock = BasicBlock::Create(ctx.mGlobal, "and.end", ctx.mFunc);
	// Also not sealed
	ctx.mSSA.addBlock(endBlock);
	
	// Now generate the LHS
	Value* lhsVal = mLHS->emitIR(ctx);
	
	BasicBlock* lhsBlock = ctx.mBlock;
	
	// Add the branch to the end of the LHS
	{
		IRBuilder<> build(ctx.mBlock);
		// We can assume it WILL be an i32 here
		// since it'd have been zero-extended otherwise
		lhsVal = build.CreateICmpNE(lhsVal, ctx.mZero, "tobool");
		build.CreateCondBr(lhsVal, rhsBlock, endBlock);
	}
	
	// rhsBlock should now be sealed
	ctx.mSSA.sealBlock(rhsBlock);
	
	// Code should now be generated in the RHS block
	ctx.mBlock = rhsBlock;
	Value* rhsVal = mRHS->emitIR(ctx);
	
	// This is the final RHS block (for the phi node)
	rhsBlock = ctx.mBlock;
	
	// Add the branch and the end of the RHS
	{
		IRBuilder<> build(ctx.mBlock);
		rhsVal = build.CreateICmpNE(rhsVal, ctx.mZero, "tobool");
		
		// We do an unconditional branch because the phi mode will handle
		// the correct value
		build.CreateBr(endBlock);
	}
	
	// endBlock should now be sealed
	ctx.mSSA.sealBlock(endBlock);
	
	ctx.mBlock = endBlock;
	
	IRBuilder<> build(ctx.mBlock);
	
	// Figure out the value to zext
	Value* zextVal = nullptr;
	
	// If rhs is not also false, we need to make a phi
	if (rhsVal != ConstantInt::getFalse(ctx.mGlobal))
	{
		PHINode* phi = build.CreatePHI(llvm::Type::getInt1Ty(ctx.mGlobal), 2);
		// If we came from the lhs, it had to be false
		phi->addIncoming(ConstantInt::getFalse(ctx.mGlobal), lhsBlock);
		phi->addIncoming(rhsVal, rhsBlock);
		zextVal = phi;
	}
	else
	{
		zextVal = ConstantInt::getFalse(ctx.mGlobal);
	}
	
	return build.CreateZExt(zextVal, llvm::Type::getInt32Ty(ctx.mGlobal));
}

AST_EMIT(ASTLogicalOr)
	// Create the block for the RHS
	BasicBlock* rhsBlock = BasicBlock::Create(ctx.mGlobal, "lor.rhs", ctx.mFunc);
	// Add the rhs block to SSA (not sealed)
	ctx.mSSA.addBlock(rhsBlock);

	// In both "true" and "false" condition, we'll jump to lor.end
	// This is because we'll insert a phi node that assume true
	// if the lor.end jump was from the lhs block
	BasicBlock* endBlock = BasicBlock::Create(ctx.mGlobal, "lor.end", ctx.mFunc);
	// Also not sealed
	ctx.mSSA.addBlock(endBlock);
	
	// Now generate the LHS
	Value* lhsVal = mLHS->emitIR(ctx);
	
	BasicBlock* lhsBlock = ctx.mBlock;
	
	// Add the branch to the end of the LHS
	{
		IRBuilder<> build(ctx.mBlock);
		// We can assume it WILL be an i32 here
		// since it'd have been zero-extended otherwise
		lhsVal = build.CreateICmpNE(lhsVal, ctx.mZero, "tobool");
		build.CreateCondBr(lhsVal, endBlock, rhsBlock);
	}
	
	// rhsBlock should now be sealed
	ctx.mSSA.sealBlock(rhsBlock);
	
	// Code should now be generated in the RHS block
	ctx.mBlock = rhsBlock;
	Value* rhsVal = mRHS->emitIR(ctx);
	
	// This is the final RHS block (for the phi node)
	rhsBlock = ctx.mBlock;
	
	// Add the branch and the end of the RHS
	{
		IRBuilder<> build(ctx.mBlock);
		rhsVal = build.CreateICmpNE(rhsVal, ctx.mZero, "tobool");
		
		// We do an unconditional branch because the phi mode will handle
		// the correct value
		build.CreateBr(endBlock);
	}
	
	// endBlock should now be sealed
	ctx.mSSA.sealBlock(endBlock);
	
	ctx.mBlock = endBlock;
	
	IRBuilder<> build(ctx.mBlock);
	
	// Figure out the value to zext
	Value* zextVal = nullptr;
	
	// If rhs is not also true, we need to make a phi
	if (rhsVal != ConstantInt::getTrue(ctx.mGlobal))
	{
		PHINode* phi = build.CreatePHI(llvm::Type::getInt1Ty(ctx.mGlobal), 2);
		// If we came from the lhs, it had to be false
		phi->addIncoming(ConstantInt::getTrue(ctx.mGlobal), lhsBlock);
		phi->addIncoming(rhsVal, rhsBlock);
		zextVal = phi;
	}
	else
	{
		zextVal = ConstantInt::getTrue(ctx.mGlobal);
	}
	
	return build.CreateZExt(zextVal, llvm::Type::getInt32Ty(ctx.mGlobal));
}

AST_EMIT(ASTBinaryCmpOp)
	Value* retVal = nullptr;
	
	// PA1: Implement
    IRBuilder<> builder(ctx.mBlock);
    Value * rhs = mRHS->emitIR(ctx);
    Value * lhs = mLHS->emitIR(ctx);
    switch (mOp)
    {
    case scan::Token::EqualTo:
        retVal = builder.CreateICmpEQ(lhs, rhs, "eq");
        break;
    case scan::Token::NotEqual:
        retVal = builder.CreateICmpNE(lhs, rhs, "ne");
        break;
    case scan::Token::GreaterThan:
        retVal = builder.CreateICmpSGT(lhs, rhs, "gt");
        break;
    case scan::Token::LessThan:
        retVal = builder.CreateICmpSLT(lhs, rhs, "lt");
        break;
    
    default:
        break;
    }
	
	return retVal;
}

AST_EMIT(ASTBinaryMathOp)
	Value* retVal = nullptr;
	
	// PA1: Implement
    IRBuilder<> builder(ctx.mBlock);
    Value * rhs = mRHS->emitIR(ctx);
    Value * lhs = mLHS->emitIR(ctx);
    switch (mOp)
    {
    case scan::Token::Plus:
        retVal = builder.CreateAdd(lhs, rhs, "add");
        break;
    case scan::Token::Minus:
        retVal = builder.CreateSub(lhs, rhs, "sub");
        break;
    case scan::Token::Mult:
        retVal = builder.CreateMul(lhs, rhs, "mul");
        break;
    case scan::Token::Div:
        retVal = builder.CreateSDiv(lhs, rhs, "div");
        break;
    case scan::Token::Mod:
        retVal = builder.CreateSRem(lhs, rhs, "mod");
        break;
    
    default:
        break;
    }
	
	return retVal;
}

// Value -->
AST_EMIT(ASTNotExpr)
	Value* retVal = nullptr;
	
	// PA1: Implement
    IRBuilder<> builder(ctx.mBlock);
	auto value = mExpr->emitIR(ctx);
    value = builder.CreateICmpEQ(value, ctx.mZero);
    retVal = builder.CreateZExt(value, llvm::Type::getInt32Ty(ctx.mGlobal));

	return retVal;
}

// Factor -->
AST_EMIT(ASTConstantExpr)
	Value* retVal = nullptr;
	
	// PA1: Implement
    if (mType == Type::Char)
        retVal = ConstantInt::get(llvm::Type::getInt8Ty(ctx.mGlobal), mValue);
    else
        retVal = ConstantInt::get(llvm::Type::getInt32Ty(ctx.mGlobal), mValue);

	return retVal;
}

AST_EMIT(ASTStringExpr)
	return mString->getValue();
}

AST_EMIT(ASTIdentExpr)
	return mIdent.readFrom(ctx);
}

AST_EMIT(ASTArrayExpr)
	// Generate the array subscript, which'll give us the address
	Value* addr = mArray->emitIR(ctx);

	IRBuilder<> build(ctx.mBlock);
	// Now load this value and return
	
	// NOTE: This still needs to be a load because arrays are in memory
	return build.CreateLoad(addr);
}

AST_EMIT(ASTFuncExpr)
	// At this point, we can assume the argument types match
	// Create the list of arguments
	std::vector<Value*> callList;
	for (auto arg : mArgs)
	{
		Value* argValue = arg->emitIR(ctx);
		// If this is an array or ptr, we need to change this to a getelemptr
		// (Provided it already isn't one)
		if (!isa<GetElementPtrInst>(argValue) &&
			argValue->getType()->isPointerTy())
		{
			if (argValue->getType()->getPointerElementType()->isArrayTy())
			{
				IRBuilder<> build(ctx.mBlock);
				std::vector<llvm::Value*> gepIdx;
				gepIdx.push_back(ctx.mZero);
				gepIdx.push_back(ctx.mZero);
				
				argValue = build.CreateInBoundsGEP(argValue, gepIdx);
			}
			else
			{
				IRBuilder<> build(ctx.mBlock);				
				// Need to return the address of the specific index in question
				// So need a GEP
				argValue = build.CreateInBoundsGEP(argValue, ctx.mZero);
			}
		}
			
		callList.push_back(argValue);
	}
	
	// Now call the function, and return it
	Value* retVal = nullptr;
	
	IRBuilder<> build(ctx.mBlock);
	if (mType != Type::Void)
	{
		retVal = build.CreateCall(mIdent.getAddress(), callList, "call");
	}
	else
	{
		retVal = build.CreateCall(mIdent.getAddress(), callList);
	}
	
	return retVal;
}

AST_EMIT(ASTIncExpr)
	Value* retVal = nullptr;
	
	// PA1: Implement
    IRBuilder<> builder(ctx.mBlock);
    auto value = mIdent.readFrom(ctx);
    value = builder.CreateAdd(value, ConstantInt::get(value->getType(), 1), "inc"); // use the same type as value
    mIdent.writeTo(ctx, value);
    retVal = value;

	return retVal;
}

AST_EMIT(ASTDecExpr)
	Value* retVal = nullptr;
	
	// PA1: Implement
    IRBuilder<> builder(ctx.mBlock);
    auto value = mIdent.readFrom(ctx);
    value = builder.CreateSub(value, ConstantInt::get(value->getType(), 1), "dec");
    mIdent.writeTo(ctx, value);
    retVal = value;
    
	return retVal;
}

AST_EMIT(ASTAddrOfArray)
	return mArray->emitIR(ctx);
}

AST_EMIT(ASTToIntExpr)
	Value* exprVal = mExpr->emitIR(ctx);
	IRBuilder<> build(ctx.mBlock);
	return build.CreateSExt(exprVal, llvm::Type::getInt32Ty(ctx.mGlobal), "conv");
}

AST_EMIT(ASTToCharExpr)
	Value* exprVal = mExpr->emitIR(ctx);
	IRBuilder<> build(ctx.mBlock);
	return build.CreateTrunc(exprVal, llvm::Type::getInt8Ty(ctx.mGlobal), "conv");
}

// Declaration
AST_EMIT(ASTDecl)
	// If there's an expression, emit this also and store it in the ident
	if (mExpr)
	{
		Value* declExpr = mExpr->emitIR(ctx);
		
		IRBuilder<> build(ctx.mBlock);
		// If this is a string, we have to memcpy
		if (declExpr->getType()->isPointerTy())
		{
			// This address should already be saved
			Value* arrayLoc = mIdent.readFrom(ctx);
			
			// GEP the address of the src
			std::vector<llvm::Value*> gepIdx;
			gepIdx.push_back(ctx.mZero);
			gepIdx.push_back(ctx.mZero);
			
			Value*  src = build.CreateGEP(declExpr, gepIdx);
			
			// Memcpy into the array
			// memcpy(dest, src, size, align, volatile)
			build.CreateMemCpy(arrayLoc, src, mIdent.getArrayCount(), 1);
		}
		else
		{
			// Basic types can just be written
			mIdent.writeTo(ctx, declExpr);
		}
	}
	
	return nullptr;
}

// Statements
AST_EMIT(ASTCompoundStmt)
	// PA1: Implement
    for (auto & x : mDecls)
        x->emitIR(ctx);
    for (auto & x : mStmts) {
        x->emitIR(ctx);
    }

	return nullptr;
}

AST_EMIT(ASTAssignStmt)
	// This is simpler than decl because we don't allow
	// assignments to happen later for full arrays
	
	// PA1: Implement
    auto value = mExpr->emitIR(ctx);
    mIdent.writeTo(ctx, value);

	return nullptr;
}

AST_EMIT(ASTAssignArrayStmt)
	// Generate the expression
	Value* exprVal = mExpr->emitIR(ctx);
	
	// Generate the array subscript, which'll give us the address
	Value* addr = mArray->emitIR(ctx);

	IRBuilder<> build(ctx.mBlock);
	
	// NOTE: This is still a create store because arrays are always stack-allocated
	build.CreateStore(exprVal, addr);
	
	return nullptr;
}

AST_EMIT(ASTIfStmt)
	// PA1: Implement
    auto value = this->mExpr->emitIR(ctx);
    IRBuilder<> builder(ctx.mBlock);
    if (!value->getType()->isIntegerTy(1)) // check bool
        value = builder.CreateICmpNE(value, ctx.mZero); // if not bool, make it
   	//ctx.mSSA.addBlock(ctx.mBlock, true);

    auto thenBody = BasicBlock::Create(ctx.mGlobal, "if.then", ctx.mFunc);
	ctx.mSSA.addBlock(thenBody, true);

    auto end = BasicBlock::Create(ctx.mGlobal, "if.end", ctx.mFunc);
	ctx.mSSA.addBlock(end);

    if (mElseStmt)
    {
        auto elseBody = BasicBlock::Create(ctx.mGlobal, "if.else", ctx.mFunc);
        builder.CreateCondBr(value, thenBody, elseBody);
		ctx.mSSA.addBlock(elseBody, true);

        ctx.mBlock = elseBody;
        mElseStmt->emitIR(ctx);
        IRBuilder<> builderElse(ctx.mBlock);
        builderElse.CreateBr(end);
    }
    else
        builder.CreateCondBr(value, thenBody, end);
    
    ctx.mBlock = thenBody;
    mThenStmt->emitIR(ctx);
    IRBuilder<> builderThen(ctx.mBlock);
    builderThen.CreateBr(end);

	ctx.mSSA.sealBlock(end);
    ctx.mBlock = end;

	return nullptr;
}

AST_EMIT(ASTWhileStmt)
    if (ctx.PeelingEnabled && CanPeel(ctx)) { // Check whether the 1st-loop-iteration peeling is applicable
      emitIR_LoopPeeling(ctx);
      this->mPeeling = true;
    }
    else {
	  // PA1: Implement
      auto cond = BasicBlock::Create(ctx.mGlobal, "while.cond", ctx.mFunc); //while header block
	  ctx.mSSA.addBlock(cond);

      IRBuilder<> builder(ctx.mBlock);
      builder.CreateBr(cond); // unconditional branch in predecessor

      ctx.mBlock = cond;
      auto value = this->mExpr->emitIR(ctx);
      IRBuilder<> builderCond(ctx.mBlock);
      if (!value->getType()->isIntegerTy(1)) // check bool
          value = builderCond.CreateICmpNE(value, ctx.mZero); // if not bool, make it

      auto body = BasicBlock::Create(ctx.mGlobal, "while.body", ctx.mFunc); // after expr's emitIR
	  ctx.mSSA.addBlock(body, true);

      auto end = BasicBlock::Create(ctx.mGlobal, "while.end", ctx.mFunc);
	  ctx.mSSA.addBlock(end, true);

      builderCond.CreateCondBr(value, body, end); // conditional branch in while.cond
	  //ctx.mSSA.sealBlock(body);
	  //ctx.mSSA.sealBlock(end);

      ctx.mBlock = body;
      this->mLoopStmt->emitIR(ctx);
      IRBuilder<> builderBody(ctx.mBlock);
      builderBody.CreateBr(cond);
	  ctx.mSSA.sealBlock(cond);
      ctx.mBlock = end;
    }
    
	return nullptr;
}

// If killedDef is found in the use (operands) of any instruction in cond blocks -> return false
// Otherwise -> return true
bool CondBBsNotUseKilledDef(BasicBlock *CondBB, std::list<llvm::Value*> &killedDef, std::set<BasicBlock*> &visited) {
  visited.insert(CondBB);
  for (auto &II : *CondBB) {
    for (int i = 0; i < II.getNumOperands(); i++) {
      // If any operand uses the rewritten variables (operands) in preheader, return false
      if (std::find(killedDef.begin(), killedDef.end(), II.getOperand(i)) != killedDef.end()) {
        return false;
      }
    }
    // 1) If there's any redefinition (rewriting) of any variables which are used in the condition, the loop peeling isn't possible
    // preheader BB: x++
    // condition BB: while (x < 10) { ... }
    // ------------------------------------------
    // preheader BB: a = 10;
    // preheader BB: x = a + 100;
    // condition BB: while (x < 10) { ... }
    // ------------------------------------------
    // 2) If there's declaration which are relevant with the condition, the loop peeling isn't possible
    // preheader BB: int y = 100
    // condition BB: while (y < 10000) { ... }
    // ------------------------------------------
    llvm::Value * rdef = dyn_cast<llvm::Value>(&II);
    if (std::find(killedDef.begin(), killedDef.end(), rdef) != killedDef.end()) {
      killedDef.remove(rdef);
    }
  }
  bool ret = true;
  for (succ_iterator succ = succ_begin(CondBB), succ_endBB = succ_end(CondBB); succ != succ_endBB; ++succ) {
    if (!visited.count(*succ)) 
      ret = ret && CondBBsNotUseKilledDef(*succ, killedDef, visited);
  }
  return ret;
}

void computePostOrder(BasicBlock *entry, std::set<BasicBlock *> &visited, std::list<BasicBlock *> &order) 
{
  visited.insert(entry);
  auto succItr = succ_begin(entry), end = succ_end(entry);
  for (; succItr != end; ++succItr) 
    if (!visited.count(*succItr))
      computePostOrder(*succItr, visited, order);
  order.push_back(entry);
}

// Compute BFS traversal order starting from BB
std::list<BasicBlock*> computeBFSOrder(CodeContext& ctx, BasicBlock* BB) {
  std::set<BasicBlock*> visited;
  std::list<BasicBlock *> toBeRemoved;
  computePostOrder(BB, visited, toBeRemoved);
  toBeRemoved.reverse();
  return toBeRemoved;
}

void ASTWhileStmt::getAllocaList(CodeContext& ctx) {
  this->AllocaList.clear();
  auto FirstBB = ctx.mFunc->begin();
  for (auto &II : *FirstBB) {
    if (AllocaInst* inst = dyn_cast<llvm::AllocaInst>(&II)) {
      this->AllocaList.push_back(inst);
    }
  }
}

void ASTWhileStmt::getKilledDefsInPreheader(CodeContext& ctx) {
  this->killedDefInPreheader.clear();
  auto FirstBB = ctx.mFunc->begin();
  if (ctx.mBlock != FirstBB) {
    for (auto &II : *ctx.mBlock) {
      if (isa<llvm::StoreInst>(II)) {
        // Overwriting of the alloca variables: Find the redefined operand
        if (std::find(this->AllocaList.begin(), this->AllocaList.end(), II.getOperand(1)) != this->AllocaList.end()) {
          this->killedDefInPreheader.push_back(II.getOperand(1));
        }
      }
    }
  }
  else { // when preheader is the first basicblock of the program
    std::unordered_map<llvm::Value*, int> defs;
    for (auto value : this->AllocaList)
      defs[value] = 0;
    for (auto &II : *ctx.mBlock) { 
      if (isa<llvm::StoreInst>(II)) {
        // Overwriting of the alloca variables: Find the redefined operand
        if (std::find(this->AllocaList.begin(), this->AllocaList.end(), II.getOperand(1)) != this->AllocaList.end()) {
          defs[II.getOperand(1)]++;
        }
      }
    }
    for (auto def : defs) {
      if (def.second > 0) { // second =1: Initial def after alloca. second > 1: This means redef happens
        this->killedDefInPreheader.push_back(def.first);
      }
    }
  }
}


bool ASTWhileStmt::CanPeel(CodeContext& ctx) {
  auto curBlock = ctx.mBlock;
  // 1) Get all alloca from the first basic block in the function
  getAllocaList(ctx);

  // 2) Find the defs (allocas) that are re-defined in the preheader
  getKilledDefsInPreheader(ctx);

  // 3) Find whether the killed allocas (killedDefInPreheader) are used in the condition
  auto CondBB = BasicBlock::Create(ctx.mGlobal, "CondBB", ctx.mFunc);
  ctx.mBlock = CondBB; 
  auto value = this->mExpr->emitIR(ctx);

  std::set<BasicBlock*> visited;
  bool ret = CondBBsNotUseKilledDef(CondBB, killedDefInPreheader, visited);

  // 4) Remove CondBB and the successor blocks generated here
  std::list<BasicBlock*> toBeRemoved = computeBFSOrder(ctx, CondBB);
  for (auto BB : toBeRemoved) {
    if (BB == toBeRemoved.back()) break;
    llvm::DeleteDeadBlock(BB);
  }
  toBeRemoved.back()->eraseFromParent();

  ctx.mBlock = curBlock;
  return ret;
}

BasicBlock* replicate_basicblock(CodeContext& ctx) {
  BasicBlock* NewBB = BasicBlock::Create(ctx.mGlobal, "replicated", ctx.mFunc);

  std::unordered_map<Instruction*, Instruction*> fixedInstList;
  for (BasicBlock::iterator II = ctx.mBlock->begin(); II != ctx.mBlock->end(); ++II) {
    if (isa<llvm::AllocaInst>(*II)) 
      continue;
    /* else if (CallInst * inst = dyn_cast<llvm::CallInst>(II)) { */
    /*   StringRef name = inst->getCalledFunction()->getName(); */
    /*   if (name.startswith("llvm.memcpy")) */
    /*     continue; */
    /* } */
    Instruction *newI;
    std::set<Instruction*> myusers;
    for (auto U : II->users()) {
      if (auto I = dyn_cast<Instruction>(U))
        myusers.insert(dyn_cast<Instruction>(U));
    }
    if (fixedInstList.find(II) == fixedInstList.end())
      newI = II->clone();
    else
      newI = fixedInstList[II];

    BasicBlock::iterator iter = II;
    iter++;
    while (iter != ctx.mBlock->end()) {
      Instruction* nextI = dyn_cast<Instruction>(iter);
      if (myusers.find(nextI) != myusers.end()) {
        Instruction *newIUser;
        if (fixedInstList.find(nextI) != fixedInstList.end()) {
          newIUser = fixedInstList[nextI];
        }
        else {
          newIUser = nextI->clone();
          fixedInstList[nextI] = newIUser;
        }
        for (int i = 0; i < newIUser->getNumOperands(); i++) {
          if (dyn_cast<Value>(newIUser->getOperand(i)) == dyn_cast<Value>(II)) {
            newIUser->setOperand(i, newI);
          }
        }
      }
      iter++;
    }
    if (II->hasName())
      newI->setName(II->getName());
    NewBB->getInstList().push_back(newI);
  } // for
  return NewBB;
}

void ASTWhileStmt::RemovePreHeader(CodeContext& ctx) {
  // 2-1) AllocaInst: These should be in the preheader
  // 2-2) CallInst: llvm.memcpy should remain here.
  // 2-3) StoreInst: "CanPeel() = true" means there's no re-written operands which are used in the cond
  // But there can be the re-written operands irrelevant with the cond (killedDefInPreheader)
  // If the given StoreInst stores to the operand in killedDefInPreheader, then remove it.
  std::list<Instruction*>toBeRemoved;
  for (auto &I : *ctx.mBlock) {
    if (isa<llvm::AllocaInst>(I)) continue;
    else if (isa<llvm::StoreInst>(I)) {
      if (std::find(this->killedDefInPreheader.begin(), this->killedDefInPreheader.end(), I.getOperand(1)) != this->killedDefInPreheader.end()) {
        toBeRemoved.push_back(&I);
      }
    }
    else if (isa<llvm::CallInst>(I)) {  
      StringRef name = cast<llvm::CallInst>(I).getCalledFunction()->getName();
      if (!name.startswith("llvm.memcpy")) 
        toBeRemoved.push_back(&I);
    }
  }
  for (auto &I : toBeRemoved)
    I->eraseFromParent();
}


void ASTWhileStmt::emitIR_LoopPeeling(CodeContext& ctx) {

  // 2) Replicate the current basic block (preheader) 
  auto replicated_preheader_1 = replicate_basicblock(ctx);
  auto replicated_preheader_2 = replicate_basicblock(ctx);

  // 3) Empty the existing preheader before appending the predicate
  RemovePreHeader(ctx);
  
  // 4) Append the predicate(fliter) and install the edges to the replicated_preheader_1 and the replicated_preheader_2
  auto value = this->mExpr->emitIR(ctx);
  IRBuilder<> builderCond(ctx.mBlock);
  if (!value->getType()->isIntegerTy(1))
    value = builderCond.CreateICmpNE(value, ctx.mZero);
  builderCond.CreateCondBr(value, replicated_preheader_1, replicated_preheader_2);

  // 5) If the condition is held: Merge the replicated preheader with the 1st peeled body
  ctx.mBlock = replicated_preheader_1;
  this->mLoopStmt->emitIR(ctx);

  auto rest_header = BasicBlock::Create(ctx.mGlobal, "while.rest_header", ctx.mFunc);
  IRBuilder<> builderPeeled(ctx.mBlock);
  builderPeeled.CreateBr(rest_header);

  // 6) header - rest_body (loop body for the rest of iterations) - end
  ctx.mBlock = rest_header; 
  auto value2 = this->mExpr->emitIR(ctx);
  IRBuilder<> builderHeader(ctx.mBlock);
  if (!value2->getType()->isIntegerTy(1))
    value2 = builderHeader.CreateICmpNE(value2, ctx.mZero);
  auto rest_body = BasicBlock::Create(ctx.mGlobal, "while.rest_body", ctx.mFunc);
  auto end = BasicBlock::Create(ctx.mGlobal, "while.end", ctx.mFunc);
  builderHeader.CreateCondBr(value2, rest_body, end);

  ctx.mBlock = rest_body;
  this->mLoopStmt->emitIR(ctx);
  IRBuilder<> builderBody(ctx.mBlock);
  builderBody.CreateBr(rest_header);

  // 7) If the condition is not held: just fall to end block
  ctx.mBlock = replicated_preheader_2;
  IRBuilder<> builderPeeled_2(ctx.mBlock);
  builderPeeled_2.CreateBr(end);

  ctx.mBlock = end;
}

AST_EMIT(ASTReturnStmt)
	// PA1: Implement
	IRBuilder<> builder(ctx.mBlock);
    if (mExpr)
        builder.CreateRet(mExpr->emitIR(ctx));
    else
        builder.CreateRetVoid();

	return nullptr;
}

AST_EMIT(ASTExprStmt)
	// PA1: Implement
	// Just emit the expression, don't care about the value
    mExpr->emitIR(ctx);

	return nullptr;
}

AST_EMIT(ASTNullStmt)
	// Doesn't do anything (hence empty)
	return nullptr;
}
