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