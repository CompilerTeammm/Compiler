#include "../../include/IR/Opt/CondMerge.hpp"
#include "../../include/lib/MyList.hpp"
#include "../../lib/CFG.hpp"
#include <queue>
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include <string>

// depth 防护常量
static constexpr int CONDMERGE_MAX_DEPTH = 16;


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
        block->EraseFromManager();
        delete block;
    }
    return modified;
}

void CondMerge::OrderBlock(BasicBlock* bb){
    DominantTree _tree(func);
    _tree.BuildDominantTree();
    auto tree=&_tree;
    if(bb->visited){
        return;
    }
    bb->visited=true;
    auto* node = tree->getNode(bb);
    if(!node){
        std::cerr << "[Error] bb not found in DominantTree mapping: " << bb << "\n";
        return;
    }
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
            if (uses.size() < 3) continue;
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
//说明: cond1 是跳到 X 的条件，cond2 也是跳到 X 的条件

bool CondMerge::Handle_Or(BasicBlock *cur, BasicBlock *succ, std::unordered_set<BasicBlock *> &wait_del){
    if (cur == succ){
        return false;     
    }

    bool changed=false;

    if(succ->GetValUseListSize()>1){
        return false;
    }

    const auto& uses = cur->GetBack()->GetUserUseList();

    if (!(uses[1]->usee == succ || uses[2]->usee == succ)) {
        // 不符合预期的分支结构，跳过
        return false;
    }

    auto Cur_Cond=dynamic_cast<CondInst*>(cur->GetBack());
    auto Succ_Cond = dynamic_cast<CondInst *>(succ->GetBack());

    if (Succ_Cond) {
        auto &sUses = Succ_Cond->GetUserUseList();
        if (sUses.size() > 2 && sUses[2]->usee == succ) {
            return false;
        }
    }
    if (Cur_Cond && Succ_Cond){
        auto &curUses = Cur_Cond->GetUserUseList();
        auto &succUses = Succ_Cond->GetUserUseList();

        // 再次保证下标安全：curUses >= 3, succUses >= 3
        if (curUses.size() < 3 || succUses.size() < 3) return false;

        Value *cond1 = curUses[0]->usee;
        Value *cond2 = succUses[0]->usee;

        if (Cur_Cond->GetUserUseList()[1]->usee == Succ_Cond->GetUserUseList()[1]->usee &&
            !Match_Lib_Phi(cur, succ, static_cast<BasicBlock *>(Cur_Cond->GetUserUseList()[1]->usee)) &&
            !DetectCall(cond2, succ, 0) && !RetPhi(static_cast<BasicBlock *>(Cur_Cond->GetUserUseList()[1]->usee))){
                Cur_Cond->ReplaceSomeUseWith(Cur_Cond->GetUserUseList()[2].get(), Succ_Cond->GetUserUseList()[2]->usee);

                BasicBlock *PhiBlock1 = Succ_Cond->GetUserUseList()[1]->usee->as<BasicBlock>();
                BasicBlock *PhiBlock2 = Succ_Cond->GetUserUseList()[2]->usee->as<BasicBlock>();

                Succ_Cond->ClearRelation();
                Succ_Cond->EraseFromManager();

                // 将 succ 中的指令移动到 cur（先收集，再移动，避免迭代时修改容器）
                std::vector<Instruction*> toMove;
                for (auto it = succ->begin(); it != succ->end(); ++it) {
                    toMove.push_back(*it);
                }
                List<BasicBlock, Instruction>::iterator Cur_iter(Cur_Cond);

                for (auto inst : toMove) {
                    inst->EraseFromManager();
                    if (auto phi = dynamic_cast<PhiInst *>(inst)) {
                        phi->removeIncomingFrom(cur);
                        // phi 插入到 cur 的前面（保持 phi 在块首）
                        cur->push_front(phi);
                    } else {
                        Cur_iter.InsertBefore(inst);
                    }
                }
                auto binary_or = new BinaryInst(cond1, BinaryInst::Op_Or, cond2);
                Cur_iter.InsertBefore(binary_or);
                Cur_Cond->ReplaceSomeUseWith(Cur_Cond->GetUserUseList()[0].get(), binary_or);
                if (PhiBlock1) {
                    for (auto phi_it = PhiBlock1->begin(); phi_it != PhiBlock1->end(); ++phi_it) {
                        if (auto phi = dynamic_cast<PhiInst *>(*phi_it)) {
                            phi->ReplaceIncomingBlock(succ, cur);
                        } else break;
                    }
                }
                if (PhiBlock2) {
                    for (auto phi_it = PhiBlock2->begin(); phi_it != PhiBlock2->end(); ++phi_it) {
                        if (auto phi = dynamic_cast<PhiInst *>(*phi_it)) {
                            phi->ReplaceIncomingBlock(succ, cur);
                        } else break;
                    }
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
    auto *curTerm = cur->GetBack();
    if (!curTerm) return false;
    const auto& uses = curTerm->GetUserUseList();

    if (uses.size() < 3 || !(uses[1]->usee == succ || uses[2]->usee == succ)) {
        return false;
    }
    auto Cur_Cond = dynamic_cast<CondInst *>(cur->GetBack());
    auto Succ_Cond = dynamic_cast<CondInst *>(succ->GetBack());

    if (Succ_Cond) {
        auto &sUses = Succ_Cond->GetUserUseList();
        if (sUses.size() > 1 && sUses[1]->usee == succ) {
            return false;
        }
    }
    bool changed = false;
    if (Cur_Cond && Succ_Cond) {
        auto &curUses = Cur_Cond->GetUserUseList();
        auto &succUses = Succ_Cond->GetUserUseList();
        if (curUses.size() < 3 || succUses.size() < 3) return false;

        Value *cond1 = curUses[0]->usee;
        Value *cond2 = succUses[0]->usee;

        if (Cur_Cond->GetUserUseList()[2]->usee == Succ_Cond->GetUserUseList()[2]->usee &&
            !Match_Lib_Phi(cur, succ, static_cast<BasicBlock *>(Cur_Cond->GetUserUseList()[2]->usee)) &&
            !DetectCall(cond2, succ, 0)) {

            // 替换Cur_Cond第一个操作数
            Cur_Cond->ReplaceSomeUseWith(Cur_Cond->GetUserUseList()[1].get(), Succ_Cond->GetUserUseList()[1]->usee);

            Succ_Cond->ClearRelation();
            Succ_Cond->EraseFromManager();

            BasicBlock *PhiBlock1 = Succ_Cond->GetUserUseList()[1]->usee->as<BasicBlock>();
            BasicBlock *PhiBlock2 = Succ_Cond->GetUserUseList()[2]->usee->as<BasicBlock>();

            std::vector<Instruction*> toMove;
            for (auto it = succ->begin(); it != succ->end(); ++it) {
                toMove.push_back(*it);
            }
            List<BasicBlock, Instruction>::iterator Cur_iter(Cur_Cond);
            for (auto inst : toMove) {
                inst->EraseFromManager();
                if (auto phi = dynamic_cast<PhiInst *>(inst)) {
                    phi->removeIncomingFrom(cur);
                    cur->push_front(phi);
                } else {
                    Cur_iter.InsertBefore(inst);
                }
            }

            auto binary_and = new BinaryInst(cond1, BinaryInst::Op_And, cond2);
            Cur_iter.InsertBefore(binary_and);

            Cur_Cond->ReplaceSomeUseWith(Cur_Cond->GetUserUseList()[0].get(), binary_and);

            if (PhiBlock1) {
                for (auto phi_it = PhiBlock1->begin(); phi_it != PhiBlock1->end(); ++phi_it) {
                    if (auto phi = dynamic_cast<PhiInst *>(*phi_it)) {
                        phi->ReplaceIncomingBlock(succ, cur);
                    } else break;
                }
            }
            if (PhiBlock2) {
                for (auto phi_it = PhiBlock2->begin(); phi_it != PhiBlock2->end(); ++phi_it) {
                    if (auto phi = dynamic_cast<PhiInst *>(*phi_it)) {
                        phi->ReplaceIncomingBlock(succ, cur);
                    } else break;
                }
            }

            wait_del.insert(succ);
            succ->ReplaceAllUseWith(cur);

            changed = true;
        }
    }
    return changed;
}


bool CondMerge::DetectCall(Value *val, BasicBlock *block, int depth){
    std::unordered_set<Value*> visited;
    std::function<bool(Value*, BasicBlock*, int)> dfs = [&](Value *v, BasicBlock *blk, int d)->bool{
        if (!v) return false;
        if (d > CONDMERGE_MAX_DEPTH) return false;
        if (visited.count(v)) return false;
        visited.insert(v);

        auto user = dynamic_cast<User *>(v);
        if (!user) return false;

        auto inst = dynamic_cast<Instruction*>(user);
        if (!inst) return false;
        // 只关心位于目标块的指令（保持与学长实现一致的防护）
        if (inst->GetParent() != blk) return false;

        // 如果本身是调用指令则有副作用
        if (dynamic_cast<CallInst *>(inst)) return true;

        auto &uses = user->GetUserUseList();
        for (size_t i = 0; i < uses.size(); ++i) {
            Value *op = uses[i]->usee;
            if (dfs(op, blk, d + 1)) return true;
        }
        return false;
    };

    return dfs(val, block, depth);
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

// DetectUserPos：检查 from_cur 在 succ 中的用户链上是否等价/传播到 from_succ。
// 增加 visited 防环
bool CondMerge::DetectUserPos(Value *userVal, BasicBlock *blockpos, Value *val) {
    std::unordered_set<Value*> visited;
    std::function<bool(Value*, BasicBlock*, Value*)> dfs = [&](Value *uVal, BasicBlock *bpos, Value *target)->bool{
        if (!uVal) return false;
        if (visited.count(uVal)) return false;
        visited.insert(uVal);

        auto userNode = dynamic_cast<User *>(uVal);
        if (!userNode) return false;

        auto &uses = userNode->GetUserUseList();
        for (auto &uPtr : uses) {
            User *u = uPtr->GetUser();
            if (!u) continue;

            if (u == target) return true;

            auto inst = dynamic_cast<Instruction*>(u);
            if (inst && inst->GetParent() == bpos) {
                if (dfs(u, bpos, target)) return true;
            }
        }
        return false;
    };

    return dfs(userVal, blockpos, val);
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
  
      // PhiRecord 结构是 map<int, pair<Value*, BasicBlock*>>
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