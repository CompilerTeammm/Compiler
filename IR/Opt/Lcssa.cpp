#include "../../include/IR/Opt/Lcssa.hpp"

bool LcSSA::run()
{
  m_dom = AM.get<DominantTree>(m_func);
  loops = AM.get<LoopAnalysis>(m_func, m_dom, std::ref(DeleteLoop));
  bool changed = false;
  if (!loops->CanBeOpt())
    return changed;
  for (auto l = loops->begin(); l != loops->end();)
  {
    auto loop = *l;
    l++;
    changed |= DFSLoops(loop);
    AM.AddAttr(loop->GetHeader(), Lcssa);
  }
  return changed;
}

bool LcSSA::DFSLoops(LoopInfo *l)
{
  std::vector<Instruction *> FormingInsts;
  for (auto sub : *l)
  {
    DFSLoops(sub);
  }
  std::set<BasicBlock *> ContainBB{l->GetLoopBody().begin(),
                                   l->GetLoopBody().end()};
  ContainBB.insert(l->GetHeader());
  for (const auto bb : ContainBB)
  {
    for (const auto inst : *bb)
    {
      if (inst->GetUserUseListSize() == 0)
        continue;
      for (const auto use : inst->GetValUseList())
      {
        if (auto *instr = dynamic_cast<Instruction *>(use->GetUser()))
        {
          if (ContainBB.find(instr->GetParent()) == ContainBB.end())
          {
            // if (auto phi = dynamic_cast<PhiInst *>(instr))
            //   if (phi->GetName().find("lcssa") != std::string::npos)
            //     continue;
            FormingInsts.push_back(inst);
          }
        }
      }
    }
  }
  if (FormingInsts.empty())
    return false;
  return FormalLcSSA(FormingInsts);
}

bool LcSSA::FormalLcSSA(std::vector<Instruction *> &FormingInsts)
{
  int x = 0;
  bool changed = false;
  std::set<User *> Erase;
  while (!FormingInsts.empty())
  {
    std::vector<Instruction *> ShouldRedoLater;
    std::set<PhiInst *> AddPhi;
    std::vector<Use *> Rewrite; // 记录inst的所有在外部use
    std::set<BasicBlock *> ContainBB;
    auto target = PopBack(FormingInsts);
    auto GetNameEnum = [](Value *val)
    {
      int p = 0;
      auto name = val->GetName() + ".lcssa." + std::to_string(p++);
      for (auto use : val->GetValUseList())
      {
        auto user = use->GetUser();
        if (user->GetName() == name)
          name = val->GetName() + ".lcssa." + std::to_string(p++);
      }
      return name;
    };

    auto l = loops->LookUp(target->GetParent());
    std::vector<BasicBlock *> exit = loops->GetExit(l);
    ContainBB.insert(l->GetLoopBody().begin(), l->GetLoopBody().end());
    ContainBB.insert(l->GetHeader());
    if (exit.size() <= 0)
      return changed;
    for (auto use : target->GetValUseList())
    {
      auto userInst = dynamic_cast<Instruction *>(use->GetUser());
      if (!userInst)
        continue; // 不是指令，跳过

      BasicBlock *pbb = userInst->GetParent();
      if (auto phi = dynamic_cast<PhiInst *>(userInst))
      {
        pbb = phi->ReturnBBIn(use);
      }

      if (!ContainBB.count(pbb))
        Rewrite.push_back(use);
    }

    if (Rewrite.empty())
      continue;
    x = 0;
    for (auto ex : exit)
    {
      if (!m_dom->dominates(target->GetParent(), ex))
        continue;
      auto phi = PhiInst::NewPhiNode(ex->GetFront(), ex, target->GetType(),
                                     GetNameEnum(target));
      for (auto predNode : m_dom->getNode(ex)->predNodes)
      {
        phi->addIncoming(target, predNode->curBlock);
      }
      changed |= true;
      if (auto subloop = loops->LookUp(ex))
        if (!l->Contain(subloop))
        {
          // 插入的phi如果在另一个循环存在，重新考虑处理
          ShouldRedoLater.push_back(phi);
        }
      AddPhi.insert(phi);
    }
    for (auto rewrite : Rewrite)
    {
      auto userInst = dynamic_cast<Instruction *>(rewrite->GetUser());
      //if (!userInst)
       // return; // 或者 continue / break，根据你的上下文逻辑处理

      BasicBlock *pbb = userInst->GetParent();
      if (auto phi = dynamic_cast<PhiInst *>(userInst))
      {
        pbb = phi->ReturnBBIn(rewrite);
      }
      // 如果直接是在exitbb
      auto it = std::find(exit.begin(), exit.end(), pbb);
      if (it != exit.end())
      {
        // 此处可以直接替换
        auto _val = rewrite->GetValue();
        assert(dynamic_cast<PhiInst *>(pbb->GetFront()));
        rewrite->RemoveFromValUseList(rewrite->GetUser());
        rewrite->SetValue() = pbb->GetFront();
        pbb->GetFront()->GetValUseList().push_front(rewrite);
        if (auto phi = dynamic_cast<PhiInst *>(userInst))
        {
          // 更新record
          for (auto &[_1, val] : phi->PhiRecord)
          {
            if (val.first == _val)
            {
              val.first = pbb->GetFront();
            }
          }
        }
        continue;
      }

      // 需要进一步检查
      std::set<BasicBlock *> ex{exit.begin(), exit.end()};
      InsertedPhis.clear();
      InsertPhis(rewrite, ex);

      for (auto phi : InsertedPhis)
      {
        if (auto subloop = loops->LookUp(phi->GetParent()))
          if (!l->Contain(subloop))
            ShouldRedoLater.push_back(phi);
      }
    }
    for (auto phi : ShouldRedoLater)
    {
      if (phi->GetValUseListSize() == 0)
      {
        Erase.insert(phi);
        continue;
      }
      FormingInsts.push_back(phi);
    }
    for (auto phi : AddPhi)
    {
      if (phi->GetValUseListSize() == 0)
        Erase.insert(phi);
    }
  }
  for (auto d : Erase)
  {
    delete d;
  }
  return changed;
}

