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
void Legalize::run_beforeRA() {
    int legalizetime=2, time = 0;
    while(time < legalizetime) {
        RISCVFunction* func = ctx.GetCurFunction();
        for(auto block : *(func)) {
            for(mylist<RISCVBasicBlock, RISCVMIR>::iterator it=block->begin(); it!=block->end(); ++it) {
                LegalizePass_before(it);
            } 
        }
        for(mylist<RISCVBasicBlock, RISCVMIR>::iterator it=func->GetExit()->begin(); it!=func->GetExit()->end(); ++it) {
            LegalizePass_before(it);
        }
        time++;
    }
}
void Legalize::run_afterRA() {
    int legalizetime=2, time = 0;
    while(time < legalizetime) {
        RISCVFunction* func = ctx.GetCurFunction();
        for(auto block : *(func)) {
            for(mylist<RISCVBasicBlock, RISCVMIR>::iterator it=block->begin(); it!=block->end(); ++it) {
                LegalizePass_after(it);
            } 
        }
        for(mylist<RISCVBasicBlock, RISCVMIR>::iterator it=func->GetExit()->begin(); it!=func->GetExit()->end(); ++it) {
            LegalizePass_after(it);
        }
        time++;
    }
}
//检查每个操作数是否需要特殊处理，栈寄存器，栈帧对象，立即数，进行对应的合法化
void Legalize::LegalizePass(mylist<RISCVBasicBlock,RISCVMIR>::iterator it){
    using PhyReg=PhyRegister::PhyReg;
    using ISA=RISCVMIR::RISCVISA;
    RISCVMIR* inst=*it;
    ISA opcode=inst->GetOpcode();

    if(opcode==ISA::call||opcode==ISA::ret){
        return;
    }
    for(int i=0;i<inst->GetOperandSize();i++){
        RISCVMOperand* oprand=inst->GetOperand(i);
        // StackReg and Frameobj out memory inst
        RISCVFrameObject* framobj = dynamic_cast<RISCVFrameObject*>(oprand);
        StackRegister* sreg = dynamic_cast<StackRegister*>(oprand);
        if(frameobj||sreg){
            if(i==0&&((opcode>ISA::BeginLoadMem&&opcode<ISA::EndLoadMem)||(opcode>ISA::BeginFloatLoadMem&&opcode<ISA::EndFloatLoadMem))) {
                OffsetLegalize(i, it);
            }else if(i==1&&((opcode>ISA::BeginStoreMem&&opcode<ISA::EndStoreMem)||(opcode>ISA::BeginFloatStoreMem&&opcode<ISA::EndFloatStoreMem))){
                OffsetLegalize(i,it);
            }
            else{
                StackAndFrameLegalize(i,it);
            }
        }
        if(Imm* constdata=dynamic_cast<Imm*>(inst->GetOperand(i))){
            // 整数立即数
            if(ConstIRInt* constint = dynamic_cast<ConstIRInt*>(constdata->Getdata())) {
                //0
                if(constdata->Getdata()->isZero() && !isImminst(opcode)) { 
                    zeroLegalize(i, it);
                    continue;
                }
                // 带立即数的分支指令
                if(opcode>ISA::BeginBranch && opcode<ISA::EndBranch) {
                    branchLegalize(i, it);
                    continue;
                }
                if(!isImminst(opcode)) {
                    noImminstLegalize(i, it);
                    continue;
                }
                if(opcode!=RISCVMIR::li)
                    constintLegalize(i, it);
            }
        }
    }
}
void Legalize::LegalizePass_before(mylist<RISCVBasicBlock,RISCVMIR>::iterator it){
    using PhyReg=PhyRegister::PhyReg;
    using ISA = RISCVMIR::RISCVISA;
    RISCVMIR* inst = *it;
    ISA opcode = inst->GetOpcode();
    if(opcode==ISA::call||opcode==ISA::ret){
        return;
    }
    for(int i=0;i<inst->GetOperandSize();i++){
        RISCVMOperand* oprand = inst->GetOperand(i);
        // StackReg and Frameobj out memory inst
        RISCVFrameObject* framobj = dynamic_cast<RISCVFrameObject*>(oprand);
        StackRegister* sreg = dynamic_cast<StackRegister*>(oprand);
        if(framobj||sreg) {
            StackAndFrameLegalize(i, it);
        } 
    }
}
void Legalize::LegalizePass_after(mylist<RISCVBasicBlock, RISCVMIR>::iterator it){
    using PhyReg = PhyRegister::PhyReg;
    using ISA = RISCVMIR::RISCVISA;
    RISCVMIR* inst = *it;
    ISA opcode = inst->GetOpcode();
    if(opcode==ISA::call||opcode==ISA::ret){
        return;
    }
    for(int i=0;i<inst->GetOperandSize();i++){
        RISCVMOperand* oprand = inst->GetOperand(i);
        RISCVFrameObject* framobj = dynamic_cast<RISCVFrameObject*>(oprand);
        StackRegister* sreg = dynamic_cast<StackRegister*>(oprand);
        if(frameobj||sreg){
            if(i==0&&((opcode>ISA::BeginLoadMem&&opcode<ISA::EndLoadMem)||(opcode>ISA::BeginFloatLoadMem&&opcode<ISA::EndFloatLoadMem))) {
                OffsetLegalize(i, it);
            }else if(i==1&&((opcode>ISA::BeginStoreMem&&opcode<ISA::EndStoreMem)||(opcode>ISA::BeginFloatStoreMem&&opcode<ISA::EndFloatStoreMem))){
                OffsetLegalize(i,it);
            }
        }
        if(Imm* constdata=dynamic_cast<Imm*>(inst->GetOperand(i))){
            // 整数立即数
            if(ConstIRInt* constint = dynamic_cast<ConstIRInt*>(constdata->Getdata())) {
                //0
                if(constdata->Getdata()->isZero() && !isImminst(opcode)) { 
                    zeroLegalize(i, it);
                    continue;
                }
                // 带立即数的分支指令
                if(opcode>ISA::BeginBranch && opcode<ISA::EndBranch) {
                    branchLegalize(i, it);
                    continue;
                }
                if(!isImminst(opcode)) {
                    noImminstLegalize(i, it);
                    continue;
                }
                constintLegalize(i, it);
            }
        }
    }
}
//处理栈相关操作数的合法化
//处理 StackRegister 和 RISCVFrameObject，插入 addi 指令计算栈地址
void Legalize::StackAndFrameLegalize(int i,mylist<RISCVBasicBlock,RISCVMIR>::iterator& it){
    RISCVMIR *inst=*it;
    StackRegister* sreg=nullptr;
    if(sreg = dynamic_cast<StackRegister*>(inst->GetOperand(i))) {
    }else if(RISCVFrameObject* obj = dynamic_cast<RISCVFrameObject*>(inst->GetOperand(i))) {
        sreg = dynamic_cast<RISCVFrameObject*>(obj)->GetStackReg();
    }
    RISCVMIR* addi=new RISCVMIR(RISCVMIR::_addi);
    PhyRegister* t0=PhyRegister::GetPhyReg(PhyRegister::t0);
    addi->SetDef(t0);
    addi->AddOperand(sreg->GetReg());
    Imm* immm=new Imm(ConstIRInt::GetNewConstant(sreg->GetOffset()));
    addi->AddOperand(imm);
    it.insert_before(addi);
    inst->SetOperand(i,addi->GetDef());
}
//处理超出 RISC-V 可用范围的偏移量，插入 li 和 add 指令进行计算
void Legalize::OffsetLegalize(int i,mylist<RISCVBasicBlock,RISCVMIR>::iterator& it){
    RISCVMIR* inst = *it;
    StackRegister* sreg = nullptr;
    if(sreg = dynamic_cast<StackRegister*>(inst->GetOperand(i))) {
    }else if(RISCVFrameObject* obj = dynamic_cast<RISCVFrameObject*>(inst->GetOperand(i))) {
        sreg = dynamic_cast<RISCVFrameObject*>(obj)->GetStackReg();
    }
    int offset = sreg->GetOffset();
    if(offset>=-2048&&offset<=2047) {
        return;
    }else{
        //li t0,offset
        RISCVMIR* li = new RISCVMIR(RISCVMIR::li);
        li->AddOperand(Imm::GetImm(ConstIRInt::GetNewConstant(offset)));
        li->SetDef(PhyRegister::GetPhyReg(PhyRegister::t0));

        //add t0, t0, sreg->GetReg()，即 t0 = offset + sreg->GetReg()。
        RISCVMIR* add = new RISCVMIR(RISCVMIR::_add);
        add->AddOperand(li->GetDef());
        add->AddOperand(sreg->GetReg());
        add->SetDef(PhyRegister::GetPhyReg(PhyRegister::t0));

        it.insert_before(li);
        it.insert_before(add);

        StackRegister* newStackReg = new StackRegister(PhyRegister::t0,0);
        inst->SetOperand(i, newStackReg);

    }
}

