#include "../include/MyBackend/RISCVContext.hpp"
#include <memory>

// Deal RetInst 
// 对每一条指令进行一个匹配，再对指令进行一个细致的匹配
// 寄存器分配可以先分配虚拟寄存器，之后使用寄存器分配算法
// 将虚拟寄存器转化为实际寄存器去接受


//现在为止，float，数组类型都没有进行处理

RISCVInst* RISCVContext::CreateInstAndBuildBind(RISCVInst::ISA op, Instruction *inst)
{
    auto RISCVInst = new class RISCVInst(op); // RISCVInst::ISA::_ret
    (*this)(inst, RISCVInst);

    return RISCVInst;
    // 这两种写法是等效的  this->operator()(inst,RCInst);  operator()(inst,RCInst);
}

void RISCVContext::extraDealLoadInst(RISCVInst* RISCVinst,LoadInst* inst)
{
    Value* val = inst->GetOperand(0);
    AllocaInst* allocInst = dynamic_cast<AllocaInst*> (val);
    assert(allocInst && "alllocInst must not be nullptr");
    auto& loadRecord = curMfunc->getLoadRecord();
    loadRecord[RISCVinst] =  allocInst;
    curMfunc->getLoadInsts().push_back(RISCVinst);
}

// 凡是 涉及 _sw, _sd, 的语句被相应的函数记录一下
void RISCVContext::extraDealStoreInst(RISCVInst* RISCVinst,StoreInst* inst)
{
    RISCVFunction* tmp = nullptr;
    Value *val = inst->GetOperand(1);
    auto& storeRecord = curMfunc->getStoreRecord();
    AllocaInst *allocInst = dynamic_cast<AllocaInst *>(val);
    assert(allocInst && "must not be nullptr");

    if(storeRecord.find(allocInst) == storeRecord.end())
        storeRecord[allocInst] = RISCVinst;
    
    curMfunc->RecordStackMalloc(RISCVinst,allocInst);
}

RISCVInst *RISCVContext::CreateRInst(RetInst *inst)
{
    if(inst->GetUserUseList().empty() || inst->GetOperand(0)->IsUndefVal())
    {
        return CreateInstAndBuildBind(RISCVInst::_ret,inst);
    }   // ret i32 0   inst ---> user ----> vals
    else if(inst->GetOperand(0) && inst->GetOperand(0)->GetType() == IntType::NewIntTypeGet())
    {   //  ret i32 0;  ->    li a0, 0    ret
        Value* op = inst->GetOperand(0);
        RISCVInst* Inst0 = nullptr;
        if(dynamic_cast<LoadInst*> (op) || dynamic_cast<FP2SIInst*>(op) || dynamic_cast<SI2FPInst*> (op)) {
            Inst0 = CreateInstAndBuildBind(RISCVInst::_mv,inst);
            auto LoadInst = Inst0->GetPrevNode();
            auto newOp = LoadInst->getOpreand(0); 
            Inst0->setRetOp(newOp);
        }else {
            Inst0 = CreateInstAndBuildBind(RISCVInst::_li,inst);
            Inst0->setRetOp(op);
        }
        auto Inst1 = CreateInstAndBuildBind(RISCVInst::_ret,inst);
        Inst0->DealMore(Inst1);
        
        return  Inst0;
    }
    else if(inst->GetOperand(0) && inst->GetOperand(0)->GetType() == FloatType::NewFloatTypeGet())
    {
        Value* op = inst->GetOperand(0);
        RISCVInst* Inst0 = nullptr; 
        if(!op->isConst())  { // const常数
            Inst0  = CreateInstAndBuildBind(RISCVInst::_fmv_s,inst);
            auto LoadInst = Inst0->GetPrevNode();
            auto newOp = LoadInst->getOpreand(0); 
            Inst0->setFRetOp(newOp);
        }else {
            Inst0  = CreateInstAndBuildBind(RISCVInst::_li,inst);
            Inst0->setRealLIOp(op);
            auto Inst1 = CreateInstAndBuildBind(RISCVInst::_fmv_w_x,inst);
            Inst1->SetRegisterOp("%." + std::to_string(Register::VirtualReg));
            Inst1->setStoreOp(Inst0);
            auto Inst2  = CreateInstAndBuildBind(RISCVInst::_fmv_s,inst);
            auto LoadInst = Inst2->GetPrevNode();
            auto newOp = LoadInst->getOpreand(0); 
            Inst2->setFRetOp(newOp);
            Inst0->DealMore(Inst1);
            Inst0->DealMore(Inst2);
        }

        auto EndInst = CreateInstAndBuildBind(RISCVInst::_ret,inst);
        Inst0->DealMore(EndInst);
        return  Inst0;
    }
    else {
        assert("can't accept condition!!!");
        return nullptr;  // 其实没有必要
    }
}

