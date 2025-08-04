#include "../../include/IR/Opt/LoopSimping.hpp"
#include "../../include/IR/Analysis/LoopInfo.hpp"
#include "../../include/IR/Analysis/Dominant.hpp"
#include "../../include/lib/CoreClass.hpp"
#include "../../include/lib/CFG.hpp"
#include "../../include/lib/Type.hpp"
#include <algorithm>
#include <cassert>
#include <set>

bool LoopSimping::Run()
{
  bool changed = false;
  DominantTree m_dom(m_func);
  m_dom.BuildDominantTree();
  this->loopAnlay = new LoopInfoAnalysis(m_func, &m_dom, DeleteLoop);
  // 先处理内层循环
  for (auto iter = loopAnlay->loopsBegin(); iter != loopAnlay->loopsEnd(); iter++)
  {
    auto loop = *iter;
    changed |= SimplifyLoopsImpl(loop, &m_dom);
    loop->MarkSimplified(); // 直接标记
  }
  SimplifyPhi();
  return changed;
}

void LoopSimping::SimplifyPhi()
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

bool LoopSimping::SimplifyLoopsImpl(Loop *loop, DominantTree *m_dom)
{
  bool changed = false;
  std::vector<Loop *> WorkLists;
  WorkLists.push_back(loop);
  // 将子循环递归的加入进来
  for (int i = 0; i < WorkLists.size(); i++)
  {
    Loop *tmp = WorkLists[i];
    for (auto sub : *tmp)
    {
      WorkLists.push_back(sub);
    }
  }
  while (!WorkLists.empty())
  {
    Loop *L = WorkLists.back();
    WorkLists.pop_back();
    // step 1: deal with preheader
    BasicBlock *preheader = loopAnlay->getPreHeader(L);
    if (!preheader)
    {
      InsertPreHeader(L);
      changed |= true;
    }
    // step 2: deal with exit
    auto exit = loopAnlay->GetExit(L);
    for (int index = 0; index < exit.size(); ++index)
    {
      bool NeedToFormat = false;
      BasicBlock *bb = exit[index];
      for (auto rev : m_dom->getNode(bb)->predNodes)
      {
        if (!loopAnlay->IsLoopIncludeBB(L, rev->curBlock))
          NeedToFormat = true;
      }
      if (!NeedToFormat)
      {
        exit[index] = exit[exit.size() - 1];
        exit.pop_back();
        index--;
      }
    }
    for (int index = 0; index < exit.size(); index++)
    {
      FormatExit(L, exit[index], m_dom);
      changed |= true;
      exit[index] = exit[exit.size() - 1];
      exit.pop_back();
      index--;
    }
    // step 3: deal with latch/back-edge
    BasicBlock *header = L->getHeader();
    std::set<BasicBlock *> contain{L->getLoopBody().begin(), L->getLoopBody().end()};
    contain.insert(L->getHeader());
    std::vector<BasicBlock *> Latch;
    for (auto rev : m_dom->getNode(header)->predNodes)
    {
      auto B = m_dom->getNode(rev->curBlock)->curBlock;
      if (B != preheader && contain.find(B) != contain.end())
        Latch.push_back(m_dom->getNode(rev->curBlock)->curBlock);
    }
    assert(!Latch.empty());
    if (Latch.size() > 1)
    {
      FormatLatch(loop, preheader, Latch, m_dom);
      changed |= true;
    }
    else
    {
      loop->setLatch(Latch[0]);
    }
  }
  return changed;
}

void LoopSimping::InsertPreHeader(Loop *loop)
{
  // phase 1：collect ouside block and inside block
  std::vector<BasicBlock *> OutSide;
  BasicBlock *Header = loop->getHeader();
  std::set<BasicBlock *> contain{loop->getLoopBody().begin(), loop->getLoopBody().end()};

  auto headerNode = m_dom->getNode(Header);
  for (auto predNode : headerNode->predNodes)
  {
    if (contain.find(predNode->curBlock) == contain.end())
    {
      OutSide.push_back(predNode->curBlock);
    }
  }
  //  phase 2: insert the preheader
  BasicBlock *preheader = new BasicBlock();
  preheader->SetName(preheader->GetName() + "_preheader");
  m_func->InsertBlock(Header, preheader);
  // DominantTree::TreeNode *Node = dominantTree.getNode(preheader);
  DominantTree::TreeNode Node;
  Node.curBlock = preheader;
#ifdef DEBUG
  std::cerr << "insert a preheader: " << preheader->GetName() << std::endl;
#endif
  // phase 3: update the rev and des
  for (auto target : OutSide)
  {
    auto condition = target->GetBack();
    if (auto cond = dynamic_cast<CondInst *>(condition))
    {
      for (int i = 0; i <= 2; i++)
        if (dynamic_cast<User *>(cond)->GetUserUseList()[i]->GetValue() == Header)
        {
          cond->ReplaceSomeUseWith(i, preheader);
        }
    }
    else if (auto uncond = dynamic_cast<UnCondInst *>(condition))
      uncond->ReplaceSomeUseWith(0, preheader);
  }
  m_dom->Nodes.push_back(&Node);
  // phase 4: update the phiNode
  std::set<BasicBlock *> work{OutSide.begin(), OutSide.end()};
  for (auto inst : *Header)
  {
    if (auto phi = dynamic_cast<PhiInst *>(inst))
      UpdatePhiNode(phi, work, preheader);
    else
      break;
  }
  // phase 5: update the rev and des
  UpdateInfo(OutSide, preheader, Header, loop, m_dom);
  loop->setPreHeader(preheader);
}

