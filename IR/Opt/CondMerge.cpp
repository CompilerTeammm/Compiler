#include "../../include/IR/Opt/CondMerge.hpp"
#include "../../include/lib/MyList.hpp"
#include "../../lib/CFG.hpp"
bool CondMerge::run(){
    bool modified = false;
    bool changed = true;

    func->init_visited_block();
    OrderBlock(func->front);
    std::reverse(DFSOrder.begin(),DFSOrder.end());

    while(changed){
        changed = false;
        changed |= AdjustCondition();
        modified |= changed;
    }
    while (!wait_del.empty()){
        BasicBlock *block = *wait_del.begin();
        wait_del.erase(block);
        delete block;
    }
    return modified;
}

void CondMerge::OrderBlock(BasicBlock* bb){
    if(bb->visited){
        return;
    }
    bb->visited=true;
    auto* node = tree->getNode(bb);
    for (auto succNode : node->succNodes) {
        OrderBlock(succNode->curBlock);
    }

    DFSOrder.push_back(bb);
}

bool CondMerge::AdjustCondition(){
    bool changed=false;
    for (BasicBlock *block : DFSOrder){
        if (wait_del.count(block)){
            continue;
        }
        auto* br=block->GetBack();
        if(br && dynamic_cast<CondInst*>(br)){
            const auto& uses = br->GetUserUseList();
            BasicBlock *succ_and = uses[1]->usee->as<BasicBlock>();
            BasicBlock *succ_or = uses[2]->usee->as<BasicBlock>();

            if(Handle_And(block,succ_and,wait_del)){
                changed=true;
            }else if(Handle_Or(block,succ_or,wait_del)){
                changed=true;
            }
        }
    }
    return changed;
}