void Legalize::zeroLegalize(int i,mylist<RISCVBasicBlock,RISCVMIR>::iterator& it){
    RISCVMIR *inst=*it;
    PhyRegister *zero=PhyRegister::GetPhyReg(PhyRegister::zero);
    inst->SetOperand(i,zero);
}

void Legalize::branchLegalize(int i,mylist<RISCVBasicBlock,RISCVMIR>::iterator& it){
    RISCVMIR* inst = *it;
    RISCVMIR* li = new RISCVMIR(RISCVMIR::li);
    PhyRegister* t0 = PhyRegister::GetPhyReg(PhyRegister::t0);
    Imm* imm = dynamic_cast<Imm*>(inst->GetOperand(i));
    li->SetDef(t0);
    li->AddOperand(imm);
    it.insert_before(li);
    inst->SetOperand(i, li->GetDef());    
}
void Legalize::noImminstLegalize(int i,mylist<RISCVBasicBlock,RISCVMIR>::iterator& it){
    RISCVMIR *inst=*it;
    Imm* constdata=dynamic_cast<Imm*>(inst->GetOperand(i));
    int val=dynamic_cast<ConstIRInt*>(constdata->GetData())->GetVal();
    if(val>=-2048 && val<2048) {//mv rd, x0 == li rd, 0
        if(inst->GetOpcode() == RISCVMIR::mv) {
            inst->SetMopcode(RISCVMIR::RISCVISA::li); 
            return;
        }
    }
    RISCVMIR* li=new RISCVMIR(RISCVMIR::li);
    PhyRegister* t0 = PhyRegister::GetPhyReg(PhyRegister::t1);
    li->SetDef(t0);
    li->AddOperand(constdata);
    it.insert_before(li);
    inst->SetOperand(i, li->GetDef());
}
void Legalize::constintLegalize(int i,mylist<RISCVBasicBlock,RISCVMIR>::iterator& it){
    RISCVMIR* inst=*it;
    Imm* constdata=dynamic_cast<Imm*>(inst->GetOperand(i));
    PhyRegister* t0 = PhyRegister::GetPhyReg(PhyRegister::t1);
    int inttemp = dynamic_cast<ConstIRInt*>(constdata->Getdata())->GetVal();

    if(inttemp>=-2048&&inttemp<2048){
        return;
    }else{
        auto mir=new RISCVMIR(RISCVMIR::li);
        mir->SetDef(PhyRegister::GetPhyReg(PhyRegister::t0));
        mir->AddOperand(constdata);
        it.insert_before(mir);
        inst->SetOperand(i, PhyRegister::GetPhyReg(PhyRegister::t0));
        MOpcodeLegalize(inst);
    }
}

