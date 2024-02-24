/**
 * USC Compiler
 * Jianping Zeng (zeng207@purdue.edu)
 * An iterative forward reaching definition analysis.
 * This pass intends to compute a set of reaching definitions for each LLVM instruction,
 * which is used by UnintializedVariables pass.
*/

#include "Passes.h"
#include "ReachingDefinitions.h"

using namespace std;
using namespace llvm;

bool enableRDA;

char ReachingDefinitions::ID = 0;
INITIALIZE_PASS(ReachingDefinitions, "rda", "reaching definition analysis", true, true)

ReachingDefinitions::ReachingDefinitions() : FunctionPass(ID), inst2Id(), id2Inst(), bb2In(), bb2Out() {
  initializeReachingDefinitionsPass(*PassRegistry::getPassRegistry());
}

FunctionPass *llvm::createRDAPass() {
  return new ReachingDefinitions();
}

bool ReachingDefinitions::hasUninitializedDef(LoadInst &ld) {
  BasicBlock *BB = ld.getParent();
  assert(BB && bb2In.count(BB) && inst2Id.count(&ld) && "load must be attached to a basic block!");
  if (!ld.getPointerOperand()->hasName())
    return false;

  // PA3
  return false;
}

void ReachingDefinitions::dumpReachingDef(Function &F) {
  outs() << "Reaching definition analysis for Function '" << F.getName() << "':\n";
  int id = 0;
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

  for (auto &bb : F) {
    outs() << bb.getName() << ":\n";
    outs() << "  IN:";
    for (auto &id : bb2In[&bb])
      outs() << " " << id;
    outs() << "\n";
    outs() << "  OUT:";
    for (auto &id : bb2Out[&bb])
      outs() << " " << id;
    outs() << "\n";
  }
  outs()<<"\n";
}

bool ReachingDefinitions::isDummyStore(Instruction &inst) {
  StoreInst *si;
  return inst.getParent() && inst.getParent()->getParent() &&
      (si = dyn_cast_or_null<StoreInst>(&inst)) &&
      dummyDefs[inst.getParent()->getParent()].count(si);
}