RISCVInst* RISCVContext::CreateLInst(LoadInst *inst)
{
    RISCVInst* Inst = nullptr;
    if(inst->GetType() == IntType::NewIntTypeGet()) {
        Inst = CreateInstAndBuildBind(RISCVInst::_lw,inst);
        Inst->setLoadOp();
        extraDealLoadInst(Inst,inst);
    }
    else if(inst->GetType() == FloatType::NewFloatTypeGet()) 
    {
        Inst = CreateInstAndBuildBind(RISCVInst::_flw,inst);
        Inst->setLoadOp();
        extraDealLoadInst(Inst,inst);
    }
    else 
        LOG(ERROR,"other conditions");
    return Inst;
}

// StoreInst ----> 要被翻译为 li, sw 两条语句
RISCVInst* RISCVContext::CreateSInst(StoreInst *inst)
{
    Value* val = inst->GetOperand(0);
    RISCVInst* Inst = nullptr;

    // float -> int
    if(dynamic_cast<FP2SIInst*>(inst->GetPrevNode()))
    {
        Inst = CreateInstAndBuildBind(RISCVInst::_sw, inst);
        auto test = Inst->GetPrevNode();
        Inst->setStoreOp(Inst->GetPrevNode());
        extraDealStoreInst(Inst, inst);
        return Inst;
    }
    // int -> float
    if(dynamic_cast<FP2SIInst*>(inst->GetPrevNode()))
    {
        Inst = CreateInstAndBuildBind(RISCVInst::_fsw, inst);
        auto test = Inst->GetPrevNode();
        Inst->setStoreOp(Inst->GetPrevNode());
        extraDealStoreInst(Inst, inst);
        return Inst;
    }

    if (val->GetType() == IntType::NewIntTypeGet())
    {   
        // store 语句第一个是值的情况
        if (val->isConst()){
            Inst = CreateInstAndBuildBind(RISCVInst::_li, inst);
            Inst->setVirLIOp(val);

            auto Inst2 = CreateInstAndBuildBind(RISCVInst::_sw, inst);
            Inst2->setStoreOp(Inst);
            extraDealStoreInst(Inst2, inst);
            Inst->DealMore(Inst2);
        } // store 语句第一个是寄存器的结构
        else {
            Inst = CreateInstAndBuildBind(RISCVInst::_sw, inst);
            Inst->setStoreOp(Inst->GetPrevNode());
            extraDealStoreInst(Inst, inst);
        }
    }
    else if( val->GetType() == FloatType::NewFloatTypeGet())
    {
        // 需要将浮点数进行截断和转化为 10 进制数
        // to_do ???
        // problem is that if we store virtualReg type 
        // we need't to create _fmv_w_x
        if (val->isConst()) {
            Inst = CreateInstAndBuildBind(RISCVInst::_li, inst);
            Inst->setRealLIOp(val);

            auto Inst2 = CreateInstAndBuildBind(RISCVInst::_fmv_w_x, inst);
            Inst->DealMore(Inst2);
            Inst2->setMVOp(Inst);

            auto Inst3 = CreateInstAndBuildBind(RISCVInst::_fsw, inst);
            // fsw 的处理
            Inst3->setStoreOp(Inst2);
            extraDealStoreInst(Inst3, inst);

            Inst->DealMore(Inst3);
        } else {
            Inst = CreateInstAndBuildBind(RISCVInst::_fsw, inst);
            Inst->setStoreOp(Inst->GetPrevNode());
            extraDealStoreInst(Inst, inst);
        }
    }

    assert(Inst && " StoreInst errors");
    return Inst;
}

RISCVInst* RISCVContext::CreateAInst(AllocaInst *inst)
{
    // auto type = inst->GetType()->GetSize();
    auto storeRecord = curMfunc->getStoreRecord();
    storeRecord[inst] = nullptr;
    return storeRecord[inst];
}

