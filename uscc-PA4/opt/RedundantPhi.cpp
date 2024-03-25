#include "Graph.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"

namespace llvm {
namespace uscc {
namespace opt {

class RedundantPhiRemoval : public FunctionPass {
public:
  static char ID;
  RedundantPhiRemoval() : FunctionPass(ID) {}
   
  Graph* inducedSubGraph = new Graph();
 
  std::set<Value*> phiFunctions;
  std::vector<std::set<llvm::Value *>> sccsSet;

  virtual bool runOnFunction(llvm::Function &F) override;
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  void processSCC(std::set<Value*> scc);
  void removeRedundantPhis(std::set<Value*>);
  void replaceSCCByValue(std::set<Value*> scc, Value* value);
};



char RedundantPhiRemoval::ID = 0;

void RedundantPhiRemoval::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.setPreservesCFG();
}


bool RedundantPhiRemoval::runOnFunction(Function &F) {

  // PA4: Implement
  //
  // This is the entry point of the redudant phi removal pass.
  // Your implementation should start here.
  // Feel free to add functions to the RedundantPhiRemoval class.

    if (F.empty())
        return false;
  
	for (auto &bb : F){
		for(auto &inst : bb){
			auto phiNode = dyn_cast_or_null<llvm::PHINode>(&inst); 
			if(phiNode)
				phiFunctions.insert(phiNode);		
		}
	}
	
	removeRedundantPhis(phiFunctions);

    return true;
}

void RedundantPhiRemoval::removeRedundantPhis(std::set<Value*> phiFunctions){

	inducedSubGraph->eraseAllNodes();
	
	std::vector<std::set<Value*>> sccs;
	for(auto it = phiFunctions.begin(); it != phiFunctions.end(); ++it){
		auto node = inducedSubGraph->getOrAddToGraph(*it); 
		auto phiNode = dyn_cast_or_null<PHINode>(*it);
		unsigned numOfIncomingValues = phiNode->getNumIncomingValues();
		for(unsigned i=0; i<numOfIncomingValues; i++){
			auto operand = phiNode->getIncomingValue(i);
			if(find(phiFunctions.begin(), phiFunctions.end(), operand) != phiFunctions.end()){
				auto operandNode = inducedSubGraph->getOrAddToGraph(operand);
				node->addEdge(operandNode);
			}
		}
	}

    sccs = inducedSubGraph->getSCCs(); 
	for(auto scc = sccs.begin(); scc != sccs.end(); ++scc){
		processSCC(*scc);
	}

}

void RedundantPhiRemoval::processSCC(std::set<Value*> scc){
	
	//std::vector<Value*> inner;
	//std::vector<Value*> outerOps;
	std::set<Value*> inner;
	std::set<Value*> outerOps;
		
	for(auto phi = scc.begin(); phi !=scc.end(); ++phi){
		bool isInner = true;
		auto phiNode = dyn_cast_or_null<PHINode>(*phi);
		unsigned numOfIncomingValues = phiNode->getNumIncomingValues();
		for(unsigned i=0; i<numOfIncomingValues; i++){
			auto operand = phiNode->getIncomingValue(i);
			if(scc.find(operand) == scc.end()){
				outerOps.insert(operand);
				isInner = false;
			}
		}
		if(isInner)
			inner.insert(*phi);
	}

	if(outerOps.size() == 1){
		replaceSCCByValue(scc, *outerOps.begin());
	}
	else if(outerOps.size() > 1){
		removeRedundantPhis(inner);
	}

}

void RedundantPhiRemoval::replaceSCCByValue(std::set<Value*> scc, Value* value){
	
	for(auto phi = scc.begin(); phi != scc.end(); ++phi){
		auto phiNode = dyn_cast_or_null<PHINode>(*phi);
		phiNode->replaceAllUsesWith(value);
		phiNode->eraseFromParent();
		//inducedSubGraph->eraseFromGraph(*phi);
	}
}

} // namespace opt
} // namespace uscc

FunctionPass *createRedundantPhiRemovalPass() {
  return new uscc::opt::RedundantPhiRemoval();
}

} // namespace llvm
