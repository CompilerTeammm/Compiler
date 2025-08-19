#include "../../include/IR/Opt/LoopSimplify.hpp"

bool LoopSimplify::run()
{
  bool changed = false;
  // if (m_func->tag != Function::Normal)
  //   return false;
  m_dom = AM.get<DominantTree>(m_func);
  loopAnlay = AM.get<LoopAnalysis>(m_func, m_dom, std::ref(DeleteLoop));
  // 先处理内层循环
  for (auto iter = loopAnlay->begin(); iter != loopAnlay->end(); iter++)
  {
    auto loop = *iter;
    changed |= SimplifyLoopsImpl(loop);
    AM.AddAttr(loop->GetHeader(), Simplified);
  }
  SimplifyPhi();
  return changed;
}

void LoopSimplify::SimplifyPhi()
{
  for (auto bb : *m_func)
    for (auto iter = bb->begin();
         iter != bb->end() && dynamic_cast<PhiInst *>(*iter);)
    {
      auto phi = dynamic_cast<PhiInst *>(*iter);
      ++iter;
      int num = 0;
      Value *Rep = nullptr;
      for (const auto &[index, val] : phi->PhiRecord)
      {
        if (val.first != phi)
        {
          num++;
          Rep = val.first;
          if (num > 1)
            break;
        }
      }
      if (num == 1)
      {
        phi->ReplaceAllUseWith(Rep);
        delete phi;
      }
    }
}

bool LoopSimplify::SimplifyLoopsImpl(LoopInfo *loop)
{
  bool changed = false;
  std::vector<LoopInfo *> WorkLists;
  WorkLists.push_back(loop);

  // 递归收集所有子循环
  for (int i = 0; i < WorkLists.size(); i++)
  {
    LoopInfo *tmp = WorkLists[i];
    for (auto sub : *tmp)
      WorkLists.push_back(sub);
  }

  while (!WorkLists.empty())
  {
    LoopInfo *L = WorkLists.back();
    WorkLists.pop_back();

    // Step 1: 处理preheader
    BasicBlock *preheader = loopAnlay->GetPreHeader(L);
    if (!preheader)
    {
      InsertPreHeader(L);
      changed |= true;
    }

    // Step 2: 处理exit blocks
    auto exit = loopAnlay->GetExit(L);
    for (int index = 0; index < exit.size(); ++index)
    {
      bool NeedToFormat = false;
      BasicBlock *bb = exit[index];

      // 使用新版本DominantTree的API获取前驱
      auto preds = m_dom->getNode(bb)->predNodes;
      for (auto rev : preds)
      {
        if (!loopAnlay->IsLoopIncludeBB(L, rev->curBlock))
          NeedToFormat = true;
      }

      if (!NeedToFormat)
        vec_pop(exit, index);
    }

    for (int index = 0; index < exit.size(); index++)
    {
      FormatExit(L, exit[index]);
      changed |= true;
      vec_pop(exit, index);
    }

    // Step 3: 处理latch/back-edge
    BasicBlock *header = L->GetHeader();
    std::set<BasicBlock *> contain{L->GetLoopBody().begin(),
                                   L->GetLoopBody().end()};
    contain.insert(L->GetHeader());

    std::vector<BasicBlock *> Latch;
    auto preds = m_dom->getPredBBs(header);
    for (auto it = preds.rbegin(); it != preds.rend(); ++it)
    {
      BasicBlock *rev = *it;
      if (rev != preheader && contain.find(rev) != contain.end())
        Latch.push_back(rev);
    }

    assert(!Latch.empty());
    if (Latch.size() > 1)
    {
      FormatLatch(loop, preheader, Latch);
      changed |= true;
    }
    else
    {
      loop->setLatch(Latch[0]);
    }
  }
  return changed;
}