// problem solved
// todo Deal the problem
void RISCVContext::extraDealBrInst(RISCVInst*& RInst,RISCVInst::ISA op,Instruction* inst,
                                    Instruction* CmpInst,RISCVInst::op cmpOp2)
{
    if (inst->GetPrevNode()->GetOperand(0)->GetType() == IntType::NewIntTypeGet())
    {
        RInst = CreateInstAndBuildBind(op, inst);
        auto val = CmpInst->GetOperand(0);
        auto LoadRInst = mapTrans(val)->as<RISCVInst>();
        auto cmpOp1 = LoadRInst->getOpreand(0);
        RInst->push_back(cmpOp1);
        RInst->push_back(cmpOp2);
        auto Label = inst->GetOperand(2);
        auto bb = mapTrans(Label);
        auto ptr = std::make_shared<RISCVOp>(bb->getName());
        RInst->push_back(ptr);
    } 
    else  
        assert("failed");
}

void RISCVContext::extraDealBeqInst(RISCVInst*& RInst,RISCVInst::ISA op,Instruction* inst)
{
    RInst = CreateInstAndBuildBind(op, inst);
    RInst->setStoreOp(RInst->GetPrevNode());
    RInst->push_back(std::make_shared<RISCVOp>("zero"));
    auto Label = inst->GetOperand(2);
    auto bb = mapTrans(Label);
    auto ptr = std::make_shared<RISCVOp>(bb->getName());
    RInst->push_back(ptr);
}

// if 语句的处理
RISCVInst* RISCVContext::CreateCondInst(CondInst *inst)
{
    auto CmpInst = inst->GetPrevNode();
    RISCVInst* RInst = nullptr;
    if(CmpInst == nullptr)
        return nullptr;
    auto condition = CmpInst->GetOperand(0);
    if (condition->GetType() == IntType::NewIntTypeGet())
    {
        if (CmpInst && CmpInst->IsCmpInst())  {
            RISCVInst *RInst = mapTrans(CmpInst)->as<RISCVInst>();
            auto cmpOp2 = RInst->getOpreand(0);
            // Instruction
            switch (CmpInst->GetInstId())
            {
            case Instruction::Eq:
                extraDealBrInst(RInst, RISCVInst::_bne, inst, CmpInst, cmpOp2);
                break;
            case Instruction::Ne:
                extraDealBrInst(RInst, RISCVInst::_bqe, inst, CmpInst, cmpOp2);
                break;
            case Instruction::Ge:
                extraDealBrInst(RInst, RISCVInst::_blt, inst, CmpInst, cmpOp2);
                break;
            case Instruction::L:
                extraDealBrInst(RInst, RISCVInst::_bge, inst, CmpInst, cmpOp2);
                break;
            case Instruction::Le:
                extraDealBrInst(RInst, RISCVInst::_bgt, inst, CmpInst, cmpOp2);
                break;
            case Instruction::G:
                extraDealBrInst(RInst, RISCVInst::_ble, inst, CmpInst, cmpOp2);
                break;
            default:
                break;
            }
        }
        else
            assert("other conditions???");
    } else if(condition->GetType() == FloatType::NewFloatTypeGet()) {
            extraDealBeqInst(RInst,RISCVInst::_beq,inst);
    }
    else {
        assert("may be other conditions");
    }

    return RInst;
}

RISCVInst* RISCVContext::CreateUCInst(UnCondInst *inst)
{
    auto RInst = CreateInstAndBuildBind(RISCVInst::_j,inst);
    auto Label = inst->GetOperand(0);
    auto bb = mapTrans(Label);
    auto ptr = std::make_shared<RISCVOp> (bb->getName());
    RInst->push_back(ptr);
    return RInst;
}

void RISCVContext::extraDealBinary(RISCVInst* & RInst,BinaryInst* inst, RISCVInst::ISA Op)
{
    Value *valOp1 = inst->GetOperand(0);
    Value *valOp2 = inst->GetOperand(1);
    if (inst->GetType() == IntType::NewIntTypeGet())
    {
        RInst = CreateInstAndBuildBind(Op, inst);
        auto RISCVop1 = mapTrans(valOp1)->as<RISCVInst>();
        auto RISCVop2 = mapTrans(valOp2)->as<RISCVInst>();

        RInst->setThreeRigs(RISCVop1->getOpreand(0), RISCVop2->getOpreand(0));
    }
    else if (inst->GetType() == FloatType::NewFloatTypeGet())
    {
        RInst = CreateInstAndBuildBind(Op, inst);
        auto RISCVop1 = mapTrans(valOp1)->as<RISCVInst>();
        auto RISCVop2 = mapTrans(valOp2)->as<RISCVInst>();

        RInst->setThreeRigs(RISCVop1->getOpreand(0), RISCVop2->getOpreand(0));
    }
    else {
        std::cout << Op << std::endl;
        assert("Op_Add || Op_Sub || ... failed");
    }
}