//cur:  if (cond1) goto X else goto succ
//succ: if (cond2) goto X else goto Y
//优化成: cur:  if (cond1 || cond2) goto X else goto Y
bool CondMerge::Handle_Or(BasicBlock *cur, BasicBlock *succ, std::unordered_set<BasicBlock *> &wait_del){
    if (cur == succ){
        return false;     
    }

    bool changed=false;

    if(succ->GetValUseListSize()>1){
        return false;
    }

    const auto& uses = cur->GetBack()->GetUserUseList();

    if (uses.size() <= 2 || !(uses[1]->usee == succ || uses[2]->usee == succ)) {
        throw std::runtime_error(
            "[SelfStoreElimination] Fatal: succ not found in terminator instruction."
        );
    }

    auto Cur_Cond=dynamic_cast<CondInst*>(cur->GetBack());
    auto Succ_Cond = dynamic_cast<CondInst *>(succ->GetBack());

    if (Succ_Cond && Succ_Cond->GetUserUseList()[2]->usee == succ){
        return false;
    }
    if (Cur_Cond && Succ_Cond){
        Value *cond1 = Cur_Cond->GetUserUseList()[0]->usee;
        Value *cond2 = Succ_Cond->GetUserUseList()[0]->usee;

        if (Cur_Cond->GetUserUseList()[1]->usee == Succ_Cond->GetUserUseList()[1]->usee &&
            !Match_Lib_Phi(cur, succ, static_cast<BasicBlock *>(Cur_Cond->GetUserUseList()[1]->usee)) &&
            !DetectCall(cond2, succ, 0) && !RetPhi(static_cast<BasicBlock *>(Cur_Cond->GetUserUseList()[1]->usee))){
                Cur_Cond->ReplaceSomeUseWith(Cur_Cond->GetUserUseList()[2].get(), Succ_Cond->GetUserUseList()[2]->usee);

                BasicBlock *PhiBlock1 = Succ_Cond->GetUserUseList()[1]->usee->as<BasicBlock>();
                BasicBlock *PhiBlock2 = Succ_Cond->GetUserUseList()[2]->usee->as<BasicBlock>();

                Succ_Cond->ClearRelation();
                Succ_Cond->EraseFromManager();

                List<BasicBlock, Instruction>::iterator Cur_iter(Cur_Cond);

                for (auto it = succ->begin(); it != succ->end();){
                    (*it)->EraseFromManager();
                    if(auto phi=dynamic_cast<PhiInst *>(*it)){
                        phi->removeIncomingFrom(cur);
                        cur->push_front(phi);
                    }else{
                        Cur_iter.InsertBefore(*it);
                    }
                    it=succ->begin();
                }
                auto binary_or = new BinaryInst(cond1, BinaryInst::Op_Or, cond2);
                Cur_iter.InsertBefore(binary_or);
                Cur_Cond->ReplaceSomeUseWith(Cur_Cond->GetUserUseList()[0].get(), binary_or);
                auto phi_iter = PhiBlock1->begin();
                while (auto phi = dynamic_cast<PhiInst *>(*phi_iter)){
                    phi->ReplaceIncomingBlock(succ,cur);
                    ++phi_iter;
                }
                phi_iter = PhiBlock2->begin();
                while (auto phi = dynamic_cast<PhiInst *>(*phi_iter)){
                    phi->ReplaceIncomingBlock(succ,cur);
                    ++phi_iter;
                }
                wait_del.insert(succ);
                succ->ReplaceAllUseWith(cur);
                changed=true;
            }
    }
    return changed;
}
bool CondMerge::Handle_And(BasicBlock *cur, BasicBlock *succ, std::unordered_set<BasicBlock *> &wait_del){
    if (cur == succ) {
        return false;
    }

    if (succ->GetValUseListSize() > 1) {
        return false;
    }
    const auto& uses = cur->GetBack()->GetUserUseList();

    if (uses.size() < 3 || !(uses[1]->usee == succ || uses[2]->usee == succ)) {
        throw std::runtime_error(
            "[CondMerge::Handle_And] Fatal: succ not found in terminator instruction."
        );
    }
    auto Cur_Cond = dynamic_cast<CondInst *>(cur->GetBack());
    auto Succ_Cond = dynamic_cast<CondInst *>(succ->GetBack());

    if (Succ_Cond && Succ_Cond->GetUserUseList().size() > 1 && Succ_Cond->GetUserUseList()[1]->usee == succ) {
        return false;
    }
    bool changed = false;
    if (Cur_Cond && Succ_Cond) {
        Value *cond1 = Cur_Cond->GetUserUseList()[0]->usee;
        Value *cond2 = Succ_Cond->GetUserUseList()[0]->usee;

        if (Cur_Cond->GetUserUseList()[2]->usee == Succ_Cond->GetUserUseList()[2]->usee &&
            !Match_Lib_Phi(cur, succ, static_cast<BasicBlock *>(Cur_Cond->GetUserUseList()[2]->usee)) &&
            !DetectCall(cond2, succ, 0)) {

            // 替换Cur_Cond第一个操作数
            Cur_Cond->ReplaceSomeUseWith(Cur_Cond->GetUserUseList()[1].get(), Succ_Cond->GetUserUseList()[1]->usee);

            Succ_Cond->ClearRelation();
            Succ_Cond->EraseFromManager();

            BasicBlock *PhiBlock1 = Succ_Cond->GetUserUseList()[1]->usee->as<BasicBlock>();
            BasicBlock *PhiBlock2 = Succ_Cond->GetUserUseList()[2]->usee->as<BasicBlock>();

            List<BasicBlock, Instruction>::iterator Cur_iter(Cur_Cond);

            for (auto it = succ->begin(); it != succ->end();) {
                (*it)->EraseFromManager();
                if (auto phi = dynamic_cast<PhiInst *>(*it)) {
                    phi->removeIncomingFrom(cur);
                    cur->push_front(phi);
                } else {
                    Cur_iter.InsertBefore(*it);
                }
                it = succ->begin();
            }

            auto binary_and = new BinaryInst(cond1, BinaryInst::Op_And, cond2);
            Cur_iter.InsertBefore(binary_and);

            Cur_Cond->ReplaceSomeUseWith(Cur_Cond->GetUserUseList()[0].get(), binary_and);

            auto phi_iter = PhiBlock1->begin();
            while (auto phi = dynamic_cast<PhiInst *>(*phi_iter)) {
                phi->ReplaceIncomingBlock(succ, cur);
                ++phi_iter;
            }
            phi_iter = PhiBlock2->begin();
            while (auto phi = dynamic_cast<PhiInst *>(*phi_iter)) {
                phi->ReplaceIncomingBlock(succ, cur);
                ++phi_iter;
            }

            wait_del.insert(succ);
            succ->ReplaceAllUseWith(cur);

            changed = true;
        }
    }
    return changed;
}
// depth 防护常量
static constexpr int CONDMERGE_MAX_DEPTH = 16;

