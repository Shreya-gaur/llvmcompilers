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
private:
	std::map<Function*, std::set<StoreInst*>> dummyDefs;

public:
  static char ID;
  UninitializedVariables() : FunctionPass(ID) {
    initializeUninitializedVariablesPass(*PassRegistry::getPassRegistry());
  }
  bool runOnFunction(llvm::Function &F) override;
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  void dumpUninitializedVars(Function &F); 
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

void UninitializedVariables::dumpUninitializedVars(Function &F) {

  uint32_t id = 0;
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

}

FunctionPass *llvm::createUninitializedVariablesPass() {
  return new UninitializedVariables();
}


bool UninitializedVariables::runOnFunction(llvm::Function &F) {
  if (F.empty()) return false;

  outs() << "Function '" << F.getName() << "':\n";
  ReachingDefinitions &rda = getAnalysis<ReachingDefinitions>();

  // PA3
  for(auto &bb : F){
	  for(auto &inst : bb){
		  if(dyn_cast_or_null<StoreInst>(&inst) != NULL){
			  if(rda.isDummyStore(inst))
				  dummyDefs[&F].insert(dyn_cast_or_null<StoreInst>(&inst));
		  } 
  	   }
  }

  dumpUninitializedVars(F);

  outs() << "Use of Uninitialized variables: {";

  uint32_t id=0; 
  bool spflag = false;
  for(auto &bb : F){
	  for(auto &inst : bb){
		  if(dyn_cast_or_null<LoadInst>(&inst) != NULL){
			  auto ld = dyn_cast_or_null<LoadInst>(&inst);
			  if(rda.hasUninitializedDef(*ld)){ 
				  if(spflag == true) outs() << " ";
				  outs() << id;	  
			  	  spflag = true;
  	  	  	  }
		  }
		  ++id;
  	   }
  }

  outs() << "}" << '\n';
  outs() << '\n';

  return true;
}