void RISCVContext::extraDealFlCmp(RISCVInst* & RInst,BinaryInst* inst, RISCVInst::ISA Op,op op1,op op2)
{
    RInst = CreateInstAndBuildBind(Op, inst);
    RInst->SetRegisterOp("%." + std::to_string(Register::VirtualReg));
    RInst->push_back(op1);
    RInst->push_back(op2);
}

void RISCVContext::extraDealCmp(RISCVInst* & RInst,BinaryInst* inst, RISCVInst::ISA Op)
{
    Value* valOp1 = inst->GetOperand(0);
    Value* valOp2 = inst->GetOperand(1);
    if (valOp2->GetType() == IntType::NewIntTypeGet())  // 对整数的处理 int 
    {
        RInst = CreateInstAndBuildBind(Op, inst);
        RInst->setVirLIOp(valOp2);
    }
    else if (valOp2->GetType() == FloatType::NewFloatTypeGet())  // need to deal this logic
    {
        RISCVInst *tmp1 = mapTrans(valOp1)->as<RISCVInst>();
        RISCVInst *tmp2 = mapTrans(valOp2)->as<RISCVInst>();
        // deal tmp1 and tmp2
        if (tmp1 == nullptr)
        {
            auto Inst = CreateInstAndBuildBind(RISCVInst::_li, inst);
            Inst->setRealLIOp(valOp1);
            auto Inst2 = CreateInstAndBuildBind(RISCVInst::_fmv_w_x, inst);
            Inst->DealMore(Inst2);
            Inst2->setMVOp(Inst);
            valToRiscvOp[valOp1] = Inst;
            tmp1 = Inst2;
        }
        if (tmp2 == nullptr)
        {
            auto Inst = CreateInstAndBuildBind(RISCVInst::_li, inst);
            Inst->setRealLIOp(valOp2);
            auto Inst2 = CreateInstAndBuildBind(RISCVInst::_fmv_w_x, inst);
            Inst->DealMore(Inst2);
            Inst2->setMVOp(Inst);
            valToRiscvOp[valOp2] = Inst;
            tmp2 = Inst2;
        }
        switch (inst->GetOp())
        {
        case BinaryInst::Op_G:
            extraDealFlCmp(RInst, inst, RISCVInst::_flt_s,tmp1->getOpreand(0),tmp2->getOpreand(0));
            break;
        case BinaryInst::Op_GE:
            extraDealFlCmp(RInst, inst, RISCVInst::_fle_s,tmp1->getOpreand(0),tmp2->getOpreand(0));
            break;
        case BinaryInst::Op_L:
            extraDealFlCmp(RInst,inst,RISCVInst::_flt_s,tmp2->getOpreand(0),tmp1->getOpreand(0));
            break;
        case BinaryInst::Op_LE:
            extraDealFlCmp(RInst,inst,RISCVInst::_fle_s,tmp2->getOpreand(0),tmp1->getOpreand(0));
            break;
        case BinaryInst::Op_E:
            extraDealFlCmp(RInst,inst,RISCVInst::_feq_s,tmp1->getOpreand(0),tmp2->getOpreand(0));
            break;        
        case BinaryInst::Op_NE: {
            extraDealFlCmp(RInst,inst,RISCVInst::_feq_s,tmp1->getOpreand(0),tmp2->getOpreand(0));
            auto Inst1 = CreateInstAndBuildBind(RISCVInst::_seqz, inst);
            Inst1->SetRegisterOp("%." + std::to_string(Register::VirtualReg));
            Inst1->setStoreOp(RInst);
            break;
        }
        default:
            break;
        }
    }
    else {
        std::cout << Op << std::endl;
        assert("Op_Add || Op_Sub || ... failed");
    }
}

