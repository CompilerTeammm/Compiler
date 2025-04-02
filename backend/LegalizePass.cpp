#include "../include/Backend/LegalizePass.hpp"
#include "../include/Backend/RISCVAsmPrinter.hpp"
#include <cstring>//提供 std::memcpy()，在某些情况下用于低级字节操作，例如浮点数与整数之间的转换。

Legalize::Legalize(RISCVLoweringContext& _ctx):ctx(_ctx){}

void Legalize::run(){
    // 将load/store指令中的物理寄存器转化为栈寄存器   
    { 
        auto func=ctx.GetCurFunction();
        for(auto block:*func){
            for(auto inst:*block){
                auto opcode=inst->GetOpcode();
                if(RISCVMIR::BeginLoadMem<opcode&&opcode<RISCVMIR::EndLoadMem||RISCVMIR::BeginFloatLoadMem<opcode&&opcode<RISCVMIR::EndFloatLoadMem){
                    if(auto preg=inst->GetOperand(0)->as<PhyRegister>()){
                        auto sreg=new StackRegister(preg->Getregenum(),0);
                        inst->SetOperand(0,sreg);
                    }
                }else if(RISCVMIR::BeginStoreMem<opcode&&opcode<RISCVMIR::EndStoreMem||RISCVMIR::BeginFloatStoreMem<opcode&&opcode<RISCVMIR::EndFloatStoreMem){
                    if(auto preg=inst->GetOperand(1)->as<PhyRegister>()){
                        auto sreg=new StackRegister(preg->Getregenum(),0);
                        inst->SetOperand(1,sreg);
                    }
                }
            }
        }
    }
    //将某些伪指令转化为标准的RISCV指令
    {
        auto func=ctx.GetCurFunction();
        for(auto block:*func){
            for(auto it=block->begin();it!=block->end();){
                auto inst=*it;
                auto opcode=inst->GetOpcode();
                switch(opcode){
                    case RISCV::LoadGlobalAddr:{
                        //lambda 表达式:是一种匿名函数，可以作为变量存储并在后续调用。
                        //使代码更简洁
                        // reg = LoadGlobalAddr globalvar
                        // lui reg, %hi(globalvar)
                        // addi reg, reg, %lo(globalvar)
                        auto getTagName=[&](){
                            if(auto glob=inst->GetOperand(0)->as<globlvar>()){
                                return glob->GetName();
                            }
                            else if(auto tag=inst->GetOperand(0)->as<OuterTag>()){
                                return tag->GetName();
                            }
                            else assert(0&&"GG in get global address");
                        };
                        // 也可以类似：
                        // std::string getTagName(RISCVMIR* inst) {
                        //     if (auto glob = inst->GetOperand(0)->as<globlvar>()) {
                        //         return glob->GetName();
                        //     }
                        //     else if (auto tag = inst->GetOperand(0)->as<OuterTag>()) {
                        //         return tag->GetName();
                        //     }
                        //     else {
                        //         assert(0 && "GG in get global address");
                        //         return "";
                        //     }
                        // }
                        auto name=getTagName();
                        auto hi=new LARegister(riscv_ptr,name,LARegister::hi);
                        auto lo=new LARegister(riscv_ptr,name,LARegister::lo);
                        auto reg=inst->GetDef()->as<Register>();
                        assert(reg!=nullptr);
                        //获取当前 LoadGlobalAddr 指令的目标寄存器 reg，这个寄存器用于存储最终的全局变量地址。
                        RISCVMIR* lui=new RISCVMIR(RISCVMIR::_lui);
                        lui->SetDef(reg);
                        lui->AddOperand(hi);

                        RISCVMIR* addi=new RISCVMIR(RISCVMIR::_addi);
                        addi->SetDef(reg);
                        addi->AddOperand(reg);
                        addi->AddOperand(lo);
                        it.insert_before(lui);
                        it.insert_before(addi);
                        //转换前
                        // a0 = LoadGlobalAddr globalvar
                        // 转换后
                        // lui  a0, %hi(globalvar)   高20位
                        // addi a0, a0, %lo(globalvar) 组合低12位
                        it=mylist<RISCVBasicBlock,RISCVMIR>::iterator(addi);
                        delete inst;
                        break;
                    }
                    case RISCVMIR::LoadLocalAddr:{
                        //addi reg, s0, offset
                        auto frameobj=inst->GetOperand(0)->as<RISCVFrameObject>();
                        auto stackreg=frameobj->GetStackReg();
                        auto reg=inst->GetDef()->as<Register>();
                        assert(reg!=nullptr);
                        RISCVMIR* addi=new RISCVMIR(RISCVMIR::_addi);
                        addi->SetDef(reg);
                        addi->AddOperand(stackreg->GetReg());
                        addi->AddOperand(Imm::GetImm(ConstIRInt::GetNewConstant(stackreg->GetOffset())));
                        //获取该局部变量在栈中的偏移量，转换成立即数并作为 addi 的第二个操作数。
                        it.insert_before(addi);
                        it=mylist<RISCVBasicBlock,RISCVMIR>::iterator(addi);
                        delete inst;
                        break;
                    }
                }
            }
        }
    }
}