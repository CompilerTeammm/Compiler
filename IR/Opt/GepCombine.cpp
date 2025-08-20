#include "../../include/IR/Opt/GepCombine.hpp"

bool GepCombine::ProcessNode(NodeContext* nodeCtx)
{
    bool modified = false;
    BasicBlock* block = nodeCtx->Block();
    std::unordered_set<GepInst*>& geps = nodeCtx->Geps();

    for (Instruction* inst : *block)
    {
        if (auto gep = dynamic_cast<GepInst*>(inst))
        {
            geps.insert(gep);

            // 尝试简化 GEP
            if (auto simplified = SimplifyGep(gep, geps))
            {
                modified = true;
                continue;  // 已处理，直接跳到下一个
            }

            // 处理 GEP PHI
            if (auto newGep = ProcessGepPhi(gep, geps))
            {
                gep = newGep;
                modified = true;
            }

            // 处理普通 GEP
            if (auto newGep = ProcessNormalGep(gep, geps))
            {
                modified = true;
            }
        }
    }

    nodeCtx->SetGeps(geps);
    return modified;
}

Value* GepCombine::SimplifyGep(GepInst* inst, std::unordered_set<GepInst*>& geps)
{
    auto& uses = inst->GetUserUseList();
    if (uses.empty())
        return nullptr;

    Value* baseVal = uses[0]->usee;

    // case1: 单一用户，直接替换为 base
    if (uses.size() == 1)
    {
        inst->ReplaceAllUseWith(baseVal);
        geps.erase(inst);
        return baseVal;
    }

    // case2: base 是 UndefValue
    if (auto* undef = dynamic_cast<UndefValue*>(baseVal))
    {
        Value* replacement = UndefValue::Get(inst->GetType());
        inst->ReplaceAllUseWith(replacement);
        geps.erase(inst);
        return replacement;
    }

    // case3: base 是另一个 GEP，且末尾 offset == 0，可合并
    if (auto* baseGep = dynamic_cast<GepInst*>(baseVal))
    {
        if (auto* offset = dynamic_cast<ConstIRInt*>(baseGep->GetUserUseList().back()->usee))
        {
            if (offset->GetVal() == 0)
            {
                std::vector<Operand> mergedOps;
                // 拷贝 baseGep 的索引（去掉最后的0）
                for (size_t i = 1; i + 1 < baseGep->GetUserUseList().size(); ++i)
                    mergedOps.push_back(baseGep->GetUserUseList()[i]->GetValue());

                // 拼接 inst 的索引
                for (size_t i = 1; i < uses.size(); ++i)
                    mergedOps.push_back(uses[i]->usee);

                auto* newGep = new GepInst(baseGep->GetUserUseList()[0]->usee, mergedOps);

                BasicBlock::List<BasicBlock, Instruction>::iterator insertPos(inst);
                insertPos.InsertBefore(newGep);

                inst->ReplaceAllUseWith(newGep);
                geps.erase(inst);
                geps.insert(newGep);
                pendingDelete.push_back(inst);

                return newGep;
            }
        }
    }

    // case4: 两个操作数，检查是否是冗余偏移
    if (uses.size() == 2)
    {
        Value* idx = uses[1]->usee;

        // 偏移为0
        if (auto* cst = dynamic_cast<ConstantData*>(idx))
        {
            if (cst->isConstZero())
            {
                inst->ReplaceAllUseWith(baseVal);
                geps.erase(inst);
                return baseVal;
            }
        }

        // base 类型大小为0
        if (auto* tp = dynamic_cast<HasSubType*>(baseVal->GetType()))
        {
            if (tp->GetSize() == 0)
            {
                inst->ReplaceAllUseWith(baseVal);
                geps.erase(inst);
                return baseVal;
            }
        }
    }

    return nullptr;
}


GepInst* GepCombine::ProcessGepPhi(GepInst* inst, std::unordered_set<GepInst*>& geps)
{
    auto* baseVal = inst->GetUserUseList()[0]->usee;
    auto* phi = dynamic_cast<PhiInst*>(baseVal);
    if (!phi) return nullptr;

    auto* refGep = dynamic_cast<GepInst*>(phi->GetVal(0));
    if (!refGep || refGep == inst)
        return nullptr;

    int diffPos = -1;

    // 遍历 phi 的所有输入值
    for (size_t i = 1; i < phi->GetUserUseList().size(); ++i)
    {
        auto* otherGep = dynamic_cast<GepInst*>(phi->GetVal(i));
        if (!otherGep || otherGep->GetUserUseList().size() != refGep->GetUserUseList().size())
            return nullptr;
        if (otherGep == inst)
            return nullptr;

        // 对比 refGep 和 otherGep 的各个 operand
        for (size_t j = 0; j < refGep->GetUserUseList().size(); ++j)
        {
            auto* opA = refGep->GetOperand(j);
            auto* opB = otherGep->GetOperand(j);

            if (opA->GetType() != opB->GetType())
                return nullptr;

            // 常量比较
            auto* cA = dynamic_cast<ConstantData*>(opA);
            auto* cB = dynamic_cast<ConstantData*>(opB);
            if (cA && cB)
            {
                // int/float 常量值不一致直接返回
                auto* iA = dynamic_cast<ConstIRInt*>(cA);
                auto* iB = dynamic_cast<ConstIRInt*>(cB);
                auto* fA = dynamic_cast<ConstIRFloat*>(cA);
                auto* fB = dynamic_cast<ConstIRFloat*>(cB);

                if (iA && iB && iA->GetVal() != iB->GetVal()) return nullptr;
                if (iA && fB && iA->GetVal() != fB->GetVal()) return nullptr;
                if (fA && iB && fA->GetVal() != iB->GetVal()) return nullptr;
                if (fA && fB && fA->GetVal() != fB->GetVal()) return nullptr;
            }
            else if (opA != opB) // 非常量但不同 → 只能有一个位置不同
            {
                if (diffPos == -1) diffPos = j;
                else return nullptr;
            }
        }
    }

    // 只有一个 use 才能优化
    if (phi->GetValUseList().GetSize() != 1)
        return nullptr;

    // 构造新的 gep
    std::vector<Operand> ops;
    for (size_t i = 1; i < refGep->GetUserUseList().size(); ++i)
        ops.push_back(refGep->GetOperand(i));

    auto* newGep = new GepInst(refGep->GetUserUseList()[0]->usee, ops);

    // 插入新指令
    BasicBlock::List<BasicBlock, Instruction>::iterator pos(inst);
    pos.InsertBefore(newGep);

    if (diffPos != -1)
    {
        // 构造新的 phi
        auto* newPhi = new PhiInst(phi, phi->GetType());
        for (auto& [block, val] : phi->PhiRecord)
            newPhi->addIncoming(val.first, val.second);

        newGep->ReplaceSomeUseWith(diffPos, newPhi);
    }

    inst->ReplaceSomeUseWith(0, newGep);

    // 更新 geps 集合和删除列表
    geps.insert(newGep);
    geps.erase(inst);
    pendingDelete.push_back(inst);

    return inst;
}