// 先处理整数
RISCVInst* RISCVContext::CreateBInst(BinaryInst *inst)
{
    Value* valOp1 = inst->GetOperand(0);
    Value* valOp2 = inst->GetOperand(1);
    RISCVInst* RInst = nullptr;
    switch(inst->GetOp())
    {
        case BinaryInst::Op_Add:
            if(inst->GetType() == IntType::NewIntTypeGet())
                extraDealBinary(RInst,inst,RISCVInst::_addw);
            else if(inst->GetType() == FloatType::NewFloatTypeGet())
                extraDealBinary(RInst,inst,RISCVInst::_fadd_s);
            else { assert("other conditions"); }
            break;
        case BinaryInst::Op_Sub:
            if(inst->GetType() == IntType::NewIntTypeGet())
                extraDealBinary(RInst,inst,RISCVInst::_subw);
            else if(inst->GetType() == FloatType::NewFloatTypeGet())
                extraDealBinary(RInst,inst,RISCVInst::_fsub_s);
            else { assert("other conditions"); }
            break;
        case BinaryInst::Op_Mul:
            if(inst->GetType() == IntType::NewIntTypeGet())
                extraDealBinary(RInst,inst,RISCVInst::_mulw);
            else if(inst->GetType() == FloatType::NewFloatTypeGet())
                extraDealBinary(RInst,inst,RISCVInst::_fmul_s);
            else { assert("other conditions"); }
            break;
        case BinaryInst::Op_Div:
            if(inst->GetType() == IntType::NewIntTypeGet())
                extraDealBinary(RInst,inst,RISCVInst::_divw);
            else if(inst->GetType() == FloatType::NewFloatTypeGet())
                extraDealBinary(RInst,inst,RISCVInst::_fdiv_s);
            else { assert("other conditions"); }
            break;
        case BinaryInst::Op_Mod:
            extraDealBinary(RInst,inst,RISCVInst::_remw);
            break;
        // 比较在这里不分区别，在 UnCond 的时候才有分明
        case BinaryInst::Op_L:
        case BinaryInst::Op_G: 
        case BinaryInst::Op_LE:
        case BinaryInst::Op_GE:
        case BinaryInst::Op_E:
        case BinaryInst::Op_NE:
            if (valOp1->GetType() == IntType::NewIntTypeGet())
                extraDealCmp(RInst, inst);
            else if (valOp1->GetType() == FloatType::NewFloatTypeGet())
                extraDealCmp(RInst, inst);
            else{
                assert("other conditions");
            }
            break;
        case BinaryInst::Op_And:
        case BinaryInst::Op_Or:
        case BinaryInst::Op_Xor:
            break;
        default:
            break;
    }

    return RInst;
}

RISCVInst*RISCVContext::CreateF2IInst(FP2SIInst *inst)
{
    RISCVInst* Inst = nullptr;
    auto val = inst->GetOperand(0);
    if (val->isConst()){
        Inst = CreateInstAndBuildBind(RISCVInst::_li, inst);
        Inst->setRealLIOp(val);

        auto Inst2 = CreateInstAndBuildBind(RISCVInst::_fmv_w_x, inst);
        Inst->DealMore(Inst2);
        Inst2->setMVOp(Inst);

        auto Inst3 = CreateInstAndBuildBind(RISCVInst::_fcvt_w_s, inst);
        Inst->DealMore(Inst3);
        Inst3->SetRegisterOp("%." + std::to_string(Register::VirtualReg));
        Inst3->setStoreOp(Inst3->GetPrevNode());
        Inst3->push_back(std::make_shared<RISCVOp>("rtz"));
    } else {
        Inst = CreateInstAndBuildBind(RISCVInst::_fcvt_w_s, inst);
        Inst->SetRegisterOp("%." + std::to_string(Register::VirtualReg));
        Inst->setStoreOp(Inst->GetPrevNode());
        Inst->push_back(std::make_shared<RISCVOp>("rtz"));
    }

    return Inst;
}

RISCVInst* RISCVContext::CreateI2Fnst(SI2FPInst *inst)
{
    RISCVInst* Inst = nullptr;
    auto val = inst->GetOperand(0);

    if(val->isConst()) {
        Inst = CreateInstAndBuildBind(RISCVInst::_li, inst);
        Inst->setRealLIOp(val);

        auto Inst2 = CreateInstAndBuildBind(RISCVInst::_fcvt_s_w, inst);
        Inst2->SetRegisterOp("%." + std::to_string(Register::VirtualReg));
        Inst2->setStoreOp(Inst2->GetPrevNode());
        Inst->DealMore(Inst2);
    } else {
        Inst = CreateInstAndBuildBind(RISCVInst::_fcvt_s_w, inst);
        Inst->SetRegisterOp("%." + std::to_string(Register::VirtualReg));
        Inst->setStoreOp(Inst->GetPrevNode());
    }

    return Inst;
}