void LoopSimping::FormatExit(Loop *loop, BasicBlock *exit, DominantTree *m_dom)
{
  std::vector<BasicBlock *> OutSide, Inside;
  // 获取 exit 对应的支配树节点
  DominantTree::TreeNode *exitTreeNode = m_dom->getNode(exit);

  // 遍历 exitTreeNode 的 rev 集合（可能是前驱、后继或支配边界节点）
  for (auto revNode : m_dom->getNode(exit)->predNodes)
  {
    // 检查当前块是否在目标循环中
    if (loopAnlay->LookUp(m_dom->getNode(revNode->curBlock)->curBlock) != loop)
      OutSide.push_back(m_dom->getNode(revNode->curBlock)->curBlock); // 不在循环内，加入 Outside
    else
      Inside.push_back(m_dom->getNode(revNode->curBlock)->curBlock); // 在循环内，加入 Inside
  }
  BasicBlock *new_exit = new BasicBlock();
  new_exit->SetName(new_exit->GetName() + "_exit");
  m_func->InsertBlock(exit, new_exit);
  // update the node info
  DominantTree::TreeNode Node;
  Node.curBlock = new_exit;
  Node.succNodes.push_front(exitTreeNode);

/*   DominantTree::TreeNode *newNode = new DominantTree::TreeNode(Node);
  m_dom->BlocktoNode[new_exit] = newNode; // 关键添加：更新映射关系
  m_dom->Nodes.push_back(newNode); */
#ifdef DEBUG
  std::cerr << "insert a exit: " << new_exit->GetName() << std::endl;
#endif
  for (auto bb : Inside)
  {
    auto condition = bb->GetBack();
    DominantTree::TreeNode *testbb = m_dom->getNode(bb);
    Node.predNodes.push_front(testbb);
    if (auto cond = dynamic_cast<CondInst *>(condition))
    {
      for (int i = 0; i < 3; i++)
      {
        if (dynamic_cast<User *>(cond)->GetUserUseList()[i]->GetValue() == exit)
        {
          cond->ReplaceSomeUseWith(i, new_exit);
        }
      }
    }
    else if (auto uncond = dynamic_cast<UnCondInst *>(condition))
    {
      uncond->ReplaceSomeUseWith(0, new_exit);
    }
  }
  DominantTree::TreeNode *newNode = new DominantTree::TreeNode(Node); // 假设有拷贝构造函数
  m_dom->BlocktoNode[new_exit] = newNode;
  m_dom->Nodes.push_back(newNode);

  std::set<BasicBlock *> work{Inside.begin(), Inside.end()};
  for (auto inst : *exit)
  {
    if (auto phi = dynamic_cast<PhiInst *>(inst))
      UpdatePhiNode(phi, work, new_exit);
    else
      break;
  }
  UpdateInfo(Inside, new_exit, exit, loop, m_dom);
}

void LoopSimping::UpdatePhiNode(PhiInst *phi, std::set<BasicBlock *> &worklist, BasicBlock *target)
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
    // phi->updateIncoming(ComingVal, target);
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
  PhiInst *pre_phi = PhiInst::NewPhiNode(target->GetFront(), target, phi->GetType(), "");
  for (auto &[i, v] : Erase)
  {
    pre_phi->addIncoming(v.first, v.second);
    phi->Del_Incomes(i);
  }
  phi->addIncoming(pre_phi, target);
  phi->FormatPhi();
  // if (phi->PhiRecord.size() == 1) {
  //   auto repl = GetOperand(phi, 0);
  //   phi->RAUW(repl);
  //   delete phi;
  // }
  return;
}

