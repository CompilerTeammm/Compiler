#include "../../include/IR/Opt/LoopRotaing.hpp"
#include "../../include/IR/Analysis/SideEffect.hpp"
#include "../../include/IR/Opt/ConstantFold.hpp"
#include "../../include/lib/Singleton.hpp"

#include <cassert>
#include <cstdlib>
#include <iterator>
#include <unordered_set>
#include <vector>

bool LoopRotaing::Run()
{
  bool changed = false;
  // if (m_func->tag != Function::Normal)
  //   return false;
  auto sideeffect = AM.get<SideEffect>(&Singleton<Module>());

  DominantTree m_dom(m_func);
  m_dom.BuildDominantTree();
  this->loopAnlasis = new LoopInfoAnalysis(m_func, &m_dom, DeleteLoop);
  auto loops = loopAnlasis->GetLoops();
  for (auto loop : loops)
  {
    DominantTree m_dom(m_func);
    m_dom.BuildDominantTree();
    // this->loopAnlasis = new LoopInfoAnalysis(m_func, &m_dom, DeleteLoop);
    bool Success = false;
    Success |= TryRotate(loop, &m_dom);
    if (RotateLoop(loop, Success) || Success)
    {
      changed |= true;
      AM.AddAttr(loop->getHeader(), Rotate);
    }
  }
  return changed;
}