bool CondMerge::DetectCall(Value *val, BasicBlock *block, int depth){
// 不是 User（即匿名常量/全局等）直接返回 false
  auto user = dynamic_cast<User *>(val);
  if (!user) return false;

  if (depth > CONDMERGE_MAX_DEPTH) return false;

  auto inst = dynamic_cast<Instruction*>(user);
  if (!inst) return false;
  if(inst->GetParent()!=block){
    return false;
  }
  // 如果本身就是调用指令，说明有副作用
  if (dynamic_cast<CallInst *>(val)) return true;
  auto &uses = user->GetUserUseList();
  for (size_t i = 0; i < uses.size(); ++i) {
    Value *op = uses[i]->usee;
    if (DetectCall(op, block, depth + 1)) return true;
  }
  return false;
}
bool CondMerge::RetPhi(BasicBlock *block) {
    // 遍历 basic block 开头的 phi 指令（若遇到非 phi 即停止）
    for (auto it = block->begin(); it != block->end(); ++it) {
      auto phi = dynamic_cast<PhiInst *>(*it);
      if (!phi) break;
      auto &userUses = phi->GetUserUseList();
      for (auto &uPtr : userUses) {
        User *userInst = uPtr->GetUser(); // Use -> user 指令
        if (dynamic_cast<RetInst *>(userInst)) return true;
      }
    }
    return false;
}

bool CondMerge::DetectUserPos(Value *userVal, BasicBlock *blockpos, Value *val) {
    // 防护：非 User 则返回 false
    auto userNode = dynamic_cast<User *>(userVal);
    if (!userNode) return false;
  
    // 遍历 userVal 的 use 列表（即谁在用这个 value）
    auto &uses = userNode->GetUserUseList(); // vector<unique_ptr<Use>> &
    for (auto &uPtr : uses) {
      User *u = uPtr->GetUser();
      if (!u) continue;
  
      // 直接是目标值（可能是某个 Instruction*），则命中
      if (u == val) return true;
  
      // 如果这个使用者位于目标基本块内，则继续沿着使用链向下搜索
      auto inst = dynamic_cast<Instruction*>(u);
      if (inst->GetParent() == blockpos) {
        if (DetectUserPos(u, blockpos, val)) return true;
      }
    }
    return false;
}
bool CondMerge::Match_Lib_Phi(BasicBlock *curr, BasicBlock *succ, BasicBlock *exit) {
    // 1) 检查 succ 中是否含有我们关心的库函数调用（有副作用）
    for (auto instPtr = succ->begin(); instPtr != succ->end(); ++instPtr) {
      if (auto call = dynamic_cast<CallInst *>(*instPtr)) {
        // 假定调用的第一个操作数是 callee name operand（和你学长代码一致）
        auto calleeOp = call->GetOperand(0);
        if (calleeOp) {
          std::string name = calleeOp->GetName();
          // 注意：修正原来代码中误用的位或 '|' 为逻辑或 '||'
          if (name == "putch" || name == "putint" || name == "putfloat" ||
              name == "putarray" || name == "putfarray" || name == "putf" ||
              name == "getint" || name == "getch" || name == "getfloat" ||
              name == "getfarray" || name == "getarray" ||
              name == "_sysy_starttime" || name == "_sysy_stoptime" ||
              name == "llvm.memcpy.p0.p0.i32") {
            return true;
          }
        }
      }
    }
  
    // 2) 检查 exit 块内 phi 节点：若 phi 从 curr 和 succ 两个分支来的值相同（或在 succ 中的用户链条上等价），阻止合并
    for (auto instPtr = exit->begin(); instPtr != exit->end(); ++instPtr) {
      auto phi = dynamic_cast<PhiInst *>(*instPtr);
      if (!phi) continue;
  
      Value *from_cur = nullptr;
      Value *from_succ = nullptr;
  
      // 你的 PhiRecord 结构是 map<int, pair<Value*, BasicBlock*>>
      for (auto &entry : phi->PhiRecord) {
        Value *v = entry.second.first;
        BasicBlock *bb = entry.second.second;
        if (bb == curr) {
          from_cur = v;
          if (from_succ) break;
        }
        if (bb == succ) {
          from_succ = v;
          if (from_cur) break;
        }
      }
  
      if (from_cur && from_succ) {
        if (from_cur == from_succ) return true;
        if (DetectUserPos(from_cur, succ, from_succ)) return true;
      }
    }
    return false;
  }