#include "../../include/IR/Opt/TRE.hpp"
std::vector<std::pair<CallInst*, RetInst*>> TailRecElim::collectTailCalls(Function* func) {
    std::vector<std::pair<CallInst*, RetInst*>> tailCalls;

    auto userList = func->GetValUseList();
    for (auto* use : userList) {
        auto* inst = use->GetUser();
        if (!inst) continue;

        auto* call = inst->as<CallInst>();
        if (!call) continue;

        // 确认调用在当前函数中
        auto* parentFunc = call->GetParent() ? call->GetParent()->GetParent() : nullptr;
        if (parentFunc != func) continue;

        auto* nextInst = call->GetNextNode();
        if (!nextInst) continue;

        auto* ret = nextInst->as<RetInst>();
        if (!ret) continue;

        auto* retOp = ret->GetOperand(0);

        // 尾递归条件判断
        if (ret->GetValUseListSize() == 0 
            || (retOp && retOp->IsUndefVal())
            || (retOp == call)) {
            tailCalls.emplace_back(call, ret);
        }
    }

    return tailCalls;
}

std::pair<BasicBlock*, BasicBlock*> TailRecElim::liftAllocas() {
    auto* entryBlock = func->GetFront();   // 函数入口块
    Instruction* lastAlloca = nullptr;

    // 找到入口块中连续的 Alloca 指令的最后一个
    for (auto* inst : *entryBlock) {
        if (inst->as<AllocaInst>() != nullptr) {
            lastAlloca = inst;
        } else {
            break;
        }
    }

    // 如果没有 Alloca 指令，则新建基本块跳转到入口块
    if (!lastAlloca) {
        auto* allocBlock = new BasicBlock();
        allocBlock->push_back(new UnCondInst(entryBlock));
        func->push_front(allocBlock);
        return {allocBlock, entryBlock};
    }

    // 获取最后一个 Alloca 的下一条指令
    Instruction* splitInst = lastAlloca->GetNextNode();
    if (!splitInst) {
        // 如果函数只有 Alloca 指令，直接返回入口块，loopBlock 为 nullptr
        return {entryBlock, nullptr};
    }

    // 在 splitInst 处分割入口块，生成新的基本块
    auto* allocBlock = entryBlock->SplitAt(splitInst);

    // 在入口块末尾添加无条件跳转到新块
    entryBlock->push_back(new UnCondInst(allocBlock));

    // 将新块插入函数基本块链表中，紧跟入口块之后
    auto it = List<Function, BasicBlock>::iterator(entryBlock);
    it.InsertAfter(allocBlock);

    return {entryBlock, allocBlock};
}


bool TailRecElim::run() {
    auto tailCalls = collectTailCalls(func);
    if (tailCalls.empty())
        return false;

    auto [entryBlock, loopBlock] = liftAllocas();
    auto& params = func->GetParams();
    std::vector<PhiInst*> paramPhiNodes;

    // 为每个参数创建 Phi 节点，并替换原有参数使用
    for (auto& param : params) {
        auto* phi = new PhiInst(param->GetType());
        param->ReplaceAllUseWith(phi);
        phi->addIncoming(param.get(), entryBlock);
        paramPhiNodes.push_back(phi);
    }

    for (auto [call, ret] : tailCalls) {
        auto* srcBlock = call->GetParent();
        size_t argCount = call->GetUserUseList().size() - 1;

        assert(argCount == paramPhiNodes.size() && "Frontend guarantees this condition");

        // 更新 phi 节点的 incoming
        for (size_t i = 0; i < argCount; ++i) {
            paramPhiNodes[i]->addIncoming(call->GetOperand(i + 1), srcBlock);
        }

        // 用无条件跳转替换返回指令
        auto* br = new UnCondInst(loopBlock);
        ret->InstReplace(br);

        // 安全删除原返回和调用指令
        delete ret;
        delete call;
    }

    // 将 phi 节点插入 loopBlock 前端，并格式化
    for (auto* phi : paramPhiNodes) {
        phi->FormatPhi();
        loopBlock->push_front(phi);
    }

    func->isRecursive(false);
    return true;
}