void LoopSimping::FormatLatch(Loop *loop, BasicBlock *PreHeader, std::vector<BasicBlock *> &latch, DominantTree *m_dom)
{
  BasicBlock *head = loop->getHeader();
  BasicBlock *new_latch = new BasicBlock();
  new_latch->SetName(new_latch->GetName() + "_latch");
  DominantTree::TreeNode Node;
  Node.curBlock = new_latch;
#ifdef DEBUG
  std::cerr << "insert a latch: " << new_latch->GetName() << std::endl;
#endif
  m_func->InsertBlock(head, new_latch);
  std::set<BasicBlock *> work{latch.begin(), latch.end()};
  for (auto inst : *head)
  {
    if (auto phi = dynamic_cast<PhiInst *>(inst))
      UpdatePhiNode(phi, work, new_latch);
    else
      break;
  }
  for (auto bb : latch)
  {
    auto condition = bb->GetBack();
    if (auto cond = dynamic_cast<CondInst *>(condition))
    {
      for (int i = 0; i < 3; i++)
      {
        if (dynamic_cast<User *>(cond)->GetUserUseList()[i]->GetValue() == head)
        {
          cond->ReplaceSomeUseWith(i, new_latch);
        }
      }
    }
    else if (auto uncond = dynamic_cast<UnCondInst *>(condition))
    {
      uncond->ReplaceSomeUseWith(0, new_latch);
    }
  }
  DominantTree::TreeNode *newNode = new DominantTree::TreeNode(Node); // 假设有拷贝构造函数
  m_dom->Nodes.push_back(newNode);
  m_dom->BlocktoNode[new_latch] = newNode;
  UpdateInfo(latch, new_latch, head, loop, m_dom);
  loop->setLatch(new_latch);
}
// need to ReAnlaysis loops （暂时先不使用这个功能）
Loop *LoopSimping::SplitNewLoop(Loop *L)
{
  BasicBlock *prehead = loopAnlay->getPreHeader(L);
  PhiInst *target = nullptr;
  bool FindOne = false;
  for (auto inst : *(L->getHeader()))
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
  m_func->InsertBlock(L->getHeader(), out);
  out->SetName(out->GetName() + "_out");
#ifdef DEBUG
  std::cerr << "insert a out: " << out->GetName() << std::endl;
#endif
  for (auto bb : Outer)
  {
    auto condition = bb->GetBack();
    if (auto cond = dynamic_cast<CondInst *>(condition))
    {
      for (int i = 0; i < 3; i++)
      {
        if (dynamic_cast<User *>(cond)->GetUserUseList()[i]->GetValue() == L->getHeader())
        {
          cond->ReplaceSomeUseWith(i, out);
        }
      }
    }
    else if (auto uncond = dynamic_cast<UnCondInst *>(condition))
    {
      uncond->ReplaceSomeUseWith(0, out);
    }
  }
  // UpdateInfo(Outer, out, L->GetHeader());
  //  TODO
  return nullptr;
}

void LoopSimping::UpdateInfo(std::vector<BasicBlock *> &bbs, BasicBlock *insert, BasicBlock *head, Loop *loop, DominantTree *m_dom)
{
  for (auto bb : bbs)
  {
    int a = 1;
    // 获取对应的TreeNode指针（确保使用正确类型）
    DominantTree::TreeNode *bbNode = m_dom->getNode(bb);
    DominantTree::TreeNode *headNode = m_dom->getNode(head);
    DominantTree::TreeNode *insertNode = m_dom->getNode(insert);

    // 更新支配关系（必须使用TreeNode*）
    m_dom->getNode(bb)->succNodes.remove(headNode); // 使用headNode而不是head
    m_dom->getNode(head)->predNodes.remove(bbNode); // 使用bbNode而不是bb

    // 添加新关系（同样使用TreeNode*）
    m_dom->getNode(bb)->succNodes.push_front(insertNode); // 使用insertNode而不是insent
    m_dom->getNode(insert)->predNodes.push_front(bbNode); // 使用bbNode而不是bb
  }

  // 获取对应的TreeNode指针
  DominantTree::TreeNode *headNode = m_dom->getNode(head);
  DominantTree::TreeNode *insertNode = m_dom->getNode(insert);

  // 必须使用TreeNode*操作，不能用基本块编号
  headNode->predNodes.push_front(insertNode); // 使用insertNode而不是insert->num
  insertNode->succNodes.push_front(headNode); // 使用headNode而不是head->num

  UpdateLoopInfo(head, insert, bbs);
}

void LoopSimping::CaculateLoopInfo(Loop *loop, LoopInfoAnalysis *Anlay)
{
  const auto Header = loop->getHeader();
  const auto Latch = Anlay->getLatch(loop);
  const auto br = dynamic_cast<CondInst *>(*(Latch->rbegin()));
  assert(br);
  const auto cmp = dynamic_cast<BinaryInst *>(dynamic_cast<User *>(br)->GetUserUseList()[0]->GetValue());
  loop->trait.cmp = cmp;
  PhiInst *indvar = nullptr;
  auto indvarJudge = [&](User *val) -> PhiInst *
  {
    if (auto phi = dynamic_cast<PhiInst *>(val))
      return phi;
    for (auto &use : val->GetUserUseList())
    {
      if (auto *phi = dynamic_cast<PhiInst *>(val))
      {
        if (phi->GetParent() != Header)
        {
          auto *phi = dynamic_cast<PhiInst *>(val);
          return phi;
        }
      }
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
    if (auto val = dynamic_cast<User *>(use->GetValue()))
    {

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
      indvar->ReturnValIn(Anlay->getPreHeader(loop, LoopInfoAnalysis::Loose));
}

void LoopSimping::UpdateLoopInfo(BasicBlock *Old, BasicBlock *New, const std::vector<BasicBlock *> &pred)
{
  auto l = loopAnlay->LookUp(Old);
  if (!l)
    return;
  Loop *InnerOutside = nullptr;
  for (const auto pre : pred)
  {
    auto curloop = loopAnlay->LookUp(pre);
    while (curloop != nullptr && !curloop->ContainBB(Old))
      curloop = curloop->GetParent();
    if (curloop && curloop->ContainBB(Old) &&
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