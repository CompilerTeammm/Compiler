#include "../../include/IR/Opt/LoopRotate.hpp"

bool LoopRotate::run()
{
    bool changed = false;
    auto sideeffect = AM.get<SideEffect>(&Singleton<Module>());
    m_dom = AM.get<DominantTree>(m_func);
    loopAnlasis = AM.get<LoopAnalysis>(m_func, m_dom, std::ref(DeleteLoop));
    auto loops = loopAnlasis->GetLoops();
    for (auto loop : loops)
    {
        m_dom = AM.get<DominantTree>(m_func);
        loopAnlasis = AM.get<LoopAnalysis>(m_func, m_dom, std::ref(DeleteLoop));
        bool Success = false;
        Success |= TryRotate(loop);
        if (RotateLoop(loop, Success) || Success)
        {
            changed |= true;
            AM.AddAttr(loop->GetHeader(), Rotate);
        }
    }
    return changed;
}

/* bool LoopRotate::run()
{
    bool changed = false;
    while (true)
    {
        auto sideeffect = AM.get<SideEffect>(&Singleton<Module>());
        m_dom = AM.get<DominantTree>(m_func);
        loopAnlasis = AM.get<LoopAnalysis>(m_func, m_dom, std::ref(DeleteLoop));
        auto loops = loopAnlasis->GetLoops();
        bool localChanged = false;
        for (auto loop : loops)
        {
            m_dom = AM.get<DominantTree>(m_func);
            loopAnlasis = AM.get<LoopAnalysis>(m_func, m_dom, std::ref(DeleteLoop));
            bool Success = false;
            Success |= TryRotate(loop);
            if (RotateLoop(loop, Success) || Success)
            {
                localChanged = true;
                AM.AddAttr(loop->GetHeader(), Rotate);
                break; // 退出 for-loop，重新构建 loopAnlasis
            }
        }

        if (!localChanged)
            break;
        changed = true;
    }

    return changed;
} */

