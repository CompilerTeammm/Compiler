#include "../include/Backend/PhiElimination.hpp"
#include "../include/Backend/RISCVMIR.hpp"
#include "../include/Backend/RISCVTrival.hpp"
#include <forward_list>
#include <queue>

RISCVBasicBlock *PhiElimination::find_copy_block(BasicBlock *from, BasicBlock *to)
{
  if (CopyBlockFinder.find(from) == CopyBlockFinder.end())
    CopyBlockFinder[from] = std::map<BasicBlock *, RISCVBasicBlock *>();
  if (CopyBlockFinder[from].find(to) == CopyBlockFinder[from].end())
  {
    auto mfunc = cxt.mapping(from->GetParent())->as<RISCVFunction>();
    auto mfrom = cxt.mapping(from)->as<RISCVBasicBlock>();
    auto mto = cxt.mapping(to)->as<RISCVBasicBlock>();

    auto critialmbb = RISCVBasicBlock::CreateRISCVBasicBlock();
    mfrom->replace_succ(mto, critialmbb);
    auto uncond = new RISCVMIR(RISCVMIR::_j);
    uncond->AddOperand(mto);
    critialmbb->push_back(uncond);

    mfunc->push_back(critialmbb);

    CopyBlockFinder[from][to] = critialmbb;
  }
  return CopyBlockFinder[from][to];
}

void PhiElimination::runOnGraph(BasicBlock *pred, BasicBlock *succ, std::vector<std::pair<RISCVMOperand *, RISCVMOperand *>> &vec)
{
  struct Node
  {
    std::forward_list<int> nxt;
    int indo = 0;
  };
  std::map<RISCVMOperand *, int> mp;
  for (int i = 0, limi = vec.size(); i < limi; i++)
    mp[vec[i].second] = i;
  std::vector<Node> graph(vec.size());
  for (int i = 0, limi = vec.size(); i < limi; i++)
    if (mp.find(vec[i].first) != mp.end())
    {
      auto src = i, dst = mp[vec[i].first];
      graph[src].nxt.push_front(dst);
      graph[dst].indo++;
    }

  std::queue<int> que;

  for (int i = 0, limi = vec.size(); i < limi; i++)
    if (graph[i].indo == 0)
      que.push(i);

  std::map<RISCVMOperand *, RISCVMOperand *> stagedRegister;
  RISCVBasicBlock *emitMBB;
  if (auto cond = dynamic_cast<CondInst *>(pred->GetBack()))
    emitMBB = find_copy_block(pred, succ);
  else
    emitMBB = cxt.mapping(pred)->as<RISCVBasicBlock>();

  for (int i = 0, j = 0, limi = vec.size(); i < limi;)
  {
    auto visit = [&](int ind)
    {
      i++;

      auto [src, dst] = vec[ind];
      if (stagedRegister.find(src) != stagedRegister.end())
        src = stagedRegister[src];

      if (graph[ind].indo != 0)
      {
        assert(graph[ind].indo == 1 && "Unexpected Degree");
        graph[ind].indo = 0;
        auto stageR = cxt.createVReg(src->GetType());
        emitMBB->push_before_branch(RISCVTrival::CopyFrom(stageR, dst));
        stagedRegister[dst] = stageR;
      }

      emitMBB->push_before_branch(RISCVTrival::CopyFrom(dst->as<VirRegister>(), src));

      auto &node = graph[ind];
      for (auto nxt : node.nxt)
      {
        auto &nnode = graph[nxt];
        nnode.indo--;
        if (nnode.indo == 0)
          que.push(nxt);
      }
    };
    while (!que.empty())
    {
      int cur = que.front();
      que.pop();
      visit(cur);
    }
    if (i >= limi)
    {
      assert(i == limi);
      break;
    }
    for (; j < limi; j++)
    {
      if (graph[j].indo)
      {
        visit(j);
        if (!que.empty())
          break;
      }
    }
  }
}

void PhiElimination::runonBasicBlock(BasicBlock *bb)
{
  std::map<BasicBlock *, std::vector<std::pair<RISCVMOperand *, RISCVMOperand *>>> phiMap;
  for (auto inst : *bb)
  {
    if (auto phi = dynamic_cast<PhiInst *>(inst))
    {
      auto &phirec = phi->PhiRecord;
      for (auto &[idx, pair] : phirec)
      {
        auto [val, bb] = pair;
        if (val->IsUndefVal())
          continue;
        auto src = cxt.mapping(val);
        auto dst = cxt.mapping(phi);
        phiMap[bb].emplace_back(src, dst);
      }
    }
    else
      break;
  }
  for (auto &[src, vec] : phiMap)
    runOnGraph(src, bb, vec);
}

bool PhiElimination::run(Function *f)
{
  for (auto bb : *f)
    runonBasicBlock(bb);
  return true;
}