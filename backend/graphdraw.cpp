#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <unordered_set>
// #include "../include/backend/RISCVFrameContext.hpp"
// #include "../include/backend/RISCVMIR.hpp"
// #include "../include/backend/RISCVRegister.hpp"
// #include "../include/backend/RISCVType.hpp"
void GraphColor::RunOnFunc() {
  bool condition = true;
  GC_init();
  for (auto b : *m_func)
    CalCulateSucc(b);
  CaculateTopu(m_func->front());
  std::reverse(topu.begin(), topu.end());
  while (condition) {
    condition = false;
    CaculateLiveness();
    MakeWorklist();
    do {
      if (!simplifyWorkList.empty())
        simplify();
      else if (!worklistMoves.empty())
        coalesce();
      else if (!freezeWorkList.empty())
        freeze();
      else if (!spillWorkList.empty())
        spill();
    } while (!simplifyWorkList.empty() || !worklistMoves.empty() ||
             !freezeWorkList.empty() || !spillWorkList.empty());
    for (auto sp : SpillStack) {
      auto it = std::find(selectstack.begin(), selectstack.end(), sp);
      if (it == selectstack.end())
        assert(0);
      selectstack.erase(it);
    }
    std::reverse(SpillStack.begin(), SpillStack.end());
    selectstack.insert(selectstack.end(), SpillStack.begin(), SpillStack.end());
    AssignColors();
    if (!spilledNodes.empty()) {
      SpillNodeInMir();
      condition = true;
    }
  }
  RewriteProgram();
}

void GraphColor::MakeWorklist() {
  for (auto node : initial) {
    //添加溢出节点
    if (Degree[node] > GetRegNums(node))
      spillWorkList.insert(node);
    else if (MoveRelated(node).size() != 0)
      freezeWorkList.insert(node);
    else {
      simplifyWorkList.push_back(node);
    }
  }
}
