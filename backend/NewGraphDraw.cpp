#include "../include/Backend/RISCVFrameContext.hpp"
#include "../include/Backend/RISCVMIR.hpp"
#include "../include/Backend/RISCVMOperand.hpp"
#include "../include/Backend/RISCVRegister.hpp"
#include "../include/Backend/RISCVType.hpp"
#include "../include/Backend/RegAlloc.hpp"
#include "../util/my_stl.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <unordered_set>

//入口
void GraphColor::RunOnFunc(){
    bool condition=true;
    GC_init();//初始化图着色相关结构
    for(auto b:*m_func){
        CalCulateSucc(b);//计算每个基本块后继用于构建控制流图
    }
    CaculateTopu(m_func->front());//对基本块做拓补排序,计算支配关系等
    std::reverse(topu.begin(),topu.end());//逆序方便数据流分析
    while (condition) {
        condition = false;
        CaculateLiveness();//活跃变量分析,构建干涉图
        MakeWorklist();//将变量分到不同的工作集里（简化、冻结、合并、溢出）
        do {
          if (!simplifyWorkList.empty()){
            simplify();// 简化图，删除度数小于寄存器数的节点    
          }else if (!worklistMoves.empty()){
            coalesce();// 合并move相关的变量，减少move指令
          }else if (!freezeWorkList.empty()){
            freeze();// 冻结move指令相关的节点，改为普通节点
          }else if (!spillWorkList.empty()){
            spill();// 溢出某个变量到内存
          }
        } while (!simplifyWorkList.empty() || !worklistMoves.empty() ||!freezeWorkList.empty() || !spillWorkList.empty());
        for (auto sp : SpillStack) {//这是处理spilled nodes 的部分
          auto it = std::find(selectstack.begin(), selectstack.end(), sp);//看它是否在着色选择栈中
          if (it == selectstack.end())
            assert(0);//如果找不到,那就不合理,
          selectstack.erase(it);//移除,因为接下来要调整溢出变量的优先级,放在栈的""底部"
        }
        std::reverse(SpillStack.begin(), SpillStack.end());
        //为什么要反转？ 因为 selectstack 是栈结构（后进先出），我们希望 spill 的变量 优先级低，放在最后被处理，所以要反转它的顺序。
        selectstack.insert(selectstack.end(), SpillStack.begin(), SpillStack.end());
        // 把所有 spill 的变量重新加回 selectstack 的栈底（末尾）。
        // 作用是：
        // 确保 spill 的变量出现在颜色选择过程中。
        // 但因为它们优先级最低，会最后尝试分配寄存器。
        AssignColors();//实际分配颜色(物理寄存器)
        if (!spilledNodes.empty()) {
          SpillNodeInMir();// 将spill节点在MIR中插入load/store指令
          condition = true;//由于插入了新的虚拟寄存器，要重新图着色
        }
      }
      RewriteProgram();//将寄存器分配的结果写入MIR
}

void GraphColor::MakeWorklist(){
    for(auto node:initial){
        //添加溢出节点
        if(Degree[node] > GetRegNums(node)){
            spillWorkList.insert(node);//度数过高，先假设溢出
        }else if(MoveRelated(node).size()!=0){
            freezeWorkList.insert(node);//与move指令有关，暂时冻结
        }else{
            simplifyWorkList.push_back(node);//可以直接简化的节点
        }
    }
}
//判断某个变量是否与尚未处理的 move 指令有关。
std::unordered_set<RISCVMIR*> GraphColor::MoveRelated(MOperand v){
  std::unordered_set<RISCVMIR*> tmp;
  if(moveList.find(v)==moveList.end()){
    return tmp;
  }
  for(auto inst:moveList[v]){
    auto iter=std::find(worklistMoves.begin(),worklistMoves.end(),inst);
    if(activeMoves.find(inst)!=activeMoves.end()||iter!=worklistMoves.end()){
      tmp.insert(inst);//还未完成，加到tmp中后续处理
    }
  }
  return tmp;
}

//查找一个变量合并后的代表节点（类似并查集）
MOperand GraphColor::GetAlias(MOperand v){
  //当前v存在于coalescedNodes，则返回v的alias
  if(coalescedNodes.find(v)!=coalescedNodes.end()){
    return GetAlias(alias[v]);
    // 向上传递直到找到根节点
  }else{
    return v;
  }
}

// 合并 src 和 dst 可以省去 mov dst, src 这样的指令；
// 但合并后可能导致寄存器分配失败（图着色冲突）；
// 所以我们引入两个启发式判断方法：

// George：强调 src 的邻居对 dst 是否友好（安全）；

// Briggs：评估合并后整体邻居中度数过高的数量是否可控。
bool GraphColor::GeorgeCheck(MOperand dst,MOperand src,RISCVType type){
  //George 启发式适用于 src 是普通节点、dst 是预着色节点。
  if(type==riscv_i32||type=riscv_ptr||type=riscv_i64){
    auto x=Adjacent(src);//获取src的邻接节点
    for(auto tmp:x){
      // bool ok=false;
      // if(Degree[tmp] < reglist.GetReglistInt().size()){
      //   ok|=true;
      // }
      // if(Precolored.find(tmp) != Precolored.end()){
      //   ok|=true;
      // }
      // auto &adj=adjSet[dst];
      // if(adj.find(tmp)!=Precolored.end()){
      //   ok|=true;
      // }
      // if(ok!=true){
      //   return false;
      // }
      //等价简洁版：

      // 判断当前 tmp 是否对 dst 是“安全”的邻居：
      // 满足任意一个条件即可：
      // 1. tmp 的度数小于可用寄存器数 ⇒ 易于着色
      // 2. tmp 是预着色的（物理寄存器） ⇒ 不冲突
      // 3. tmp 与 dst 已经相邻 ⇒ 不会新增冲突边
      auto &adj = adjSet[dst];
      if (!(Degree[tmp] < reglist.GetReglistInt().size() ||
            Precolored.find(tmp) != Precolored.end() ||
            adj.find(tmp) != adj.end())){
          return false; // 只要有一个邻居不安全，就不能合并
      }
    }
  }else if(type==riscv_float32){

  }
}