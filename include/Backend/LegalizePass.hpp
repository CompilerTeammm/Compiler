#pragma once
#include "../../include/backend/RISCVMIR.hpp"
#include "../../include/backend/RISCVContext.hpp"
class LegalizeConstInt;
//合法化
class Legalize{
    RISCVLoweringContext& ctx;
    public:
    Legalize(RISCVLoweringContext&);
    void LegalizePass(mylist<RISCVBasicBlock,RISCVMIR>::iterator);//对单条指令进行合法化
    void LegalizePass_before(mylist<RISCVBasicBlock, RISCVMIR>::iterator);//在寄存器分配之前执行合法化
    void LegalizePass_after(mylist<RISCVBasicBlock, RISCVMIR>::iterator);//之后进行合法化
    void StackAndFrameLegalize(int,mylist<RISCVBasicBlock,RISCVMIR>::iterator&);//处理涉及SP和FP的指令，如栈帧分配回收等
    void OffsetLegalize(int,mylist<RISCVBasicBlock,RISCVMIR>::iterator&);//处理偏移调整
    void zeroLegalize(int,mylist<RISCVBasicBlock,RISCVMIR>::iterator&);//针对0这个立即数的特殊处理
    void branchLegalize(int,mylist<RISCVBasicBlock,RISCVMIR>::iterator&);//处理分支指令的立即数
    void noImminstLegalize(int,mylist<RISCVBasicBlock,RISCVMIR>::iterator&);//处理没有立即数版本的指令
    void constintLegalize(int,mylist<RISCVBasicBlock,RISCVMIR>::iterator&);//处理大立即数
    void MOpcodeLegalize(RISCVMIR*);//处理RISCVMIR指令的合法化
    bool isImminst(RISCVMIR::RISCVISA);//是否是立即数指令？
    void run();
    void run_beforeRA();
    void run_afterRA();
};