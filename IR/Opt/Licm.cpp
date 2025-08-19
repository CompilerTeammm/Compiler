#include "../../include/IR/Opt/Licm.hpp"

bool LICMPass::run()
{
  m_dom = AM.get<DominantTree>(m_func);
  loop = AM.get<LoopAnalysis>(m_func, m_dom, std::ref(DeleteLoop));
  alias = AM.get<AliasAnalysis>(m_func);
  auto side = AM.get<SideEffect>(&Singleton<Module>());
  bool changed = false;
  for (auto l = loop->begin(); l != loop->end(); l++)
  {
    changed |= RunOnLoop(*l);
  }
  return changed;
}

bool LICMPass::RunOnLoop(LoopInfo *l)
{
  std::set<BasicBlock *> contain{l->GetLoopBody().begin(),
                                 l->GetLoopBody().end()};
  contain.insert(l->GetHeader());
  auto head = l->GetHeader();
  change |= licmSink(contain, l, head);
  change |= licmHoist(contain, l, head);
  return change;
}
// user在外部
bool LICMPass::licmSink(const std::set<BasicBlock *> &contain, LoopInfo *l,
                        BasicBlock *bb)
{
  bool changed = false;
  if (contain.find(bb) == contain.end())
    return changed;
  // 找到支配最低节点
  auto node = m_dom->getNode(bb); // 返回 TreeNode*
  for (auto childNode : node->idomChild)
  {
    auto childBB = childNode->curBlock; // TreeNode* -> BasicBlock*
    change |= licmSink(contain, l, childBB);
  }

  // 只处理当前循环bb
  if (loop->LookUp(bb) != l)
    return changed;
  for (auto iter = bb->rbegin(); iter != bb->rend(); --iter)
  {
    auto inst = *iter;
    if (UserOutSideLoop(contain, inst, l) && CanBeMove(l, inst))
    {
      std::map<BasicBlock *, Instruction *> InsertNew; // 记录每个exit对应的
      for (auto user : inst->GetValUseList())
      {
        auto phiInst = dynamic_cast<PhiInst *>(user->GetUser());
        assert(phiInst && "after lcssa user must be PhiInst");

        auto PBB = phiInst->GetParent();
        Instruction *CloneInst = nullptr;
        if (InsertNew.find(PBB) == InsertNew.end())
        {
          CloneInst = inst->CloneInst();
          CloneInst->SetName(inst->GetName() + ".licm");
          for (auto it = PBB->begin(); it != PBB->end(); ++it)
          {
            if (!dynamic_cast<PhiInst *>(*it))
            {
              it.InsertBefore(CloneInst);
              break;
            }
          }
          InsertNew[PBB] = CloneInst;
        }
        else
        {
          CloneInst = InsertNew[PBB];
        }
        phiInst->ReplaceAllUseWith(CloneInst);
        delete phiInst;
        changed = true;
      }
      assert(inst->GetValUseListSize() == 0);
      delete inst;
    }
  }
  return changed;
}
// use都是不变量
bool LICMPass::licmHoist(const std::set<BasicBlock *> &contain, LoopInfo *l,
                         BasicBlock *bb)
{
  bool changed = false;
  if (contain.find(bb) == contain.end())
    return changed;
  // 只处理当前循环bb
  if (loop->LookUp(bb) != l)
    return changed;
  for (auto iter = bb->begin(); iter != bb->end(); ++iter)
  {
    auto inst = *iter;
    if (LoopAnalysis::IsLoopInvariant(contain, inst, l) && CanBeMove(l, inst))
    {
      auto exit = loop->GetExit(l);
      if (!IsDomExit(inst, exit) && !UserInsideLoop(inst, l))
        continue;
      auto prehead = loop->GetPreHeader(l, LoopAnalysis::Loose);
      assert(prehead);
      auto New_inst = inst->CloneInst();
      auto it = prehead->rbegin();
      it.InsertBefore(New_inst);
      inst->ReplaceAllUseWith(New_inst);
      delete inst;
      changed = true;
    }
  }
  auto node = m_dom->getNode(bb);        // 返回 TreeNode*
  for (auto childNode : node->idomChild) // idomChild 是 std::list<TreeNode*>
  {
    auto childBB = childNode->curBlock; // TreeNode* -> BasicBlock*
    change |= licmHoist(contain, l, childBB);
  }

  return changed;
}