void LcSSA::InsertPhis(Use *u, std::set<BasicBlock *> &exit)
{
  std::set<BasicBlock *> target;
  std::set<BasicBlock *> visited;

  auto userInst = dynamic_cast<Instruction *>(u->GetUser());
  if (!userInst)
    return; // 或者根据上下文选择 continue / break

  auto _val = u->GetValue();
  auto targetBB = userInst->GetParent();
  if (auto phi = dynamic_cast<PhiInst *>(userInst))
    targetBB = phi->ReturnBBIn(u);
  FindBBRecursive(exit, target, visited, targetBB);
  assert(!target.empty());
  // 开始创建phi函数
  if (target.size() == 1)
  {
    auto p = *(target.begin());
    assert(dynamic_cast<PhiInst *>(p->GetFront()));
    u->RemoveFromValUseList(u->GetUser());
    u->SetValue() = p->GetFront();
    p->GetFront()->GetValUseList().push_front(u);
    if (auto phi = dynamic_cast<PhiInst *>(u->GetUser()))
    {
      // 更新record
      for (auto &[_1, val] : phi->PhiRecord)
      {
        if (val.first == _val)
        {
          val.first = p->GetFront();
        }
      }
    }
  }
  else if (target.size() > 1)
  {
    BasicBlock *b = nullptr;
    for (auto bb : target)
    {
      auto node = m_dom->getNode(bb);
      if (node->succNodes.empty()) // 没有后继就跳过
        continue;

      auto des_1 = *node->succNodes.begin(); // 第一个后继 TreeNode*

      if (!b)
      {
        b = des_1->curBlock;
        continue;
      }
      if (b != des_1->curBlock)
      {
        b = nullptr;
        continue;
      }
    }
    assert(b && "What happen?");
    auto phi = PhiInst::NewPhiNode(b->GetFront(), b,
                                   (*(target.begin()))->GetFront()->GetType(),
                                   u->GetValue()->GetName() + ".phi.lcssa");
    InsertedPhis.insert(phi);
    for (auto bb : target)
      phi->addIncoming(bb->GetFront(), bb);
    u->RemoveFromValUseList(u->GetUser());
    u->SetValue() = phi;
    phi->GetValUseList().push_front(u);
    if (auto _phi = dynamic_cast<PhiInst *>(u->GetUser()))
    {
      // 更新record
      for (auto &[_1, val] : _phi->PhiRecord)
      {
        if (val.first == _val)
        {
          val.first = phi;
        }
      }
    }
  }
}

void LcSSA::FindBBRecursive(std::set<BasicBlock *> &exit,
                            std::set<BasicBlock *> &target,
                            std::set<BasicBlock *> &visited, BasicBlock *bb)
{
  if (visited.insert(bb).second)
  {
    if (exit.find(bb) != exit.end())
    {
      target.insert(bb);
      return;
    }
    for (auto predNode : m_dom->getNode(bb)->predNodes)
    {
      auto predbb = predNode->curBlock;
      FindBBRecursive(exit, target, visited, predbb);
    }
  }
}

void LcSSA::FindBBRoot(BasicBlock *src, BasicBlock *dst,
                       std::set<BasicBlock *> &visited,
                       std::stack<BasicBlock *> &assist)
{
  for (auto succNode : m_dom->getNode(src)->succNodes)
  {
    auto succ = succNode->curBlock;
    if (visited.insert(succ).second && succ != dst)
    {
      assist.push(succ);
      FindBBRoot(succ, dst, visited, assist);
    }
  }

  assist.pop();
}