bool LoopRotate::RotateLoop(LoopInfo *loop, bool Succ)
{
    if (loop->RotateTimes > 8)
        return false;
    if (loop->GetLoopBody().size() == 1)
        return false;

    loop->RotateTimes++;

    // m_dom = AM.get<DominantTree>(m_func);
    // loopAnlasis = AM.get<LoopAnalysis>(m_func, m_dom, std::ref(DeleteLoop));

    auto prehead = loopAnlasis->GetPreHeader(loop);
    auto header = loop->GetHeader();
    auto latch = loopAnlasis->GetLatch(loop);

    assert(latch && prehead && header && "After Simplify Loop Must Be Canonical");

    if (!loopAnlasis->IsLoopExiting(loop, header))
        return false;
    if (loopAnlasis->IsLoopExiting(loop, latch) && !Succ)
        return false;

    auto cond = dynamic_cast<CondInst *>(header->GetBack());
    assert(cond && "Header Must have 2 succ: One is exit, another is body");

    auto New_header = dynamic_cast<BasicBlock *>(cond->GetOperand(1));
    auto Exit = dynamic_cast<BasicBlock *>(cond->GetOperand(2));

    std::unordered_map<Value *, Value *> PreHeaderValue;

    // phase1: change the edge
    bool revert = false;
    if (loop->Contain(Exit))
    {
        std::swap(New_header, Exit);
        revert = true;
    }

    auto It = prehead->rbegin();
    assert(dynamic_cast<UnCondInst *>(*It));

    for (auto iter = header->begin(); iter != header->end();)
    {
        auto inst = *iter;
        ++iter;

        if (auto phi = dynamic_cast<PhiInst *>(inst))
        {
            PreHeaderValue[phi] = phi->ReturnValIn(prehead);
            continue;
        }

        const std::set<BasicBlock *> contain{loop->GetLoopBody().begin(),
                                             loop->GetLoopBody().end()};

        if (LoopAnalysis::IsLoopInvariant(contain, inst, loop) && CanBeMove(inst))
        {
            inst->EraseFromManager();
            It.InsertBefore(inst);
            It = prehead->rbegin();
            continue;
        }
        else
        {
            auto new_inst = inst->CloneInst();
            Value *simplify = nullptr;

            for (int i = 0; i < new_inst->GetUserUseListSize(); i++)
            {
                auto &use = new_inst->GetUserUseList()[i];
                if (PreHeaderValue.count(use->GetValue()))
                {
                    auto tmp = PreHeaderValue[use->GetValue()];
                    new_inst->ReplaceSomeUseWith(i, tmp);
                }
            }

            if (dynamic_cast<BinaryInst *>(new_inst) &&
                new_inst->GetOperand(0)->isConst() &&
                new_inst->GetOperand(1)->isConst())
            {
                simplify = ConstantFold::ConstFoldBinaryOps(
                    dynamic_cast<BinaryInst *>(new_inst),
                    dynamic_cast<ConstantData *>(new_inst->GetOperand(0)),
                    dynamic_cast<ConstantData *>(new_inst->GetOperand(1)));
            }

            if (simplify)
            {
                delete new_inst;
                PreHeaderValue[inst] = simplify;
                CloneMap[inst] = simplify;
            }
            else
            {
                PreHeaderValue[inst] = new_inst;
                new_inst->SetManager(prehead);
                It.InsertBefore(new_inst);
                It = prehead->rbegin();
                CloneMap[inst] = new_inst;
            }
        }
    }
    delete *It;

    if (revert)
    {
        prehead->GetBack()->ReplaceSomeUseWith(1, Exit);
        prehead->GetBack()->ReplaceSomeUseWith(2, New_header);
    }
    else
    {
        prehead->GetBack()->ReplaceSomeUseWith(1, New_header);
        prehead->GetBack()->ReplaceSomeUseWith(2, Exit);
    }

    // 更新支配树边
    auto ExitNode = m_dom->getNode(Exit);
    auto PreheadNode = m_dom->getNode(prehead);
    auto HeaderNode = m_dom->getNode(header);
    auto NewHeaderNode = m_dom->getNode(New_header);

    PreheadNode->succNodes.push_front(ExitNode);
    ExitNode->predNodes.push_front(PreheadNode);

    PreheadNode->succNodes.remove(HeaderNode);
    HeaderNode->predNodes.remove(PreheadNode);

    PreheadNode->succNodes.push_front(NewHeaderNode);
    NewHeaderNode->predNodes.push_front(PreheadNode);

    // Deal With Phi in header
    PreservePhi(header, latch, loop, prehead, New_header, PreHeaderValue, loopAnlasis);

    if (dynamic_cast<CondInst *>(prehead->GetBack()) &&
        dynamic_cast<ConstIRBoolean *>(prehead->GetBack()->GetOperand(0)))
    {

        auto cond = dynamic_cast<CondInst *>(prehead->GetBack());
        auto Bool = dynamic_cast<ConstIRBoolean *>(cond->GetUserUseList()[0]->GetValue());
        BasicBlock *nxt = dynamic_cast<BasicBlock *>(cond->GetUserUseList()[1]->GetValue());
        BasicBlock *ignore = dynamic_cast<BasicBlock *>(cond->GetUserUseList()[2]->GetValue());

        if (Bool->GetVal() == false)
        {
            std::swap(nxt, ignore);
        }

        for (auto iter = ignore->begin();
             iter != ignore->end() && dynamic_cast<PhiInst *>(*iter); ++iter)
        {
            auto phi = dynamic_cast<PhiInst *>(*iter);
            for (int i = 0; i < phi->PhiRecord.size(); i++)
            {
                if (phi->PhiRecord[i].second == prehead)
                    phi->Del_Incomes(i);
            }
            phi->FormatPhi();
        }

        auto uncond = new UnCondInst(nxt);
        m_dom->getNode(ignore)->predNodes.remove(PreheadNode);
        PreheadNode->succNodes.remove(m_dom->getNode(ignore));
        prehead->rbegin().InsertBefore(uncond);
        delete cond;
    }

    loop->setHeader(New_header);
    AM.ChangeLoopHeader(header, New_header);

    SimplifyBlocks(header, loop);

    return true;
}