bool ReachingDefinitions::runOnFunction(Function &F) {
  if (F.empty())
    return false;
  BasicBlock &frontBB = F.front();
  BasicBlock &endBB = F.back();
  assert(!frontBB.empty() && !endBB.empty() && "the front/end basic block must not be empty!");

  // Move all AllocaInst to be at the beginning of the front basic block for easy implementation
  auto pos = frontBB.begin();
  while (pos != frontBB.end() && dyn_cast_or_null<AllocaInst>(&*pos))
    ++pos;

  auto Iter = pos;
  while (Iter != frontBB.end()) {
    if (dyn_cast_or_null<AllocaInst>(&*Iter))
      Iter->moveBefore(pos);
    ++Iter;
  }

  // PA3
  // Step #1: identify program variables.
  set<Value*> namedDefs;

  for(auto &bb : F ){
	  for(auto &inst : bb){
		  if(dyn_cast_or_null<AllocaInst>(&inst) != NULL){
			 namedDefs.insert(&inst); 
		  }
	  }
  }

  // Step #2: insert dummy definitions for each local variable
  std::set<StoreInst*> dummyStores;
  for(auto &bb : F ){
	  for(auto &inst : bb){
		  if(dyn_cast_or_null<AllocaInst>(&inst) != NULL){
			 auto allocaInst = dyn_cast_or_null<AllocaInst>(&inst);
			 auto si = new StoreInst(Constant::getNullValue(allocaInst->getType()->getPointerElementType()), allocaInst);
			 frontBB.getInstList().insert(pos, si);
			 dummyStores.insert(si);
		  }
	  }
  }
  dummyDefs[&F] = dummyStores; 

  // Step #3: find out all definitions of namedDefs variables.
  map<Value*, set<uint32_t>> var2Defs;
  set<uint32_t> setofDefs;
  uint32_t id = 0;
  for(auto &bb : F ){
	  for(auto &inst : bb){
		  inst2Id[&inst] = id;
		  id2Inst[&F].push_back(&inst);
		  if(dyn_cast_or_null<StoreInst>(&inst) != NULL){
			auto operand = dyn_cast_or_null<llvm::StoreInst>(&inst)->getPointerOperand();
			if(namedDefs.find(operand) != namedDefs.end()){
				var2Defs[operand].insert(inst2Id[&inst]);
			}
		  }
		  ++id;
	  }
  }
  
  // Step #4: calculate GEN/KILL set for each basic block
  // rda1 gen 27 and kill 34
  map<BasicBlock *, set<uint32_t>> bb2Kill, bb2Gen;
  set<uint32_t> K, G;
  for(auto &bb : F){
	  for(auto &inst : bb){
		  if(dyn_cast_or_null<StoreInst>(&inst) != NULL){
			auto operand = dyn_cast_or_null<llvm::StoreInst>(&inst)->getPointerOperand();
			if(namedDefs.find(operand) != namedDefs.end()){
				G.insert(inst2Id[&inst]);	
				
				//var2Defs - op
				set_difference(var2Defs[operand].begin(), var2Defs[operand].end(), G.begin(), G.end(), std::inserter(K, K.end()));

				std::set<uint32_t> gen, kill;
				set_difference(bb2Gen[&bb].begin(), bb2Gen[&bb].end(), K.begin(), K.end(), std::inserter(gen, gen.end()));
				bb2Gen[&bb] = gen;
				bb2Gen[&bb].insert(G.begin(), G.end());

				set_difference(bb2Kill[&bb].begin(), bb2Kill[&bb].end(), G.begin(), G.end(), std::inserter(kill, kill.end()));
				bb2Kill[&bb] = kill;
				bb2Kill[&bb].insert(K.begin(), K.end());

			}
		  }
		 K.clear();
		 G.clear();
	  }
  }

  // Step #5: compute reverse post order traversal
	std::vector<BasicBlock*> visited;
	std::vector<BasicBlock*> preorderVisited;
	for(auto &bb : F){
		if(find(preorderVisited.begin(), preorderVisited.end(), &bb) == preorderVisited.end())
			postOrderTraversal(&bb, visited, preorderVisited);
	}

	std::reverse(visited.begin(), visited.end());

  // Step #6: iterate over control flow graph of the input function until the fixed point.
    unsigned cnt = 0;
    unsigned change = 1;
	while(change == 1){
		cnt++;
		change = 0;
		for(auto it=visited.begin(); it!=visited.end(); it++){
			auto old_OUT = bb2Out[*it]; 

			for( auto pred=pred_begin(*it); pred!=pred_end(*it); pred++){
				bb2In[*it].insert(bb2Out[*pred].begin(), bb2Out[*pred].end());
			}

			auto tmp_out = bb2In[*it];
			tmp_out.insert(bb2Gen[*it].begin(), bb2Gen[*it].end());
			set_difference(tmp_out.begin(), tmp_out.end(), bb2Kill[*it].begin(), bb2Kill[*it].end(), std::inserter(bb2Out[*it], bb2Out[*it].end()));
			
			if(old_OUT != bb2Out[*it]){
				change = 1;
			}
		}
	}

  // Step #7: print out IN and OUT set of each basic block.
  if (enableRDA)
    dumpReachingDef(F);

  // ReachingDefinitions does not change the input function at all.
  return false;
}

void ReachingDefinitions::postOrderTraversal(llvm::BasicBlock *current, std::vector<llvm::BasicBlock*> &visited, std::vector<llvm::BasicBlock*> &preorderVisited)
{
	preorderVisited.push_back(current);
	for(auto it = succ_begin(current); it != succ_end(current); ++it){
		if(find(preorderVisited.begin(), preorderVisited.end(), *it) == preorderVisited.end())
			postOrderTraversal(*it, visited, preorderVisited);
	}

	visited.push_back(current);
	return;
}
