/**
 * USCC Compiler
 * Jianping Zeng (zeng207@purdue.edu)
 * An iterative backward liveness analysis.
 * This pass intends to compute a set of live-out/in variables for each LLVM basic block
 * and maintain a set of LLVM instructions that are dead---not used by any following others.
*/

#include "Liveness.h"
#include <iostream>

using namespace std;
using namespace llvm;

bool enableLiveness;

char Liveness::ID = 0;
INITIALIZE_PASS(Liveness, "liveness", "Liveness Analysis", true, true)

FunctionPass *llvm::createLivenessPass() 
{
    return new Liveness();
}

bool Liveness::runOnFunction(Function &F) 
{
    if (F.empty())
        return false;
    BasicBlock &frontBB = F.front();
    BasicBlock &endBB = F.back();
    assert(!frontBB.empty() && !endBB.empty() && "the front/end basic block must not be empty!");
    // The OUT set of the last block is empty.
    bb2Out[&endBB] = std::set<StringRef>();

    // PA3
    // Step #1: identify program variables.
	for (auto &bb : F){
		for(auto &inst : bb){
			if(inst.getOpcode() == llvm::Instruction::Alloca && inst.hasName())
				namedVars.insert(inst.getName());		
		}
	}

    // Step #2: calculate DEF/USE set for each basic block
	std::map<BasicBlock*, std::set<StringRef>> bb2Use;
	std::map<BasicBlock*, std::set<StringRef>> bb2Def;
	std::set<StringRef> Use; 
	std::set<StringRef> Def; 

	for(auto &bb : F){
		for(auto it = bb.rbegin() ; it != bb.rend() ; it++){
			auto &inst = *it;
			if(inst.getOpcode() == llvm::Instruction::Load){
				auto operand = dyn_cast_or_null<llvm::LoadInst>(&inst)->getPointerOperand();
				if(namedVars.find(operand->getName()) != namedVars.end()){
					if(Def.find(operand->getName()) != Def.end()) Def.erase(operand->getName());
					Use.insert(operand->getName());				 
				}
			}
			if(inst.getOpcode() == llvm::Instruction::Store){
				auto operand = dyn_cast_or_null<llvm::StoreInst>(&inst)->getPointerOperand();
				if(namedVars.find(operand->getName()) != namedVars.end()){
					if(Use.find(operand->getName()) != Use.end()) Use.erase(operand->getName());
					Def.insert(operand->getName());				 
				}
			}
		}
		bb2Use[&bb] = Use;
		bb2Def[&bb] = Def;
		Use.clear();
		Def.clear();
	}


    // Step #3: compute post order traversal.
	std::vector<BasicBlock*> visited;
	std::vector<BasicBlock*> preorderVisited;
	for(auto &bb : F){
		if(find(preorderVisited.begin(), preorderVisited.end(), &bb) == preorderVisited.end())
			postOrderTraversal(&bb, visited, preorderVisited);
	}

    // Step #4: iterate over control flow graph of the input function until the fixed point.
    unsigned cnt = 0;
    unsigned change = 1;
	while(change == 1){
		cnt++;
		change = 0;
		for(auto it=visited.begin(); it!=visited.end(); it++){
			std::set<StringRef>old_IN = bb2In[*it]; 

			for( auto successor=succ_begin(*it); successor!=succ_end(*it); successor++){
				bb2Out[*it].insert(bb2In[*successor].begin(), bb2In[*successor].end());
			}

			auto tmp_out = bb2Out[*it];
			tmp_out.insert(bb2Use[*it].begin(), bb2Use[*it].end());
			set_difference(tmp_out.begin(), tmp_out.end(), bb2Def[*it].begin(), bb2Def[*it].end(), std::inserter(bb2In[*it], bb2In[*it].end()));
			
			if(old_IN != bb2In[*it]){
				change = 1;
			}
		}
	}


    // Step #5: output IN/OUT set for each basic block.
    if (enableLiveness) 
    {
        llvm::outs() << "********** Live-in/Live-out information **********\n";
        llvm::outs() << "********** Function: " << F.getName().str() << ", analysis iterates " << cnt << " times\n";
        for (auto &bb : F) 
        {
            llvm::outs() << bb.getName() << ":\n";
            llvm::outs() << "  IN:";
            for (auto &var : bb2In[&bb])
                llvm::outs() << " " << var.substr(0, var.size() - 5);
            llvm::outs() << "\n";
            llvm::outs() << "  OUT:";
            for (auto &var : bb2Out[&bb])
                llvm::outs() << " " << var.substr(0, var.size() - 5);
            llvm::outs() << "\n";
        }
    }
    // Liveness does not change the input function at all.
    return false;
}

void Liveness::postOrderTraversal(llvm::BasicBlock *current, std::vector<llvm::BasicBlock*> &visited, std::vector<llvm::BasicBlock*> &preorderVisited)
{
	preorderVisited.push_back(current);
	for(auto it = succ_begin(current); it != succ_end(current); ++it){
		if(find(preorderVisited.begin(), preorderVisited.end(), *it) == preorderVisited.end())
			postOrderTraversal(*it, visited, preorderVisited);
	}

	visited.push_back(current);
	return;
}

void Liveness::isLiveWithinBB(llvm::Instruction &inst, BasicBlock* bb, std::set<StringRef> &LiveWithinBB){

	BasicBlock::iterator it(bb->end());
	BasicBlock::iterator it_end(inst);

	//auto it = bb->end();
	//auto &currInst = *it;
	//while(!inst.isIdenticalTo(&currInst)){
	while(it != it_end){
	//for(auto it = bb->rbegin() ; it !=  ; it++){
		auto &currInst = *it;
		if(currInst.getOpcode() == llvm::Instruction::Load){
			auto operand = dyn_cast_or_null<llvm::LoadInst>(&currInst)->getPointerOperand();
			if(namedVars.find(operand->getName()) != namedVars.end()){
				LiveWithinBB.insert(operand->getName());				 
			}
		}
		if(currInst.getOpcode() == llvm::Instruction::Store){
			auto operand = dyn_cast_or_null<llvm::StoreInst>(&currInst)->getPointerOperand();
			if(namedVars.find(operand->getName()) != namedVars.end()){
				if(!LiveWithinBB.empty()) LiveWithinBB.erase(operand->getName());				 
			}
		}
		--it;
	}
	
	return;
	
}

bool Liveness::isDead(llvm::Instruction &inst) 
{
	//std::cout << "Hey" << '\n';
    BasicBlock *bb = inst.getParent();
    if (!bb)
        return true;
    if (!bb2Out.count(bb))
        return true;

    // PA3
	if(inst.getOpcode() == llvm::Instruction::Store){
		auto operand = dyn_cast_or_null<llvm::StoreInst>(&inst)->getPointerOperand();
		if(namedVars.find(operand->getName()) != namedVars.end()){
			//Then do the analysis for whether the instruction is dead or not.
			std::set<StringRef> LiveWithinBB = bb2Out[bb];
			isLiveWithinBB(inst, bb, LiveWithinBB);
			if(LiveWithinBB.find(operand->getName())==LiveWithinBB.end()){
				return true;
			}
			else return false;
		}
	}
	
    return false;
}