void LoopSimplify::InsertPreHeader(LoopInfo *loop)
{
  // Phase 1: Collect outside blocks
  std::vector<BasicBlock *> OutSide;
  BasicBlock *Header = loop->GetHeader();
  std::set<BasicBlock *> contain{loop->GetLoopBody().begin(),
                                 loop->GetLoopBody().end()};

  // 使用新版本的DominantTree API获取前驱块
  auto preds = m_dom->getPredBBs(Header);
  for (auto pred : preds)
  {
    if (contain.find(pred) == contain.end())
    {
      OutSide.push_back(pred);
    }
  }

  // Phase 2: Create and insert preheader
  BasicBlock *preheader = new BasicBlock();
  preheader->SetName(preheader->GetName() + "_preheader");
  m_func->InsertBlock(Header, preheader);

  // 更新DominantTree节点
  DominantTree::TreeNode *preheaderNode = new DominantTree::TreeNode();
  preheaderNode->curBlock = preheader;
  m_dom->Nodes.push_back(preheaderNode);
  m_dom->BlocktoNode[preheader] = preheaderNode;

#ifdef DEBUG
  std::cerr << "Insert a preheader: " << preheader->GetName() << std::endl;
#endif

  // Phase 3: Update branch instructions
  for (auto target : OutSide)
  {
    auto terminator = target->GetTerminator();
    if (auto cond = dynamic_cast<CondInst *>(terminator))
    {
      for (int i = 0; i <= 2; i++)
      {
        if (cond->GetOperand(i) == Header)
        {
          cond->ReplaceSomeUseWith(i, preheader);
        }
      }
    }
    else if (auto uncond = dynamic_cast<UnCondInst *>(terminator))
    {
      uncond->ReplaceSomeUseWith(0, preheader);
    }
  }

  // Phase 4: Update Phi nodes
  std::set<BasicBlock *> work{OutSide.begin(), OutSide.end()};
  for (auto inst : *Header)
  {
    if (auto phi = dynamic_cast<PhiInst *>(inst))
    {
      UpdatePhiNode(phi, work, preheader);
    }
    else
    {
      break;
    }
  }

  // Phase 5: Update CFG and dominance info
  UpdateInfo(OutSide, preheader, Header, loop);
  loop->setPreHeader(preheader);

  // 重建支配树信息
  m_dom->BuildDominantTree();
}

void LoopSimplify::FormatExit(LoopInfo *loop, BasicBlock *exit)
{
  // Phase 1: Classify predecessors into outside and inside blocks
  std::vector<BasicBlock *> OutSide, Inside;
  auto preds = m_dom->getPredBBs(exit);

  for (auto pred : preds)
  {
    if (loopAnlay->LookUp(pred) != loop)
    {
      OutSide.push_back(pred);
    }
    else
    {
      Inside.push_back(pred);
    }
  }

  // Phase 2: Create and insert new exit block
  BasicBlock *new_exit = new BasicBlock();
  new_exit->SetName(new_exit->GetName() + "_exit");
  m_func->InsertBlock(exit, new_exit);

  // Update DominantTree node info
  DominantTree::TreeNode *newNode = new DominantTree::TreeNode();
  newNode->curBlock = new_exit;
  newNode->succNodes.push_back(m_dom->BlocktoNode[exit]);
  m_dom->Nodes.push_back(newNode);
  m_dom->BlocktoNode[new_exit] = newNode;

#ifdef DEBUG
  std::cerr << "Insert an exit: " << new_exit->GetName() << std::endl;
#endif

  // Phase 3: Update branch instructions from inside blocks
  for (auto bb : Inside)
  {
    auto terminator = bb->GetTerminator();
    if (auto cond = dynamic_cast<CondInst *>(terminator))
    {
      for (int i = 0; i < 3; i++)
      {
        if (cond->GetOperand(i) == exit)
        {
          cond->ReplaceSomeUseWith(i, new_exit);
        }
      }
    }
    else if (auto uncond = dynamic_cast<UnCondInst *>(terminator))
    {
      uncond->ReplaceSomeUseWith(0, new_exit);
    }

    // Update predecessor info
    newNode->predNodes.push_back(m_dom->BlocktoNode[bb]);
  }

  // Phase 4: Update Phi nodes in the original exit block
  std::set<BasicBlock *> work{Inside.begin(), Inside.end()};
  for (auto inst : *exit)
  {
    if (auto phi = dynamic_cast<PhiInst *>(inst))
    {
      UpdatePhiNode(phi, work, new_exit);
    }
    else
    {
      break;
    }
  }

  // Phase 5: Update CFG and dominance info
  UpdateInfo(Inside, new_exit, exit, loop);
}