bool Legalize::isImminst(RISCVMIR::RISCVISA opcode){
    if(opcode == RISCVMIR::_slli ||
       opcode == RISCVMIR::_slliw ||
       opcode == RISCVMIR::_srli ||
       opcode == RISCVMIR::_srliw ||
       opcode == RISCVMIR::_srai ||
       opcode == RISCVMIR::_sraiw ||
       opcode == RISCVMIR::_addi ||
       opcode == RISCVMIR::_addiw ||
       opcode == RISCVMIR::_xori ||
       opcode == RISCVMIR::_ori ||
       opcode == RISCVMIR::_andi ||
       opcode == RISCVMIR::_slti ||
       opcode == RISCVMIR::_sltiu ||
       opcode == RISCVMIR::li) {
        return true;
       }else{
        return false;
       }
}
void Legalize::MOpcodeLegalize(RISCVMIR* inst) {
    using ISA = RISCVMIR::RISCVISA;
    ISA& opcode = inst->GetOpcode();

    if (opcode == ISA::_slli) {
        inst->SetMopcode(ISA::_sll);
    } else if (opcode == ISA::_slliw) {
        inst->SetMopcode(ISA::_sllw);
    } else if (opcode == ISA::_srli) {
        inst->SetMopcode(ISA::_srl);
    } else if (opcode == ISA::_srliw) {
        inst->SetMopcode(ISA::_srlw);
    } else if (opcode == ISA::_srai) {
        inst->SetMopcode(ISA::_sra);
    } else if (opcode == ISA::_sraiw) {
        inst->SetMopcode(ISA::_sraw);
    } else if (opcode == ISA::_addi) {
        inst->SetMopcode(ISA::_add);
    } else if (opcode == ISA::_addiw) {
        inst->SetMopcode(ISA::_addw);
    } else if (opcode == ISA::_xori) {
        inst->SetMopcode(ISA::_xor);
    } else if (opcode == ISA::_ori) {
        inst->SetMopcode(ISA::_or);
    } else if (opcode == ISA::_andi) {
        inst->SetMopcode(ISA::_and);
    } else if (opcode == ISA::_slti) {
        inst->SetMopcode(ISA::_slt);
    } else if (opcode == ISA::_sltiu) {
        inst->SetMopcode(ISA::_sltu);
    } else if (opcode == ISA::li) {
        inst->SetMopcode(ISA::mv);
    } else if (
        opcode == ISA::_sw || opcode == ISA::_sd || opcode == ISA::_sh ||opcode == ISA::_sb) {
        // memory store instructions: no need to modify MOpcode
    } else {
        assert(0 && "Invalid MOpcode type");
    }
}