bool LICMPass::UserOutSideLoop(const std::set<BasicBlock *> &contain, User *I,
                               LoopInfo *curloop)
{
  for (auto use : I->GetValUseList())
  {
    auto userInst = dynamic_cast<Instruction *>(use->GetUser());
    if (!userInst)
      continue; // 不是指令，跳过

    auto userbb = userInst->GetParent();
    if (auto phi = dynamic_cast<PhiInst *>(use->GetUser()))
    {
      // check lcssa form
      bool IsLcssaPhi =
          std::all_of(phi->PhiRecord.begin(), phi->PhiRecord.end(),
                      [I](const auto &val)
                      { return I == val.second.first; });
      if (IsLcssaPhi)
        continue;
      // true phi form
      bool IsPhiMoveable =
          std::all_of(phi->PhiRecord.begin(), phi->PhiRecord.end(),
                      [I, &contain](const auto &val)
                      {
                        if (I == val.second.first)
                          if (contain.find(val.second.second) != contain.end())
                            return false;
                        return true;
                      });
      if (!IsPhiMoveable)
        return false;
    }
    if (contain.find(userbb) != contain.end())
      return false;
  }
  return true;
}

bool LICMPass::IsDomExit(User *I, std::vector<BasicBlock *> &exit)
{
  assert(!exit.empty());

  auto inst = dynamic_cast<Instruction *>(I);
  if (!inst)
    return false; // 非指令无法获取 BasicBlock，直接返回 false

  auto targetbb = inst->GetParent();
  for (auto ex : exit)
  {
    if (!m_dom->dominates_(targetbb, ex))
      return false;
  }
  return true;
}

bool LICMPass::CanBeMove(LoopInfo *curloop, User *I)
{
  if (auto ld = dynamic_cast<LoadInst *>(I))
  {
    bool HaveSideEffect = false;
    for (auto use : ld->GetValUseList())
    {
      auto userInst = dynamic_cast<Instruction *>(use->GetUser());
      if (!userInst)
        continue; // 不是指令，跳过

      if (!curloop->Contain(userInst->GetParent()))
        continue;

      HaveSideEffect |= (!CanBeMove(curloop, userInst));
      if (HaveSideEffect)
        return false;
    }

    return HaveSideEffect;
  }
  else if (dynamic_cast<StoreInst *>(I))
  {
    return false;
  }
  else if (dynamic_cast<PhiInst *>(I))
  {
    return false;
  }
  else if (auto call = dynamic_cast<CallInst *>(I))
  {
    auto target_func = dynamic_cast<Function *>(call->GetOperand(0));
    if (call->HasSideEffect())
      return false;
    if (target_func->MemRead())
      return false;
    if (target_func->MemWrite())
      return false;
  }
  else if (auto bin = dynamic_cast<BinaryInst *>(I))
  {
    if (bin->IsAtomic())
      return false;
    return true;
  }
  else if (auto gep = dynamic_cast<GepInst *>(I))
  {
    auto hash = AliasAnalysis::GetHash(gep);
    auto ValueSet = alias->FindHashVal(hash);
    bool HaveSideEffect = false;
    for (auto g : ValueSet)
    {
      for (auto use : g->GetValUseList())
      {
        auto userInst = dynamic_cast<Instruction *>(use->GetUser());
        if (!userInst)
          continue; // 不是指令，跳过

        if (!curloop->Contain(userInst->GetParent()))
          continue;

        HaveSideEffect |= !CanBeMove(curloop, userInst);
        if (HaveSideEffect)
          return false;
      }
    }
    return HaveSideEffect;
  }
  else
  {
    return false;
  }
  return true;
}

bool LICMPass::UserInsideLoop(User *I, LoopInfo *curloop)
{
  for (auto use : I->GetValUseList())
  {
    auto userInst = dynamic_cast<Instruction *>(use->GetUser());
    if (!userInst)
      continue; // 不是指令，跳过

    if (!curloop->Contain(userInst->GetParent()))
      return false;
  }

  return true;
}