bool LoopRotate::CanBeMove(Instruction *I)
{
    if (auto call = dynamic_cast<CallInst *>(I))
    {
        if (call->HasSideEffect())
            return false;
        auto callee = dynamic_cast<Function *>(call->GetOperand(0));
        if (callee->MemRead() || callee->MemWrite())
            return false;
        return true;
    }
    else if (auto bin = dynamic_cast<BinaryInst *>(I))
    {
        return true;
    }
    else if (auto gep = dynamic_cast<GepInst *>(I))
    {
        return true;
    }
    return false;
}

void LoopRotate::PreservePhi(
    BasicBlock *header, BasicBlock *Latch, LoopInfo *loop,
    BasicBlock *preheader, BasicBlock *new_header,
    std::unordered_map<Value *, Value *> &PreHeaderValue,
    LoopAnalysis *loopAnlasis)
{

    std::map<PhiInst *, std::map<bool, Value *>> RecordPhi;
    std::map<Value *, PhiInst *> NewInsertPhi;
    std::map<PhiInst *, PhiInst *> PhiInsert;

    // 更新 header 的 successor Phi
    for (auto succNode : m_dom->getNode(header)->succNodes)
    {
        auto succ = succNode->curBlock;
        for (auto iter = succ->begin();
             iter != succ->end() && dynamic_cast<PhiInst *>(*iter); ++iter)
        {
            auto phi = dynamic_cast<PhiInst *>(*iter);
            if (PreHeaderValue.find(phi->ReturnValIn(header)) != PreHeaderValue.end())
                phi->addIncoming(PreHeaderValue[phi->ReturnValIn(header)], preheader);
            else
                phi->addIncoming(phi->ReturnValIn(header), preheader);
        }
    }

    // clear phi
    for (auto iter = header->begin();
         iter != header->end() && dynamic_cast<PhiInst *>(*iter); ++iter)
    {
        auto phi = dynamic_cast<PhiInst *>(*iter);
        for (int i = 0; i < phi->PhiRecord.size(); i++)
        {
            if (phi->PhiRecord[i].second == preheader)
            {
                RecordPhi[phi][true] = phi->PhiRecord[i].first;
                phi->Del_Incomes(i--);
                phi->FormatPhi();
                continue;
            }
            RecordPhi[phi][false] = phi->PhiRecord[i].first;
        }
    }

    auto DealPhi = [&](PhiInst *phi, Use *use)
    {
        auto user = use->GetUser();
        if (PhiInsert.find(phi) == PhiInsert.end())
        {
            assert(phi->PhiRecord.size() == 1);
            auto new_phi = PhiInst::NewPhiNode(new_header->GetFront(), new_header, phi->GetType());
            for (auto [flag, val] : RecordPhi[phi])
            {
                if (flag)
                    new_phi->addIncoming(RecordPhi[phi][flag], preheader);
                else
                    new_phi->addIncoming(RecordPhi[phi][flag], header);
            }
            user->ReplaceSomeUseWith(use, new_phi);
            PhiInsert[phi] = new_phi;
        }
        else
        {
            user->ReplaceSomeUseWith(use, PhiInsert[phi]);
        }

        if (auto p = dynamic_cast<PhiInst *>(use->GetUser()))
        {
            for (int i = 0; i < p->PhiRecord.size(); i++)
            {
                if (p->PhiRecord[i].first == phi)
                    p->PhiRecord[i].first = use->SetValue();
            }
        }

        for (auto ex : loopAnlasis->GetExit(loop))
            for (auto _inst : *ex)
                if (auto p = dynamic_cast<PhiInst *>(_inst))
                {
                    for (int i = 0; i < p->PhiRecord.size(); i++)
                        if (p->PhiRecord[i].first == phi && p->PhiRecord[i].second != header)
                        {
                            p->ReplaceSomeUseWith(i, PhiInsert[phi]);
                            p->PhiRecord[i].first = PhiInsert[phi];
                        }
                }
    };

    std::vector<std::pair<PhiInst *, Use *>> PhiSet;
    std::unordered_set<PhiInst *> assist;

    for (auto inst : *header)
    {
        if (auto phi = dynamic_cast<PhiInst *>(inst))
        {
            for (auto iter = inst->GetValUseList().begin(); iter != inst->GetValUseList().end();)
            {
                auto use = *iter;
                ++iter;

                auto userInst = dynamic_cast<Instruction *>(use->GetUser());
                if (!userInst)
                    continue; // 不是指令，跳过

                auto targetBB = userInst->GetParent();

                if (targetBB == header || !loop->Contain(targetBB) || targetBB == preheader)
                    continue;

                PhiSet.emplace_back(phi, use);
                assist.insert(phi);
            }
        }
        else
        {
            std::vector<Use *> Rewrite;
            for (auto iter = inst->GetValUseList().begin(); iter != inst->GetValUseList().end();)
            {
                auto use = *iter;
                ++iter;

                auto userInst = dynamic_cast<Instruction *>(use->GetUser());
                if (!userInst)
                    continue;
                auto targetBB = userInst->GetParent();
                if (targetBB == header || targetBB == preheader)
                    continue;

                Rewrite.push_back(use);
            }
            if (Rewrite.empty())
                continue;

            auto cloned = CloneMap[inst];
            if (!cloned)
                continue;
            auto ty = inst->GetType();
            if (auto ld = dynamic_cast<LoadInst *>(inst))
                ty = ld->GetOperand(0)->GetType();
            auto new_phi = PhiInst::NewPhiNode(new_header->GetFront(), new_header, inst->GetType());
            new_phi->addIncoming(cloned, preheader);
            new_phi->addIncoming(inst, Latch);
            for (auto use : Rewrite)
            {
                auto user = use->GetUser();
                if (auto phi = dynamic_cast<PhiInst *>(user))
                {
                    if (phi->ReturnBBIn(use) != header)
                        phi->ReplaceVal(use, new_phi);
                }
                else
                    user->ReplaceSomeUseWith(use, new_phi);
            }
        }
    }

    while (!PhiSet.empty())
    {
        auto [phi, use] = PhiSet.back();
        PhiSet.pop_back();
        DealPhi(phi, use);

        for (auto &u : phi->GetUserUseList())
        {
            auto insert_pos = new_header->begin();
            auto phi_use = dynamic_cast<PhiInst *>(u->GetValue());
            auto bb = phi->GetParent();
            while (phi_use && (phi_use->GetParent() == bb) && assist.insert(phi_use).second)
            {
                if (phi->GetUserUseList().size() == 1)
                {
                    phi->addIncoming(RecordPhi[phi][true], preheader);
                    phi->EraseFromManager();
                    insert_pos = insert_pos.InsertBefore(phi);
                }
                if (PhiInsert.find(phi_use) == PhiInsert.end())
                {
                    auto tmp = dynamic_cast<PhiInst *>(phi_use->GetOperand(0));
                    phi_use->addIncoming(RecordPhi[phi_use][true], preheader);
                    phi_use->EraseFromManager();
                    insert_pos = insert_pos.InsertBefore(phi_use);
                    phi_use = tmp;
                }
                else
                {
                    phi_use = PhiInsert[phi];
                }
            }
        }
    }
}

