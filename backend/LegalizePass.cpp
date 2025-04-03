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
                    case RISCVMIR::LoadImmReg:{
                        //reg = LoadImmReg imm
                        //如果是整数：li reg, imm
                        //li t0, imm_as_int  # 先用 li 加载 IEEE 754 表示的整数
                        //fmv.w.x reg, t0    # 再转换成浮点寄存器
                        auto imm=inst->GetOperand(0)->as<Imm>();
                        auto isfloatconst=imm->GetType()==RISCVType::riscv_float32;
                        if(isfloatconst){
                            auto fval=imm->Getdata()->as<ConstIRFloat>()->GetVal();
                            int initval;
                            std::memcpy(&initval,&fval,sizeof(float));
                            //fval是浮点值
                            //将浮点数按位模式拷贝到整数变量 initval。
                            //目的是让 initval 具有 fval 的 IEEE 754 表示，这样 li 指令就能正确加载该浮点数的二进制表示。
                            auto liinst=new RISCVMIR(RISCVMIR::li);
                            liinst->SetDef(PhyRegister::GetPhyReg(PhyRegister::t0));
                            liinst->AddOperand(IMm::GetImm(ConstIRInt::GetNewConstant(initval)));

                            auto fmvwx=new RISCVMIR(RISCVMIR::_fmv_w_x);
                            fmvwx->SetDef(inst->GetDef());
                            fmvwx->AddOperand(PhyRegister::GetPhyReg(PhyRegister::t0));

                            it=mylist<RISCVBasicBlock,RISCVMIR>::iterator(inst);
                            it.insert_before(liinst);
                            it.insert_before(fmvwx);
                            delete inst;
                            it=mylist<RISCVBasicBlock,RISCVMIR>::iterator(fmvwx);
                            break;
                        }else{
                            inst->SetMopcode(RISCV::li);
                        }
                    }
                    default:
                     break;
                }
                ++it;
            }
        }
    }
    {
        int legalizetime = 2, time = 0;
        while(time < legalizetime) {
            RISCVFunction* func = ctx.GetCurFunction();
            for(auto block : *(func)) {
                for(mylist<RISCVBasicBlock, RISCVMIR>::iterator it=block->begin(); it!=block->end(); ++it) {
                    LegalizePass(it);
                } 
            }
            for(mylist<RISCVBasicBlock, RISCVMIR>::iterator it=func->GetExit()->begin(); it!=func->GetExit()->end(); ++it) {
                LegalizePass(it);
            }
            time++;
        }
    }
}