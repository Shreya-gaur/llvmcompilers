//
//  Emitter.cpp
//  uscc
//
//  Implements helper classes related to LLVM IR
//  construction.
//
//---------------------------------------------------------
//  Copyright (c) 2014, Sanjay Madhav
//  All rights reserved.
//
//  This file is distributed under the BSD license.
//  See LICENSE.TXT for details.
//---------------------------------------------------------


#include "Emitter.h"
#include "Parse.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <llvm/IR/Value.h>
#include <llvm/PassManager.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Bitcode/BitcodeWriterPass.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Support//FileSystem.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/MC/SubtargetFeature.h>
#include <llvm/IR/IRBuilder.h>

#include "../opt/Passes.h"
#pragma clang diagnostic pop

using namespace uscc::parse;
using namespace llvm;

CodeContext::CodeContext(StringTable& strings, bool ASTOptimized, bool PeelingEnabled)
: mGlobal(getGlobalContext())
, mModule(nullptr)
, mBlock(nullptr)
, mStrings(strings)
, mPrintfIdent(nullptr)
, mZero(nullptr)
, mFunc(nullptr)
, ASTOptimized(ASTOptimized)
, PeelingEnabled(PeelingEnabled)
{
	
}

Emitter::Emitter(Parser& parser, bool ASTOptimized, bool PeelingEnabled) noexcept
: mContext(parser.mStrings, ASTOptimized, PeelingEnabled)
{
	if (parser.mNeedPrintf)
	{
		mContext.mPrintfIdent = parser.mSymbols.getIdentifier("printf");
	}
	
	// Initialize zero
	mContext.mZero = Constant::getNullValue(IntegerType::getInt32Ty(mContext.mGlobal));

	// This is what kicks off the generation of the LLVM IR from the AST
    if (!parser.readBC)
        parser.mRoot->emitIR(mContext);
}

void Emitter::printCode(Parser& parser) noexcept
{
    // Print the .usc code
    parser.mRoot->ASTtoCode(*(parser.mASTStream),0);
}

void Emitter::optimize() noexcept
{
	legacy::PassManager pm;
	uscc::opt::registerOptPasses(pm);
	pm.run(*mContext.mModule);
}

void Emitter::print() noexcept
{
	legacy::PassManager pm;
	pm.add(createPrintModulePass(outs()));
	pm.run(*mContext.mModule);
}

void Emitter::writeBitcode(const char* fileName) noexcept
{
	legacy::PassManager pm;
	std::string err;
	raw_fd_ostream file(fileName, err, sys::fs::F_None);
	pm.add(createBitcodeWriterPass(file));
	pm.run(*mContext.mModule);
}

bool Emitter::verify() noexcept
{
	//return !verifyModule(*mContext.mModule);
  return true;
}

// This function will take the bitcode emitted by uscc and convert it to assembly
bool Emitter::writeAsm(const char *fileName) noexcept
{
	// 11/20/2014 - This function removed because it doesn't work with LLVM 3.5.0...
	
	return true;
}

void Emitter::doNaturalLoop(const char* fileName) noexcept
{
  llvm::SMDiagnostic Err;
  llvm::LLVMContext myContext;
  llvm::Module* mCurrentModel = ParseIRFile(fileName, Err, myContext);
  if (!mCurrentModel) {
    Err.print("mycompiler", llvm::errs());
  }
  legacy::PassManager pm;
	uscc::opt::registerNaturalLoopPasses(pm);
	pm.run(*mCurrentModel);

}

void Emitter::doPhiRemoval(const char* fileName) noexcept
{
  llvm::SMDiagnostic Err;
  llvm::LLVMContext myContext;
  llvm::Module* mCurrentModel = ParseIRFile(fileName, Err, myContext);
  if (!mCurrentModel) {
    Err.print("mycompiler", llvm::errs());
  }
  legacy::PassManager pm;
    pm.add(createRedundantPhiRemovalPass());
	pm.add(createPrintModulePass(outs()));
	pm.run(*mCurrentModel);

}
void Emitter::enableNaturalLoop()
{

  legacy::PassManager pm;
	uscc::opt::registerNaturalLoopPasses(pm);
	pm.run(*mContext.mModule);

}
void Emitter::registerAnalysis()
{
    PassRegistry *registry = PassRegistry::getPassRegistry();
    uscc::opt::registerAnalysisPasses(*registry);
}

void Emitter::doDCE()
{
    legacy::PassManager pm;
    pm.add(createDCEPass());
    pm.run(*mContext.mModule);
}

void Emitter::doLiveness()
{
  legacy::PassManager pm;
  pm.add(createLivenessPass());
  pm.run(*mContext.mModule);
}

void Emitter::doRDA()
{
  legacy::PassManager pm;
  pm.add(createRDAPass());
  pm.run(*mContext.mModule);
}

void Emitter::identifyUninitializedVars()
{
  legacy::PassManager pm;
  pm.add(createUninitializedVariablesPass());
  pm.run(*mContext.mModule);
}