/* void LoopRotate::SimplifyBlocks(BasicBlock *Header, LoopInfo *loop) {
    BasicBlock *Latch = nullptr;

    // 获取前驱 latch
    for (auto predNode : m_dom->getNode(Header)->predNodes) {
        if (!Latch)
            Latch = predNode->curBlock;
        else {
            Latch = nullptr;
            break;
        }
    }
    assert(Latch && "Must Have One Latch!");
    if (dynamic_cast<CondInst *>(Latch->GetBack()))
        return;

    // 简化 Header 内 Phi
    for (auto iter = Header->begin();
         iter != Header->end() && dynamic_cast<PhiInst *>(*iter);) {
        auto phi = dynamic_cast<PhiInst *>(*iter);
        ++iter;
        if (phi->PhiRecord.size() == 1) {
            auto repl = (*(phi->PhiRecord.begin())).second.first;
            if (repl != phi) {
                phi->ReplaceAllUseWith(repl);
                delete phi;
            }
        }
    }

    // 更新 successor Phi
    for (auto succNode : m_dom->getNode(Header)->succNodes) {
        auto succ = succNode->curBlock;
        for (auto iter = succ->begin(); iter != succ->end();) {
            auto inst = *iter;
            ++iter;
            if (auto phi = dynamic_cast<PhiInst *>(inst)) {
                auto iter_header = std::find_if(
                    phi->PhiRecord.begin(), phi->PhiRecord.end(),
                    [Header](auto &ele) { return ele.second.second == Header; });
                auto iter_latch = std::find_if(
                    phi->PhiRecord.begin(), phi->PhiRecord.end(),
                    [Latch](auto &ele) { return ele.second.second == Latch; });

                if (iter_header == phi->PhiRecord.end())
                    continue;
                if (iter_latch != phi->PhiRecord.end() &&
                    iter_header != phi->PhiRecord.end()) {
                    phi->Del_Incomes(iter_header->first);
                    continue;
                }
                iter_header->second.second = Latch;
            } else {
                break;
            }
        }
    }

    Header->ReplaceAllUseWith(Latch);
    delete *(Latch->rbegin());

    // 将 Header 内指令移动到 Latch
    while (true) {
        auto iter = Header->begin();
        if (iter == Header->end())
            break;
        auto inst = *iter;
        if (dynamic_cast<PhiInst *>(inst)) {
            inst->EraseFromManager();
            auto it = Latch->begin();
            for (; it != Latch->end() && dynamic_cast<PhiInst *>(*it); ++it) {}
            it.InsertBefore(inst);
            continue;
        }
        inst->EraseFromManager();
        inst->SetManager(Latch);
        Latch->push_back(inst);
    }

    loopAnlasis->DeleteBlock(Header);

    auto it = std::find(m_func->GetBBs().begin(),
                        m_func->GetBBs().end(), Header);
    m_func->GetBBs().erase(it);
    delete Header;
    FunctionChange(m_func);
} */