GepInst* GepCombine::ProcessNormalGep(GepInst* inst, std::unordered_set<GepInst*>& geps)
{
    auto* base = dynamic_cast<GepInst*>(inst->GetUserUseList()[0]->usee);

    // 如果 base 也是 GEP，并且 base->base 也是 GEP，就不处理
    if (base)
    {
        if (!CanCombineGep(inst, base))
            return nullptr;

        auto* baseBase = dynamic_cast<GepInst*>(base->GetUserUseList()[0]->usee);
        if (baseBase && baseBase->GetUserUseList().size() == 2 && CanCombineGep(base, baseBase))
            return nullptr;
    }

    // 只处理 [base, zero, idx] 这种情况
    if (inst->GetUserUseList().size() == 3)
    {
        auto* zero = dynamic_cast<ConstantData*>(inst->GetUserUseList()[1]->usee);
        if (!zero || !zero->isConstZero())
            return nullptr;

        auto* idxVal = inst->GetUserUseList()[2]->usee;
        auto* binOp = dynamic_cast<BinaryInst*>(idxVal);
        if (!binOp || binOp->GetOp() != BinaryInst::Operation::Op_Add)
            return nullptr;

        // 尝试匹配 (gep, const) 或 (const, gep)
        auto tryCombine = [&](Value* a, Value* b) -> bool {
            auto* gep = dynamic_cast<GepInst*>(a);
            auto* constVal = dynamic_cast<ConstantData*>(b);
            if (!gep || !constVal) return false;

            if (gep->GetUserUseList()[0]->usee == inst->GetUserUseList()[0]->usee && geps.count(gep))
            {
                inst->ReplaceSomeUseWith(inst->GetUserUseList()[0].get(), gep);
                inst->ReplaceSomeUseWith(inst->GetUserUseList()[1].get(), constVal);

                geps.erase(inst);
                pendingDelete.push_back(inst);
                return true;
            }
            return false;
        };

        if (tryCombine(binOp->GetUserUseList()[0]->usee, binOp->GetUserUseList()[1]->usee) ||
            tryCombine(binOp->GetUserUseList()[1]->usee, binOp->GetUserUseList()[0]->usee))
        {
            return inst;
        }
    }

    return nullptr;
}


bool GepCombine::CanCombineGep(GepInst* src, GepInst* base)
{
    auto allOffsetZero = [](GepInst* gep) {
        return std::all_of(
            gep->GetUserUseList().begin() + 1, 
            gep->GetUserUseList().end(),
            [](User::UsePtr& use) {
                if (auto* val = dynamic_cast<ConstantData*>(use->usee))
                    return val->isConstZero();
                return false;
            });
    };

    // 如果 src 的所有偏移是 0 且 base 不是全 0 且 base 被多处使用，就不能合并
    if (allOffsetZero(src) && !allOffsetZero(base) && base->GetValUseList().GetSize() != 1)
        return false;

    return true;
}


bool GepCombine::run()
{
    domTree = analysisMgr.get<DominantTree>(func);
    auto* SE = analysisMgr.get<SideEffect>(&Singleton<Module>());

    bool modified = false;
    std::deque<NodeContext*> workList;

    BasicBlock* entryBB = func->GetFront();
    DominantTree::TreeNode* entryNode = domTree->getNode(entryBB);

    workList.push_back(new NodeContext(domTree,
                                       entryNode,
                                       entryNode->idomChild.begin(),
                                       entryNode->idomChild.end(),
                                       std::unordered_set<GepInst*>{}));

    while (!workList.empty())
    {
        NodeContext* current = workList.back();

        if (!current->IsProcessed())
        {
            modified |= ProcessNode(current);
            current->MarkProcessed();
            current->SetChildGeps(current->Geps());
        }
        else if (current->ChildIterator() != current->EndIterator())
        {
            DominantTree::TreeNode* childNode = *(current->NextChild());
            workList.push_back(new NodeContext(domTree,
                                               childNode,
                                               childNode->idomChild.begin(),
                                               childNode->idomChild.end(),
                                               current->ChildGeps()));
        }
        else
        {
            delete current;
            workList.pop_back();
        }
    }

    // 删除待清理指令
    for (auto inst : pendingDelete)
        delete inst;
    pendingDelete.clear();

    return modified;
}
