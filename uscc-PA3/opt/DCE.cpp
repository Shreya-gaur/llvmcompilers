/**
 * USCC Compiler
 * Jianping Zeng (zeng207@purdue.edu)
 * A client of liveness to perform dead code elimination.
 * Note that this pass is different from the Dead Block Elimination that analyzes the reachability of each basic block
 * in a control flow graph (CFG) to determine if the basic block is dead.
*/

#include "Passes.h"
#include "Liveness.h"

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

    // PA3
    // Step #1: get a set of dead instructions and remove them.
    
    // Step #2: remove the Alloca instructions having no uses.
    return false;
}