void LoopSimplify::UpdatePhiNode(PhiInst *phi, std::set<BasicBlock *> &worklist,
                                 BasicBlock *target)
{
  Value *ComingVal = nullptr;
  for (auto &[_1, tmp] : phi->PhiRecord)
  {
    if (worklist.find(tmp.second) != worklist.end())
    {
      if (ComingVal == nullptr)
      {
        ComingVal = tmp.first;
        continue;
      }
      if (ComingVal != tmp.second)
      {
        ComingVal = nullptr;
        break;
      }
    }
  }
  // 传入的数据流对应的值为一个
  if (ComingVal)
  {
    // std::vector<int> Erase;
    for (auto &[_1, tmp] : phi->PhiRecord)
    {
      if (worklist.find(tmp.second) != worklist.end())
      {
        tmp.second = target;
      }
    }
    // for (auto i : Erase)
    //   phi->Del_Incomes(i);
    // phi->addIncoming(ComingVal, target);
    // phi->FormatPhi();

    return;
  }
  // 对应的传入值有多个
  std::vector<std::pair<int, std::pair<Value *, BasicBlock *>>> Erase;
  for (auto &[_1, tmp] : phi->PhiRecord)
  {
    if (worklist.find(tmp.second) != worklist.end())
    {
      Erase.push_back(std::make_pair(_1, tmp));
    }
  }
  bool same = std::all_of(Erase.begin(), Erase.end(), [&Erase](auto &ele)
                          { return ele.second.first == Erase.front().second.first; });
  Value *sameval = Erase.front().second.first;
  if (same)
  {
    for (auto &[i, v] : Erase)
    {
      phi->Del_Incomes(i);
    }
    phi->addIncoming(sameval, target);
    phi->FormatPhi();
    return;
  }
  PhiInst *pre_phi =
      PhiInst::NewPhiNode(target->GetFront(), target, phi->GetType());
  for (auto &[i, v] : Erase)
  {
    pre_phi->addIncoming(v.first, v.second);
    phi->Del_Incomes(i);
  }
  phi->addIncoming(pre_phi, target);
  phi->FormatPhi();
  return;
}

void LoopSimplify::FormatLatch(LoopInfo *loop, BasicBlock *PreHeader,
                               std::vector<BasicBlock *> &latch)
{
  BasicBlock *head = loop->GetHeader();

  // 创建新的 latch block
  BasicBlock *new_latch = new BasicBlock();
  new_latch->SetName(new_latch->GetName() + "_latch");
  m_func->InsertBlock(head, new_latch);

#ifdef DEBUG
  std::cerr << "Insert a latch: " << new_latch->GetName() << std::endl;
#endif

  // 创建对应的 DominantTree 节点
  DominantTree::TreeNode *newNode = new DominantTree::TreeNode();
  newNode->curBlock = new_latch;
  newNode->visited = false;
  newNode->dfs_order = 0; // 后续可由 DFS 填充
  newNode->dfs_fa = nullptr;
  newNode->sdom = newNode;
  newNode->idom = nullptr;

  // 加入 DominantTree
  m_dom->Nodes.push_back(newNode);
  m_dom->BlocktoNode[new_latch] = newNode;

  // 更新 header 中的 phi 节点
  std::set<BasicBlock *> work{latch.begin(), latch.end()};
  for (auto inst : *head)
  {
    if (auto phi = dynamic_cast<PhiInst *>(inst))
    {
      UpdatePhiNode(phi, work, new_latch);
    }
    else
    {
      break;
    }
  }

  // 重定向原来 latches 的分支到新的 latch
  for (auto bb : latch)
  {
    auto terminator = bb->GetTerminator();
    if (auto cond = dynamic_cast<CondInst *>(terminator))
    {
      for (int i = 0; i < 3; i++)
      {
        if (cond->GetOperand(i) == head)
        {
          cond->ReplaceSomeUseWith(i, new_latch);
        }
      }
    }
    else if (auto uncond = dynamic_cast<UnCondInst *>(terminator))
    {
      uncond->ReplaceSomeUseWith(0, new_latch);
    }
  }

  // 更新 loop 信息
  UpdateInfo(latch, new_latch, head, loop);
  loop->setLatch(new_latch);
}

// need to ReAnlaysis loops （暂时先不使用这个功能）
LoopInfo *LoopSimplify::SplitNewLoop(LoopInfo *L)
{
  BasicBlock *prehead = loopAnlay->GetPreHeader(L);
  PhiInst *target = nullptr;
  bool FindOne = false;
  for (auto inst : *(L->GetHeader()))
  {
    if (auto phi = dynamic_cast<PhiInst *>(inst))
    {
      for (auto &[_1, val] : phi->PhiRecord)
      {
        if (val.first == phi)
        {
          target = phi;
          FindOne = true;
          break;
        }
      }
    }
    else
      return nullptr;
    if (FindOne)
      break;
  }
  assert(target && "phi in there must be a nullptr");
  std::vector<BasicBlock *> Outer;
  for (auto &[_1, val] : target->PhiRecord)
  {
    if (val.first != target)
      Outer.push_back(val.second);
  }
  BasicBlock *out = new BasicBlock();
  m_func->InsertBlock(L->GetHeader(), out);
  out->SetName(out->GetName() + "_out");
  for (auto bb : Outer)
  {
    auto condition = bb->GetBack();
    if (auto cond = dynamic_cast<CondInst *>(condition))
    {
      for (int i = 0; i < 3; i++)
        if (GetOperand(cond, i) == L->GetHeader())
          cond->ReplaceSomeUseWith(i, out);
    }
    else if (auto uncond = dynamic_cast<UnCondInst *>(condition))
    {
      uncond->ReplaceSomeUseWith(0, out);
    }
  }
  return nullptr;
}