// SimplifyBlocks 的安全版
void LoopRotate::SimplifyBlocks(BasicBlock *Header, LoopInfo *loop)
{
    BasicBlock *Latch = nullptr;

    // 获取前驱 latch（你当前的方式）
    for (auto predNode : m_dom->getNode(Header)->predNodes)
    {
        if (!Latch)
            Latch = predNode->curBlock;
        else
        {
            Latch = nullptr;
            break;
        }
    }
    assert(Latch && "Must Have One Latch!");
    if (dynamic_cast<CondInst *>(Latch->GetBack()))
        return;

    // 简化 Header 内 Phi
    for (auto iter = Header->begin();
         iter != Header->end() && dynamic_cast<PhiInst *>(*iter);)
    {
        auto phi = dynamic_cast<PhiInst *>(*iter);
        ++iter;
        if (phi->PhiRecord.size() == 1)
        {
            auto repl = (*(phi->PhiRecord.begin())).second.first;
            if (repl != phi)
            {
                phi->ReplaceAllUseWith(repl);
                // 假设 PhiInst 的内存由指令管理器管理，使用 EraseFromManager 而不是 delete
                phi->EraseFromManager();
                // 如果你的指令确实需要 delete，则要确认谁拥有此指令
                // delete phi; // <- 不要在这里 delete，除非你确定 ownership
            }
        }
    }

    // 更新 successor Phi（保持原逻辑，只改可能的删除）
    for (auto succNode : m_dom->getNode(Header)->succNodes)
    {
        auto succ = succNode->curBlock;
        for (auto iter = succ->begin(); iter != succ->end();)
        {
            auto inst = *iter;
            ++iter;
            if (auto phi = dynamic_cast<PhiInst *>(inst))
            {
                auto iter_header = std::find_if(
                    phi->PhiRecord.begin(), phi->PhiRecord.end(),
                    [Header](auto &ele)
                    { return ele.second.second == Header; });
                auto iter_latch = std::find_if(
                    phi->PhiRecord.begin(), phi->PhiRecord.end(),
                    [Latch](auto &ele)
                    { return ele.second.second == Latch; });

                if (iter_header == phi->PhiRecord.end())
                    continue;
                if (iter_latch != phi->PhiRecord.end() &&
                    iter_header != phi->PhiRecord.end())
                {
                    phi->Del_Incomes(iter_header->first);
                    continue;
                }
                iter_header->second.second = Latch;
            }
            else
            {
                break;
            }
        }
    }

    // 把 Header 的所有 use 替换为 Latch（注意：这会改写外部对 BasicBlock 的引用）
    Header->ReplaceAllUseWith(Latch);

    // 原来的: delete *(Latch->rbegin());
    // 改为：如果需要移除某条终结指令或空的最后一条指令，使用 EraseFromManager()（不要直接 delete）
    if (Latch->begin() == Latch->end())
    {
        Instruction *last = *(Latch->rbegin());
        // 如果该指令确实是终结指令并且由管理器负责释放，通过 EraseFromManager 删除它的管理关系
        last->EraseFromManager();
        // 不要 delete last; 除非你明确知道这块内存的唯一所有者是你
    }

    // 将 Header 内指令移动到 Latch（不改变指令所有权，只改变管理关系）
    while (true)
    {
        auto iter = Header->begin();
        if (iter == Header->end())
            break;
        auto inst = *iter;
        if (dynamic_cast<PhiInst *>(inst))
        {
            inst->EraseFromManager();
            auto it = Latch->begin();
            for (; it != Latch->end() && dynamic_cast<PhiInst *>(*it); ++it)
            {
            }
            it.InsertBefore(inst); // 插入到 Latch 中
            continue;
        }
        inst->EraseFromManager();
        inst->SetManager(Latch);
        Latch->push_back(inst);
    }
    loopAnlasis->DeleteBlock(Header);

    auto it = std::find_if(m_func->GetBBs().begin(), m_func->GetBBs().end(),
                           [Header](const Function::BBPtr &sp)
                           { return sp.get() == Header; });
    if (it != m_func->GetBBs().end())
    {
        m_func->GetBBs().erase(it); // shared_ptr 被移除：若这是最后一个引用，BasicBlock 会被 delete 自动析构
    }
    else
    {
        assert(false && "Header not found in m_func->GetBBs()");
    }
    Header->EraseFromManager();
    /*     auto it = std::find(m_func->GetBBs().begin(),
                            m_func->GetBBs().end(), Header);
        m_func->GetBBs().erase(it);

        auto node = Header;
        Header->EraseFromManager(); // 从 list 链表里摘掉
        delete node;                // 手动释放 */

    FunctionChange(m_func);
}