bool LoopRotaing::RotateLoop(Loop *loop, bool Succ)
{
  if (loop->RotateTimes > 8)
    return false;
  if (loop->getLoopBody().size() == 1)
    return false;
  loop->RotateTimes++;
  DominantTree m_dom(m_func);
  m_dom.BuildDominantTree();
  // loopAnlasis = new LoopInfoAnalysis(m_func, &m_dom, DeleteLoop);
  auto prehead = loopAnlasis->getPreHeader(loop);
  auto header = loop->getHeader();
  auto latch = loopAnlasis->getLatch(loop);
  assert(latch && prehead && header && "After Simplify Loop Must Be Conon");
  if (!loopAnlasis->isLoopExiting(loop, header))
  {
    return false;
  }
  if (loopAnlasis->isLoopExiting(loop, latch) && !Succ)
    return false;
  auto cond = dynamic_cast<CondInst *>(header->GetBack());
  assert(cond && "Header Must have 2 succ: One is exit ,another is body");
  auto New_header = dynamic_cast<BasicBlock *>(cond->GetOperand(1));
  auto Exit = dynamic_cast<BasicBlock *>(cond->GetOperand(2));
  std::unordered_map<Value *, Value *> PreHeaderValue;
  // phase1: change th edge
  bool revert = false;
  if (loop->ContainBB(Exit))
  {
    std::swap(New_header, Exit);
    revert = true;
  }
  auto It = prehead->rbegin();
  assert(dynamic_cast<UnCondInst *>(*It));
  for (auto iter = header->begin(); iter != header->end();)
  {
    // condition��ȡ��preheader��˳����һЩ��������ȡ
    auto inst = *iter;
    ++iter;
    if (auto phi = dynamic_cast<PhiInst *>(inst))
    {
      PreHeaderValue[phi] = phi->ReturnValIn(prehead);
      continue;
    }
    const std::set<BasicBlock *> contain{loop->getLoopBody().begin(), loop->getLoopBody().end()};
    if (LoopInfoAnalysis::IsLoopInvariant(contain, inst, loop) && CanBeMove(inst))
    {
      inst->EraseFromManager();
      It.InsertBefore(inst);
      It = prehead->rbegin();
      continue;
    }
    else
    {
      auto new_inst = inst->CloneInst();
      // ���ܴ�phi��ȡ��ѭ�����Ϊ�˿ɼ򻯵�ָ��?
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
      if (dynamic_cast<BinaryInst *>(new_inst) && new_inst->GetOperand(0)->isConst() &&
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
        auto new_inst1 = dynamic_cast<Instruction *>(new_inst);
        new_inst1->SetManager(prehead);
        It.InsertBefore(new_inst1);
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

  DominantTree::TreeNode *exitNode = m_dom.getNode(Exit);
  DominantTree::TreeNode *preheadNode = m_dom.getNode(prehead);
  DominantTree::TreeNode *headerNode = m_dom.getNode(header);
  DominantTree::TreeNode *New_headerNode = m_dom.getNode(New_header);

  m_dom.getNode(Exit)->predNodes.push_front(preheadNode);
  m_dom.getNode(prehead)->succNodes.push_front(exitNode);
  m_dom.getNode(prehead)->succNodes.remove(headerNode);
  m_dom.getNode(header)->predNodes.remove(preheadNode);
  m_dom.getNode(New_header)->predNodes.push_front(preheadNode);
  m_dom.getNode(prehead)->succNodes.push_front(New_headerNode);
  // Deal With Phi In header

  PreservePhi(header, latch, loop, prehead, New_header, PreHeaderValue, loopAnlasis, &m_dom);
  if (dynamic_cast<CondInst *>(prehead->GetBack()) &&
      dynamic_cast<ConstIRBoolean *>(prehead->GetBack()->GetOperand(0)))
  {
    auto cond = dynamic_cast<CondInst *>(prehead->GetBack());
    auto Bool = dynamic_cast<ConstIRBoolean *>(cond->GetUserUseList()[0]->GetValue());
    BasicBlock *nxt = dynamic_cast<BasicBlock *>(cond->GetUserUseList()[1]->GetValue());
    BasicBlock *ignore = dynamic_cast<BasicBlock *>(cond->GetUserUseList()[2]->GetValue());
    DominantTree::TreeNode *ignoreNode = m_dom.getNode(ignore);
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
    m_dom.getNode(ignore)->predNodes.remove(preheadNode);
    m_dom.getNode(prehead)->succNodes.remove(ignoreNode);
    prehead->rbegin().InsertBefore(uncond);
    delete cond;
  }
  loop->addHeader(New_header);
  AM.ChangeLoopHeader(header, New_header);
  SimplifyBlocks(header, loop, &m_dom);
  return true;
}

bool LoopRotaing::CanBeMove(User *I)
{
  if (auto call = dynamic_cast<CallInst *>(I))
  {
    if (call->HasSideEffect())
      return false;
    auto callee = dynamic_cast<Function *>(call->GetOperand(0));
    if (callee->MemWrite() || callee->MemWrite())
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

void LoopRotaing::PreservePhi(
    BasicBlock *header, BasicBlock *Latch, Loop *loop,
    BasicBlock *preheader, BasicBlock *new_header,
    std::unordered_map<Value *, Value *> &PreHeaderValue,
    LoopInfoAnalysis *loopAnlasis, DominantTree *m_dom)
{
  // bool = true ---> outside the loop
  std::map<PhiInst *, std::map<bool, Value *>> RecordPhi;
  std::map<Value *, PhiInst *> NewInsertPhi;
  std::map<PhiInst *, PhiInst *> PhiInsert;
  for (auto des : m_dom->getNode(header)->succNodes)
  {
    auto succ = m_dom->getNode(des->curBlock)->curBlock;
    for (auto iter = succ->begin(); iter != succ->end() && dynamic_cast<PhiInst *>(*iter); ++iter)
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
       (iter != header->end()) && dynamic_cast<PhiInst *>(*iter); ++iter)
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
  // ȥ��preheader��header�ı�֮������loop����������������ڣ���Ҫ����phi
  //  lcssa��֤user��loop��
  auto DealPhi = [&](PhiInst *phi, Use *use)
  {
    auto user = use->GetUser();
    if (PhiInsert.find(phi) == PhiInsert.end())
    {
      assert(phi->PhiRecord.size() == 1);
      auto new_phi = PhiInst::NewPhiNode(new_header->GetFront(), new_header, phi->GetType(), "new_phi_name");
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
        {
          p->PhiRecord[i].first = use->SetValue();
        }
      }
    }
    for (auto ex : loopAnlasis->GetExit(loop))
      for (auto _inst : *ex)
        if (auto p = dynamic_cast<PhiInst *>(_inst))
        {
          for (int i = 0; i < p->PhiRecord.size(); i++)
            if (p->PhiRecord[i].first == phi &&
                p->PhiRecord[i].second != header)
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
      for (auto iter = inst->GetUserUseList().begin();
           iter != inst->GetUserUseList().end();)
      {
        auto &use = *iter;
        ++iter;
        auto user = use->GetUser();
        auto user1 = dynamic_cast<Instruction *>(user);
        auto targetBB = user1->GetParent();
        if (targetBB == header)
          continue;
        if (!loop->ContainBB(targetBB))
          continue;
        if (targetBB == preheader)
        {
          continue;
        }
        PhiSet.emplace_back(phi, use.get());
        assist.insert(phi);
      }
      // auto usee = use->GetValue();
      // if (NewInsertPhi.find(usee) == NewInsertPhi.end()) {
      //   auto cloned = CloneMap[usee];
      //   if (!cloned)
      //     continue;
      //   auto tmp = header;
      //   while (std::distance(m_dom->GetNode(tmp->num).rev.begin(),
      //                        m_dom->GetNode(tmp->num).rev.end()) == 1) {
      //     tmp = m_dom->GetNode((m_dom->GetNode(tmp->num).rev.front()))
      //               .thisBlock;
      //   }
      //   auto new_phi =
      //       PhiInst::NewPhiNode(tmp->front(), tmp, inst->GetType());
      //   new_phi->updateIncoming(cloned, preheader);
      //   new_phi->updateIncoming(usee, tmp);
      //   user->RSUW(use, new_phi);
      //   NewInsertPhi[usee] = new_phi;
      // } else {
      //   user->RSUW(use, NewInsertPhi[usee]);
      // }
    }
    else
    {
      std::vector<Use *> Rewrite;
      for (auto iter = inst->GetUserUseList().begin(); iter != inst->GetUserUseList().end();)
      {
        auto &use = *iter;
        ++iter;
        auto user = use->GetUser();
        auto user2 = dynamic_cast<Instruction *>(user);
        auto targetBB = user2->GetParent();
        if (targetBB == header)
          continue;
        // if (!loop->Contain(targetBB))
        //   continue;
        if (targetBB == preheader)
        {
          continue;
        }
        Rewrite.push_back(use.get());
      }
      if (Rewrite.empty())
        continue;
      auto cloned = CloneMap[inst];
      if (!cloned)
        continue;
      auto ty = inst->GetType();
      if (auto ld = dynamic_cast<LoadInst *>(inst))
        ty = ld->GetOperand(0)->GetType();
      auto new_phi = PhiInst::NewPhiNode(new_header->GetFront(), new_header, inst->GetType(), "new_phi_name");
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
      // �����ݹ�ʹ��phi
      auto insert_pos = new_header->begin();
      auto phi_use = dynamic_cast<PhiInst *>(u->GetValue());
      auto bb = phi->GetParent();
      while (phi_use && (phi_use->GetParent() == bb) &&
             assist.insert(phi_use).second)
      {
        if (phi->GetUserUseListSize() == 1)
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

void LoopRotaing::SimplifyBlocks(BasicBlock *Header, Loop *loop, DominantTree *m_dom)
{
  BasicBlock *Latch = nullptr;
  for (auto rev : m_dom->getNode(Header)->predNodes)
  {
    if (!Latch)
      Latch = m_dom->getNode(rev->curBlock)->curBlock;
    else
    {
      Latch = nullptr;
      break;
    }
  }
  assert(Latch && "Must Have One Latch!");
  if (dynamic_cast<CondInst *>(Latch->GetBack()))
    return;
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
        delete phi;
      }
    }
  }
  for (auto des : m_dom->getNode(Header)->succNodes)
  {
    auto succ = m_dom->getNode(des->curBlock)->curBlock;
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
  Header->ReplaceAllUseWith(Latch);

  delete *(Latch->rbegin());
  auto iter = Header->begin();
  for (;;)
  {
    iter = Header->begin();
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
      it.InsertBefore(inst);
      continue;
    }
    inst->EraseFromManager();
    inst->SetManager(Latch);
    Latch->push_back(inst);
  }
  loopAnlasis->deleteBB(Header);

  // auto it = std::find(m_func->GetBBs().begin(), m_func->GetBBs().end(), Header);
  // m_func->GetBBs().erase(it);
  auto it = std::find(m_func->GetBBs().begin(), m_func->GetBBs().end(), Header);
  if (it != m_func->GetBBs().end())
  {
    m_func->GetBBs().erase(it);
  }
  // delete Header;
  m_func->RenumberBBs();
}

void LoopRotaing::PreserveLcssa(BasicBlock *new_exit, BasicBlock *old_exit, BasicBlock *pred)
{
  for (auto inst : *old_exit)
    if (auto phi = dynamic_cast<PhiInst *>(inst))
      for (auto &[_1, val] : phi->PhiRecord)
        if (val.second == pred)
        {
          auto Insert =
              PhiInst::NewPhiNode(new_exit->GetFront(), new_exit, phi->GetType(), "new_phinode");
          Insert->SetName(Insert->GetName() + ".lcssa");
          Insert->addIncoming(val.first, pred);
          phi->ReplaceSomeUseWith(_1, Insert);
          phi->ModifyBlock(val.second, new_exit);
          phi->PhiRecord[_1] = std::make_pair(Insert, new_exit);
        }
}

bool LoopRotaing::TryRotate(Loop *loop, DominantTree *m_dom)
{
  bool Legal = false;
  auto latch = loopAnlasis->getLatch(loop);
  auto head = loop->getHeader();
  auto prehead = loopAnlasis->getPreHeader(loop, LoopInfoAnalysis::Loose);
  auto uncond = dynamic_cast<UnCondInst *>(latch->GetBack());
  if (!uncond)
    return false;
  assert(latch);
  int pred = std::distance(m_dom->getNode(latch)->predNodes.begin(), m_dom->getNode(latch)->predNodes.end());
  if (pred != 1)
    return false;
  auto PredBB = m_dom->getNode(m_dom->getNode(latch)->predNodes.front()->curBlock)->curBlock;
  for (auto des : m_dom->getNode(PredBB)->succNodes)
  {
    auto succ = m_dom->getNode(des->curBlock)->curBlock;
    if (!loop->ContainBB(succ))
      Legal = true;
  }
  if (!Legal)
    return false;
  int times = 0;
  auto exiting = loopAnlasis->getExitingBlocks(loop);
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
          auto user = use->GetUser();
          auto user1 = dynamic_cast<Instruction *>(user);
          if (!loop->ContainBB(user1->GetParent()))
          {
            Legal = false;
            break;
          }
        }
      }
      for (auto use : inst->GetValUseList())
      {
        auto user = use->GetUser();
        auto user2 = dynamic_cast<Instruction *>(user);
        if (!loop->ContainBB(user2->GetParent()))
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
      if ((bin->GetOp() == BinaryInst::Op_Mod || bin->GetOp() == BinaryInst::Op_Mod) && HasZero)
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

  DominantTree::TreeNode *latchNode = m_dom->getNode(latch);
  DominantTree::TreeNode *PredBBNode = m_dom->getNode(PredBB);
  DominantTree::TreeNode *headNode = m_dom->getNode(head);

  for (int i = 0; i < cond->GetUserUseListSize(); i++)
  {
    if (cond->GetOperand(i) == latch)
    {
      cond->ReplaceSomeUseWith(i, head);
      loop->deleteBB(latch);
      m_dom->getNode(PredBB)->succNodes.remove(latchNode);
      m_dom->getNode(head)->predNodes.remove(latchNode);
      m_dom->getNode(head)->predNodes.push_front(PredBBNode);
      m_dom->getNode(PredBB)->succNodes.push_front(headNode);
      for (auto inst : *head)
        if (auto phi = dynamic_cast<PhiInst *>(inst))
          phi->ModifyBlock(latch, PredBB);
      break;
    }
  }
  // auto iter = std::find(m_func->GetBBs().begin(), m_func->GetBBs().end(), latch);
  // m_func->GetBBs().erase(iter); // ?????
  // m_func->RemoveBBs(latch);

  auto it = std::find(m_func->GetBBs().begin(), m_func->GetBBs().end(), latch);
  if (it != m_func->GetBBs().end())
  {
    m_func->GetBBs().erase(it);
  }

  m_func->RenumberBBs();

  return true;
}
