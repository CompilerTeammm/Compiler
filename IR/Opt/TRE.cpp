#include "../../include/IR/Opt/TRE.hpp"
std::vector<std::pair<CallInst*, RetInst*>> TRE::TailCallPairs(Function* func) {
    std::vector<std::pair<CallInst*, RetInst*>> worklist;

    auto usrlist = func->GetValUseList();
    for (auto usr : usrlist) {
        auto userInst = usr->GetUser();
        if (!userInst) continue;  // 防空

        auto call = userInst->as<CallInst>();
        if (!call) continue;  // 非调用指令跳过

        // 判断call属于当前函数
        if (call->GetParent() && call->GetParent()->GetParent() == func) {
            auto nextInst = call->GetNextNode();
            if (!nextInst) continue;  // 防空

            auto ret = nextInst->as<RetInst>();
            if (!ret) continue;

            auto retOp = ret->GetOperand(0);
            // 满足以下任一条件则认为是尾递归
            if (ret->GetValUseListSize() == 0 
                || (retOp && retOp->IsUndefVal())
                || (retOp == call)) {
                worklist.emplace_back(call, ret);
            }
        }
    }

    return worklist;
}

std::pair<BasicBlock*, BasicBlock*> TRE::HoistAllocas() {
    auto entry = func->GetFront();  // 入口块
    Instruction* lastAllocaInst = nullptr;

    // 找到入口块中连续的Alloca指令的最后一个
    for (auto inst : *entry) {
        if (inst->as<AllocaInst>() != nullptr) {
            lastAllocaInst = inst;
        } else {
            break;
        }
    }

    // 如果没有alloca指令，则新建基本块，跳转到入口块
    if (!lastAllocaInst) {
        auto newBlock = new BasicBlock();
        newBlock->push_back(new UnCondInst(entry));
        func->push_front(newBlock);
        return {newBlock, entry};
    }

    // 获取最后一个alloca指令的下一条指令
    Instruction* splitInst = lastAllocaInst->GetNextNode();
    if (!splitInst) {
        // 这里可能函数只有alloca指令，没有后续指令，异常处理
        // 可以直接返回或抛异常，根据项目约定
        return {entry, nullptr};
    }

    // 在splitInst处分割入口块，生成新块
    auto newBlock = entry->SplitAt(splitInst);

    // 在入口块末尾添加无条件跳转到新块
    entry->push_back(new UnCondInst(newBlock));

    // 在函数基本块链表中插入新块，紧跟入口块之后
    auto funcIt = List<Function, BasicBlock>::iterator(entry);
    funcIt.InsertAfter(newBlock);

    return {entry, newBlock};
}


bool TRE::run() {
    auto worklist = TailCallPairs(func);
    if (worklist.empty())
        return false;

    auto [entry, jump_dst] = HoistAllocas();

    auto& params = func->GetParams();
    std::vector<PhiInst*> paramPhis;

    // 为每个参数创建Phi节点，替换原有参数使用
    for (auto& param : params) {
        auto newPhi = new PhiInst(param->GetType());
        param->ReplaceAllUseWith(newPhi);
        newPhi->addIncoming(param.get(), entry);
        paramPhis.push_back(newPhi);
    }

    for (auto [call, ret] : worklist) {
        auto src = call->GetParent();
        size_t size = call->GetUserUseList().size() - 1;

        // 保证调用参数数目和phi节点数一致
        assert(size == paramPhis.size() && "Frontend guarantees this condition");

        for (size_t i = 0; i < size; ++i) {
            paramPhis[i]->addIncoming(call->GetOperand(i + 1), src);
        }

        // 用无条件跳转指令替换返回指令，实现循环跳转
        auto newUncond = new UnCondInst(jump_dst);
        ret->InstReplace(newUncond);

        // 注意：直接delete可能导致悬空指针，确保安全后操作
        delete ret;
        delete call;
    }

    // 格式化phi指令并插入跳转块前面
    for (auto phi : paramPhis) {
        phi->FormatPhi();
        jump_dst->push_front(phi);
    }

    func->isRecursive(false);
    return true;
}