// Todo:
// 函数调用相关的语句
RISCVInst* RISCVContext::CreateCInst(CallInst *inst)
{
    return nullptr;
}

// 处理数组
RISCVInst* RISCVContext::CreateGInst(GepInst *inst)
{
    return nullptr;
}

// maybe need not to deal those!!!
RISCVInst* RISCVContext::CreateZInst(ZextInst *inst) {  return nullptr; }
RISCVInst* RISCVContext::CreateSInst(SextInst *inst)  {  return nullptr; }
RISCVInst* RISCVContext::CreateTInst(TruncInst *inst) {  return nullptr;  }
RISCVInst* RISCVContext::CreateMaxInst(MaxInst *inst)  {  return nullptr;  }
RISCVInst* RISCVContext::CreateMinInst(MinInst *inst)  {  return nullptr;  }
RISCVInst* RISCVContext::CreateSelInst(SelectInst *inst) {  return nullptr;  }


RISCVOp* RISCVContext::Create(Value* val)
{
    if(auto func = dynamic_cast<Function*>(val)){
        auto it = std::make_shared<RISCVFunction>(func,func->GetName());
        Mfuncs.emplace_back(it);
        // get 仅仅获得它的拷贝，不增加引用计数，不需要释放内存
        // 也不要用它去创建新的 share_ptr指针，会导致双重释放空间
        return it.get();
    }
    // 这个用智能指针会导致释放问题，，恶心了
    if(auto block = dynamic_cast<BasicBlock*> (val)){
        auto it = new RISCVBlock(block,".BB"+block->GetName());
        //离谱的api
        // auto  func = block->GetParent();
        auto parent = mapTrans(block->GetParent())->as<RISCVFunction> ();
        
        // 把BB父亲设置为func
        // func 插入 BB
        parent->push_back(it);
        return it;
    }

    if(auto inst = dynamic_cast<Instruction*>(val)){
        // RISCVInst inst;
        if(auto Inst = dynamic_cast<LoadInst*>(inst))  return CreateLInst(Inst);
        if(auto Inst= dynamic_cast<StoreInst*>(inst))  return CreateSInst(Inst);
        if(auto Inst= dynamic_cast<AllocaInst*>(inst))   return CreateAInst(Inst);
        if(auto Inst= dynamic_cast<CallInst*>(inst))   return CreateCInst(Inst);
        if(auto Inst= dynamic_cast<RetInst*>(inst))   return CreateRInst(Inst);
        if(auto Inst= dynamic_cast<CondInst*>(inst))   return CreateCondInst(Inst);
        if(auto Inst= dynamic_cast<UnCondInst*>(inst))   return CreateUCInst(Inst);
        if(auto Inst= dynamic_cast<BinaryInst*>(inst))   return CreateBInst(Inst);
        if(auto Inst= dynamic_cast<ZextInst*>(inst))    return CreateZInst(Inst);
        if(auto Inst= dynamic_cast<SextInst*>(inst))   return CreateSInst(Inst);
        if(auto Inst= dynamic_cast<TruncInst*>(inst))   return CreateTInst(Inst);
        if(auto Inst= dynamic_cast<MaxInst*>(inst))   return CreateMaxInst(Inst);
        if(auto Inst= dynamic_cast<MinInst*>(inst))   return CreateMinInst(Inst);
        if(auto Inst= dynamic_cast<SelectInst*>(inst))   return CreateSelInst(Inst);
        if(auto Inst= dynamic_cast<GepInst*>(inst))   return CreateGInst(Inst);
        if(auto Inst = dynamic_cast<FP2SIInst*>(inst) ) return CreateF2IInst(Inst);
        if(auto Inst = dynamic_cast<SI2FPInst*>(inst) ) return CreateI2Fnst(Inst);
        assert("have no right Inst type to match it");
    }

    return nullptr;
}

RISCVOp* RISCVContext::mapTrans(Value* val)
{
    if(valToRiscvOp.find(val) == valToRiscvOp.end()){
        valToRiscvOp[val] = Create(val);
    }

    return valToRiscvOp[val];
};