void LoopRotate::PreserveLcssa(BasicBlock *new_exit, BasicBlock *old_exit,
                               BasicBlock *pred)
{
    for (auto inst : *old_exit)
        if (auto phi = dynamic_cast<PhiInst *>(inst))
            for (auto &[_1, val] : phi->PhiRecord)
                if (val.second == pred)
                {
                    auto Insert =
                        PhiInst::NewPhiNode(new_exit->GetFront(), new_exit, phi->GetType());
                    Insert->SetName(Insert->GetName() + ".lcssa");
                    Insert->addIncoming(val.first, pred);
                    phi->ReplaceSomeUseWith(_1, Insert);
                    phi->ModifyBlock(val.second, new_exit);
                    phi->PhiRecord[_1] = std::make_pair(Insert, new_exit);
                }
}

/* bool LoopRotate::TryRotate(LoopInfo *loop) {
    bool Legal = false;
    auto latch = loopAnlasis->GetLatch(loop);
    auto head = loop->GetHeader();
    auto prehead = loopAnlasis->GetPreHeader(loop, LoopAnalysis::Loose);
    auto uncond = dynamic_cast<UnCondInst *>(latch->GetBack());
    if (!uncond)
        return false;
    assert(latch);

    auto latchNode = m_dom->getNode(latch);

    int pred = latchNode->predNodes.size();
    if (pred != 1)
        return false;

    BasicBlock *PredBB = latchNode->predNodes.front()->curBlock;

    for (auto succNode : m_dom->getNode(PredBB)->succNodes) {
        auto succ = succNode->curBlock;
        if (!loop->Contain(succ))
            Legal = true;
    }
    if (!Legal)
        return false;

    int times = 0;
    auto exiting = loopAnlasis->GetExitingBlock(loop);

    for (auto iter = latch->begin(); iter != latch->end();) {
        auto inst = *iter;
        ++iter;

        if (dynamic_cast<UnCondInst *>(inst) || dynamic_cast<CondInst *>(inst))
            break;

        if (auto bin = dynamic_cast<BinaryInst *>(inst)) {
            if (times > 0)
                return false;
            times++;

        if (exiting.size() > 1) {
            auto lhs = inst->GetOperand(0);
            for (auto use : lhs->GetValUseList()) {
                auto userInst = dynamic_cast<Instruction *>(use->GetUser());
                if (!userInst)
                    continue;

                if (!loop->Contain(userInst->GetParent())) {
                    Legal = false;
                    break;
                }
            }
        }

        for (auto use : inst->GetValUseList()) {
            auto userInst = dynamic_cast<Instruction *>(use->GetUser());
            if (!userInst)
                continue;

            if (!loop->Contain(userInst->GetParent())) {
                Legal = false;
                break;
            }
        }

            bool HasZero = false;
            auto rhs = inst->GetOperand(1);
            if (auto phi = dynamic_cast<PhiInst *>(rhs))
                for (auto &[idnex, val] : phi->PhiRecord) {
                    if (auto cond = dynamic_cast<ConstIRInt *>(val.first))
                        if (cond->GetVal() == 0)
                            HasZero = true;
                }
            if (auto con = dynamic_cast<ConstIRInt *>(rhs))
                if (con->GetVal() == 0)
                    HasZero = true;

            if ((bin->GetOp() == BinaryInst::Op_Mod ||
                 bin->GetOp() == BinaryInst::Op_Mod) &&
                HasZero)
                return false;
        } else {
            Legal = false;
            break;
        }
    }

    if (!Legal)
        return false;

    for (auto iter = latch->begin(); iter != latch->end();) {
        auto inst = *iter;
        ++iter;
        if (dynamic_cast<CondInst *>(inst) || dynamic_cast<UnCondInst *>(inst))
            break;
        auto it = PredBB->rbegin();
        inst->EraseFromManager();
        it.InsertBefore(inst);
    }

    auto cond = dynamic_cast<CondInst *>(PredBB->GetBack());
    assert(cond);

    for (int i = 0; i < cond->GetUserUseList().size(); i++) {
        if (cond->GetOperand(i) == latch) {
            cond->ReplaceSomeUseWith(i, head);
            loop->DeleteBlock(latch);

            auto PredNode = m_dom->getNode(PredBB);
            auto headNode = m_dom->getNode(head);

            // 更新支配树关系
            PredNode->succNodes.remove(latchNode);
            headNode->predNodes.remove(latchNode);
            headNode->predNodes.push_front(PredNode);
            PredNode->succNodes.push_front(headNode);

            for (auto inst : *head)
                if (auto phi = dynamic_cast<PhiInst *>(inst))
                    phi->ModifyBlock(latch, PredBB);

            break;
        }
    }

    auto iterBB = std::find(m_func->GetBBs().begin(),
                            m_func->GetBBs().end(), latch);
    m_func->GetBBs().erase(iterBB);
    delete latch;

    FunctionChange(m_func);
    return true;
} */
// TryRotate 的安全版
bool LoopRotate::TryRotate(LoopInfo *loop)
{
    bool Legal = false;
    auto latch = loopAnlasis->GetLatch(loop);
    auto head = loop->GetHeader();
    auto prehead = loopAnlasis->GetPreHeader(loop, LoopAnalysis::Loose);
    auto uncond = dynamic_cast<UnCondInst *>(latch->GetBack());
    if (!uncond)
        return false;
    assert(latch);

    auto latchNode = m_dom->getNode(latch);

    int pred = latchNode->predNodes.size();
    if (pred != 1)
        return false;

    BasicBlock *PredBB = latchNode->predNodes.front()->curBlock;

    for (auto succNode : m_dom->getNode(PredBB)->succNodes)
    {
        auto succ = succNode->curBlock;
        if (!loop->Contain(succ))
            Legal = true;
    }
    if (!Legal)
        return false;

    int times = 0;
    auto exiting = loopAnlasis->GetExitingBlock(loop);

    for (auto iter = latch->begin(); iter != latch->end();)
    {
        auto inst = *iter;
        ++iter;

        if (dynamic_cast<UnCondInst *>(inst) || dynamic_cast<CondInst *>(inst))
            break;

        if (auto bin = dynamic_cast<BinaryInst *>(inst))
        {
            if (times > 0)
                return false;
            times++;

            if (exiting.size() > 1)
            {
                auto lhs = inst->GetOperand(0);
                for (auto use : lhs->GetValUseList())
                {
                    auto userInst = dynamic_cast<Instruction *>(use->GetUser());
                    if (!userInst)
                        continue;

                    if (!loop->Contain(userInst->GetParent()))
                    {
                        Legal = false;
                        break;
                    }
                }
            }

            for (auto use : inst->GetValUseList())
            {
                auto userInst = dynamic_cast<Instruction *>(use->GetUser());
                if (!userInst)
                    continue;

                if (!loop->Contain(userInst->GetParent()))
                {
                    Legal = false;
                    break;
                }
            }

            bool HasZero = false;
            auto rhs = inst->GetOperand(1);
            if (auto phi = dynamic_cast<PhiInst *>(rhs))
                for (auto &[idnex, val] : phi->PhiRecord)
                {
                    if (auto cond = dynamic_cast<ConstIRInt *>(val.first))
                        if (cond->GetVal() == 0)
                            HasZero = true;
                }
            if (auto con = dynamic_cast<ConstIRInt *>(rhs))
                if (con->GetVal() == 0)
                    HasZero = true;

            if ((bin->GetOp() == BinaryInst::Op_Mod ||
                 bin->GetOp() == BinaryInst::Op_Mod) &&
                HasZero)
                return false;
        }
        else
        {
            Legal = false;
            break;
        }
    }

    if (!Legal)
        return false;

    // 移动 latch 内（非分支）指令到 PredBB（使用 EraseFromManager + InsertBefore）
    for (auto iter = latch->begin(); iter != latch->end();)
    {
        auto inst = *iter;
        ++iter;
        if (dynamic_cast<CondInst *>(inst) || dynamic_cast<UnCondInst *>(inst))
            break;
        auto it = PredBB->rbegin();
        inst->EraseFromManager();
        it.InsertBefore(inst);
    }

    auto cond = dynamic_cast<CondInst *>(PredBB->GetBack());
    assert(cond);

    for (int i = 0; i < (int)cond->GetUserUseList().size(); i++)
    {
        if (cond->GetOperand(i) == latch)
        {
            cond->ReplaceSomeUseWith(i, head);
            loop->DeleteBlock(latch);

            auto PredNode = m_dom->getNode(PredBB);
            auto headNode = m_dom->getNode(head);

            // 更新支配树关系（按你原来的逻辑）
            PredNode->succNodes.remove(latchNode);
            headNode->predNodes.remove(latchNode);
            headNode->predNodes.push_front(PredNode);
            PredNode->succNodes.push_front(headNode);

            for (auto inst : *head)
                if (auto phi = dynamic_cast<PhiInst *>(inst))
                    phi->ModifyBlock(latch, PredBB);

            break;
        }
    }

    // 从函数 BB 列表中移除 latch（erase shared_ptr，从而让 shared_ptr 控制释放）
    auto iterBB = std::find_if(m_func->GetBBs().begin(), m_func->GetBBs().end(),
                               [latch](const Function::BBPtr &sp)
                               { return sp.get() == latch; });
    if (iterBB != m_func->GetBBs().end())
    {
        m_func->GetBBs().erase(iterBB);
    }
    else
    {
        assert(false && "latch not found in GetBBs()");
    }
    latch->EraseFromManager();

    /*     auto iter = std::find(m_func->GetBBs().begin(),
                              m_func->GetBBs().end(), latch);
        m_func->GetBBs().erase(iter);

        auto node = latch;
        latch->EraseFromManager(); // 从 list 链表里摘掉
        delete node;               // 手动释放 */

    FunctionChange(m_func);
    return true;
}
