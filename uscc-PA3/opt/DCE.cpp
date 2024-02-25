/**
 * USCC Compiler
 * Jianping Zeng (zeng207@purdue.edu)
 * A client of liveness to perform dead code elimination.
 * Note that this pass is different from the Dead Block Elimination that analyzes the reachability of each basic block
 * in a control flow graph (CFG) to determine if the basic block is dead.
*/

#include "Passes.h"
#include "Liveness.h"
#include <iostream>
#include <typeinfo>

using namespace llvm;
namespace 
{
class DeadCodeElimination : public FunctionPass 
{
public:
    static char ID;
    DeadCodeElimination() : FunctionPass(ID) {}
    virtual bool runOnFunction(llvm::Function &F) override;
    void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
private:
    void findDeadDefinitions(Instruction *inst, std::set<Instruction *> &dead);
};
}
char DeadCodeElimination::ID = 0;
FunctionPass *llvm::createDCEPass() 
{
    return new DeadCodeElimination();
}

void DeadCodeElimination::getAnalysisUsage(llvm::AnalysisUsage &AU) const 
{
    AU.addRequired<Liveness>();
    AU.addPreserved<Liveness>();
    // This function call indicates DCE pass does not
    // (1) add/delete basic blocks;
    // (2) remove terminator instruction at the end of each basic block.
    AU.setPreservesCFG();
}

void DeadCodeElimination::findDeadDefinitions(llvm::Instruction *inst,
                                              std::set<Instruction *> &dead) 
{
    for (unsigned i = 0, e = inst->getNumOperands(); i < e; ++i) 
    {
        Value *val = inst->getOperand(i);
        Instruction *src;
        if ((src = dyn_cast_or_null<Instruction>(val)) != nullptr &&
            src->hasOneUse() && src->getOpcode() != Instruction::Call) 
        {
            dead.insert(src);
            findDeadDefinitions(src, dead);
        }
    }
}

bool DeadCodeElimination::runOnFunction(llvm::Function &F) 
{
    if (F.empty())
        return false;

    Liveness &lv = getAnalysisID<Liveness>(&Liveness::ID);
	lv.runOnFunction(F);

	std::set<llvm::Instruction*> deadVars;
	std::set<llvm::Instruction*> deadAllocas;

    // PA3
    // Step #1: get a set of dead instructions and remove them.
	bool changed = true;
	while(changed){
		changed = false;
		for (auto &bb : F){
			for(auto it = bb.begin() ; it != bb.end() ; it++){
				auto &inst = *it;
				if(lv.isDead(inst)){
					deadVars.insert(&inst);
					auto val = dyn_cast_or_null<llvm::StoreInst>(&inst)->getValueOperand();
					if(dyn_cast_or_null<llvm::Instruction>(val)!=NULL){
						deadVars.insert(dyn_cast_or_null<llvm::Instruction>(val));
						std::set<llvm::Instruction*> dead;
						findDeadDefinitions(&inst, dead);
						if(!dead.empty()) deadVars.insert(dead.begin(), dead.end());
					}
				}
			}
		}
		if(!deadVars.empty())
		{ 
			for(auto &inst : deadVars){
				if(!inst->use_empty()){
					inst->replaceAllUsesWith(UndefValue::getNullValue(inst->getType()));
				}
		//		std::cout << "deleted instruction" << '\n';
				inst->eraseFromParent();
			}
			lv.releaseMemory();
			lv.runOnFunction(F);
			changed = true;
			deadVars.clear();
		}
		else std::cout<< "";
		//else std::cout<< "deadVars is empty()" << '\n';
	}
    // Step #2: remove the Alloca instructions having no uses.
		for(auto &bb : F){
			for(auto &inst : bb){
				if(inst.getOpcode() == llvm::Instruction::Alloca && inst.use_empty()){
					//std::cout << "alloca instruction without any use" << '\n';
					deadAllocas.insert(&inst);
				}
			}
		}

		for(auto &inst : deadAllocas){
		//	std::cout << "deleted instruction" << '\n';
			inst->eraseFromParent();
		}
		

    return true;
}