void LoopSimplify::UpdateInfo(std::vector<BasicBlock *> &bbs,
                              BasicBlock *insert, BasicBlock *head,
                              LoopInfo *loop)
{
  auto headNode = m_dom->getNode(head);
  auto insertNode = m_dom->getNode(insert);

  for (auto bb : bbs)
  {
    auto bbNode = m_dom->getNode(bb);

    // 移除 bb -> head
    bbNode->succNodes.remove(headNode);
    headNode->predNodes.remove(bbNode);

    // 添加 bb -> insert
    bbNode->succNodes.push_front(insertNode);
    insertNode->predNodes.push_front(bbNode);
  }

  // 添加 insert -> head
  headNode->predNodes.push_front(insertNode);
  insertNode->succNodes.push_front(headNode);

  UpdateLoopInfo(head, insert, bbs);
}

void LoopSimplify::CaculateLoopInfo(LoopInfo *loop, LoopAnalysis *Anlay)
{
  const auto Header = loop->GetHeader();
  const auto Latch = Anlay->GetLatch(loop);
  const auto br = dynamic_cast<CondInst *>(*(Latch->rbegin()));
  assert(br);
  const auto cmp = dynamic_cast<BinaryInst *>(GetOperand(br, 0));
  loop->trait.cmp = cmp;
  PhiInst *indvar = nullptr;
  auto indvarJudge = [&](User *val) -> PhiInst *
  {
    if (auto phi = dynamic_cast<PhiInst *>(val))
      return phi;
    for (auto &use : val->GetUserUseList())
    {
      if (auto phi = dynamic_cast<PhiInst *>(use->GetValue()))
      {
        if (phi->GetParent() != Header)
          return nullptr;
        return phi;
      }
    }
    return nullptr;
  };
  for (auto &use : cmp->GetUserUseList())
  {
    if (auto val = dynamic_cast<Instruction *>(use->GetValue())) // xxxxxx
    {
      if (dynamic_cast<PhiInst *>(val) && val->GetParent() != Header)
      {
        return;
      }
      if (auto phi = indvarJudge(val))
      {
        if (!indvar)
        {
          indvar = phi;
          auto bin = dynamic_cast<BinaryInst *>(use->GetValue());
          if (!bin)
            return;
          loop->trait.change = bin;
          for (auto &use : bin->GetUserUseList())
          {
            if (dynamic_cast<PhiInst *>(use->GetValue()))
              continue;
            if (auto con = dynamic_cast<ConstIRInt *>(use->GetValue()))
              loop->trait.step = con->GetVal();
          }
          continue;
        }
        if (indvar)
          assert(0 && "What happen?");
      }
    }
    if (!indvar)
      return;
    if (use->GetValue() != loop->trait.change)
    {
      loop->trait.boundary = use->GetValue();
    }
  }
  loop->trait.indvar = indvar;
  loop->trait.initial =
      indvar->ReturnValIn(Anlay->GetPreHeader(loop, LoopAnalysis::Loose));
}

void LoopSimplify::UpdateLoopInfo(BasicBlock *Old, BasicBlock *New,
                                  const std::vector<BasicBlock *> &pred)
{
  auto l = loopAnlay->LookUp(Old);
  if (!l)
    return;
  LoopInfo *InnerOutside = nullptr;
  for (const auto pre : pred)
  {
    auto curloop = loopAnlay->LookUp(pre);
    while (curloop != nullptr && !curloop->Contain(Old))
      curloop = curloop->GetParent();
    if (curloop && curloop->Contain(Old) &&
        (!InnerOutside ||
         InnerOutside->GetLoopDepth() < curloop->GetLoopDepth()))
      InnerOutside = curloop;
    if (InnerOutside)
    {
      loopAnlay->setLoop(New, InnerOutside);
      while (InnerOutside != nullptr)
      {
        InnerOutside->InsertLoopBody(New);
        InnerOutside = InnerOutside->GetParent();
      }
    }
  }
}