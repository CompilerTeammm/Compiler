#include "../include/MyBackend/RISCVContext.hpp"
#include "../include/MyBackend/MIR.hpp"
#include "CFG.hpp"
#include <memory>

// Deal RetInst 
// 对每一条指令进行一个匹配，再对指令进行一个细致的匹配
// 寄存器分配可以先分配虚拟寄存器，之后使用寄存器分配算法
// 将虚拟寄存器转化为实际寄存器去接受

// 数组，全局变量未进行处理 
// call 函数未进行处理  全部支持  class RISCVInst

bool RISCVContext::dealGlobalVal(Value* val)
{
    auto text = std::make_shared<TextSegment> (val);
    addText(text);
    valToText[val] = text; // val linked with text
    return true;
}

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
    AllocaInst *allocInst = dynamic_cast<AllocaInst *>(val);
    assert(allocInst && "must not be nullptr");

    // auto& storeRecord = curMfunc->getStoreRecord();
    // if(storeRecord.find(allocInst) == storeRecord.end())
    //     storeRecord[allocInst] = RISCVinst;
    
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
            auto Inst = dynamic_cast<Instruction*> (op);
            if (Inst == nullptr) {
                Inst0 = CreateInstAndBuildBind(RISCVInst::_li,inst);
                Value* val = inst->GetOperand(0);
                Inst0->setRetOp(val);
            }else {
                Inst0 = CreateInstAndBuildBind(RISCVInst::_mv,inst);
                RISCVInst* PreInst = Inst0->GetPrevNode();
                Inst0->setRetOp(PreInst->getOpreand(0));
            }
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
            Inst1->SetVirFloatRegister();
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

// 数组（局部和全局）      全局 int,float 单独处理需要
RISCVInst* RISCVContext::CreateLInst(LoadInst *inst)
{
    RISCVInst *Inst = nullptr;

    if (dynamic_cast<GepInst *>(inst->GetOperand(0)) )
    {
        // global 的lw 应该不需要记录，因为这个全局加载
        if (inst->GetType() == IntType::NewIntTypeGet()) {
            Inst = CreateInstAndBuildBind(RISCVInst::_lw, inst);
            Inst->SetVirRegister();
            Inst->getOpsVec().push_back(Inst->GetPrevNode()->getOpreand(0));
            getCurFunction()->getRecordGepOffset().emplace(inst,0);
        } else {
            Inst = CreateInstAndBuildBind(RISCVInst::_flw, inst);
            Inst->SetVirFloatRegister();
            Inst->getOpsVec().push_back(Inst->GetPrevNode()->getOpreand(0));
            getCurFunction()->getRecordGepOffset().emplace(inst,0);
        }
    }
    else if (inst->GetOperand(0)->isGlobal())
    {
        Value *globlVal = inst->GetOperand(0);

        RISCVInst* laInst = CreateInstAndBuildBind(RISCVInst::_la, inst);
        laInst->SetVirRegister();
        laInst->SetAddrOp(globlVal);

        Inst = CreateInstAndBuildBind(RISCVInst::_lw, inst);
        Inst->SetVirRegister();
        Inst->getOpsVec().push_back(Inst->GetPrevNode()->getOpreand(0));

        getCurFunction()->getGloblValRecord().push_back(inst);
    }
    else
    {
        if (inst->GetType() == IntType::NewIntTypeGet())
        {
            Inst = CreateInstAndBuildBind(RISCVInst::_lw, inst);
            Inst->SetVirRegister();
            extraDealLoadInst(Inst, inst);
        }
        else if (inst->GetType() == FloatType::NewFloatTypeGet())
        {
            Inst = CreateInstAndBuildBind(RISCVInst::_flw, inst);
            Inst->SetVirFloatRegister();
            extraDealLoadInst(Inst, inst);
        } 
        else {  
            // 指针的处理     
            Inst = CreateInstAndBuildBind(RISCVInst::_ld, inst);
            Inst->SetVirRegister();
            extraDealLoadInst(Inst, inst);
        }
            
    }
    return Inst;
}

// StoreInst ----> 要被翻译为 li, sw 两条语句
// 如果是 参数的store  ----> 要被翻译为 mv，sw 两天语句
RISCVInst* RISCVContext::CreateSInst(StoreInst *inst)
{
    Value* val = inst->GetOperand(0);
    RISCVInst* Inst = nullptr;
    if (dynamic_cast<GepInst*>(inst->GetOperand(1)) ) {
        // Value* Gval = inst->GetOperand(1);
        // TextPtr it = valToText[Gval];
        if (dynamic_cast<Instruction*>(val) == nullptr) {
            RISCVInst *liInst = CreateInstAndBuildBind(RISCVInst::_li, inst);
            liInst->SetVirRegister();
            liInst->SetImmOp(val);
            Inst = CreateInstAndBuildBind(RISCVInst::_sw, inst);
            Inst->push_back(liInst->getOpreand(0));
            if(liInst->GetPrevNode() != nullptr)
                Inst->push_back(liInst->GetPrevNode()->getOpreand(0));
            getCurFunction()->getGloblValRecord().push_back(inst);
        } else {
            if (inst->GetOperand(0)->GetType() == IntType::NewIntTypeGet()) 
            {
                Inst = CreateInstAndBuildBind(RISCVInst::_sw, inst);
                auto it = mapTrans(inst->GetOperand(0))->as<RISCVInst>();
                Inst->push_back(it->getOpreand(0));
                Inst->push_back(Inst->GetPrevNode()->getOpreand(0));
                getCurFunction()->getGloblValRecord().push_back(inst);
            } else {
                auto val = inst->GetOperand(1);
                auto riscvInst = mapTrans(val)->as<RISCVInst>();
                Inst = CreateInstAndBuildBind(RISCVInst::_fsw, inst);
                auto it = mapTrans(inst->GetOperand(0))->as<RISCVInst>();
                Inst->push_back(it->getOpreand(0));
                if (riscvInst == nullptr)
                    Inst->push_back(Inst->GetPrevNode()->getOpreand(0));
                else {
                    Inst->push_back(riscvInst->getOpreand(0));
                }
                getCurFunction()->getGloblValRecord().push_back(inst);
            }

            // extraDealStoreInst(Inst, inst);
        }
        return Inst;
    } else if (dynamic_cast<LoadInst*>(val) && 
              dynamic_cast<GepInst*>(val->as<LoadInst>()->GetOperand(0))) {
        Inst = CreateInstAndBuildBind(RISCVInst::_sw,inst);
        Inst->push_back(Inst->GetPrevNode()->getOpreand(0));
        extraDealStoreInst(Inst, inst);
        return Inst;
    }

    if (inst->GetOperand(1)->isGlobal())
    {
        Value* globlVal = inst->GetOperand(1);
        if(dynamic_cast<ConstantData*> (val)) {

            RISCVInst *laInst = CreateInstAndBuildBind(RISCVInst::_la, inst);
            laInst->SetVirRegister();
            laInst->SetAddrOp(globlVal);

            RISCVInst* liInst = CreateInstAndBuildBind(RISCVInst::_li, inst);
            liInst->SetVirRegister();
            liInst->SetImmOp(val);

            Inst = CreateInstAndBuildBind(RISCVInst::_sw, inst);
            Inst->push_back(liInst->getOpreand(0));
            Inst->push_back(liInst->GetPrevNode()->getOpreand(0));

            getCurFunction()->getGloblValRecord().push_back(inst);

        } else {

            RISCVInst *laInst = CreateInstAndBuildBind(RISCVInst::_la, inst);
            laInst->SetVirRegister();
            laInst->SetAddrOp(globlVal);

            Inst = CreateInstAndBuildBind(RISCVInst::_sw, inst);
            auto it = mapTrans(inst->GetOperand(0))->as<RISCVInst>();
            if (dynamic_cast<CallInst*>(inst->GetOperand(0))){
                Inst->SetRealRegister("a0");
                Inst->push_back(Inst->GetPrevNode()->getOpreand(0));
            }
            else {
                Inst->push_back(it->getOpreand(0));
                Inst->push_back(Inst->GetPrevNode()->getOpreand(0));
            }
            getCurFunction()->getGloblValRecord().push_back(inst);
        }
        return Inst;
    }

    if (auto var = dynamic_cast<Var*>(val))
    {
        if(var->isParam())
        {
            RISCVInst* SwInst = nullptr;
            if (inst->GetOperand(1)->GetType()->GetLayer() > 1)
            {
                auto it = mapTrans(inst->GetOperand(0))->as<RealRegister>();
                if (it != nullptr) {
                    SwInst = CreateInstAndBuildBind(RISCVInst::_sd, inst);
                    auto Rop = std::make_shared<RealRegister>(it->getName());
                    SwInst->push_back(Rop);
                    extraDealStoreInst(SwInst, inst);          
                } else {
                    RISCVInst* ldInst = CreateInstAndBuildBind(RISCVInst::_ld,inst);
                    ldInst->SetVirRegister();

                    // 栈帧部分进行遍历就可以赋值了
                    getCurFunction()->getSpilledParamLoadInst().push_back(ldInst);
                    SwInst = CreateInstAndBuildBind(RISCVInst::_sd, inst);
                    SwInst->push_back(ldInst->getOpreand(0));
                    extraDealStoreInst(SwInst, inst);
                }   
            }
            else {
                auto it = mapTrans(inst->GetOperand(0))->as<RealRegister>();
                if (inst->GetOperand(0)->GetType() == FloatType::NewFloatTypeGet())
                {
                    if (it != nullptr)
                    { //  fa0-fa7 的参数
                        SwInst = CreateInstAndBuildBind(RISCVInst::_fsw, inst);
                        auto Rop = std::make_shared<RealRegister>(it->getName());
                        SwInst->push_back(Rop);
                        extraDealStoreInst(SwInst, inst);
                    }
                    else
                    { //  栈帧中的  这里绝对需要再处理 ld 的逻辑，切记
                        // I think the order is right, don't need me to change
                        RISCVInst *lwInst = CreateInstAndBuildBind(RISCVInst::_flw, inst);
                        lwInst->SetVirRegister();
                        // 栈帧部分进行遍历就可以赋值了
                        getCurFunction()->getSpilledParamLoadInst().push_back(lwInst);
                        SwInst = CreateInstAndBuildBind(RISCVInst::_fsw, inst);
                        SwInst->push_back(lwInst->getOpreand(0));
                        extraDealStoreInst(SwInst, inst);
                    }
                } else {
                    if (it != nullptr)
                    { //  a0-a7 的参数
                        SwInst = CreateInstAndBuildBind(RISCVInst::_sw, inst);
                        auto Rop = std::make_shared<RealRegister>(it->getName());
                        SwInst->push_back(Rop);
                        extraDealStoreInst(SwInst, inst);
                    }
                    else
                    { //  栈帧中的  这里绝对需要再处理 ld 的逻辑，切记
                        // I think the order is right, don't need me to change
                        RISCVInst *lwInst = CreateInstAndBuildBind(RISCVInst::_lw, inst);
                        lwInst->SetVirRegister();
                        // 栈帧部分进行遍历就可以赋值了
                        getCurFunction()->getSpilledParamLoadInst().push_back(lwInst);
                        SwInst = CreateInstAndBuildBind(RISCVInst::_sw, inst);
                        SwInst->push_back(lwInst->getOpreand(0));
                        extraDealStoreInst(SwInst, inst);
                    }
                }
            }
            return SwInst;
        }
    }

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
            Inst2->SetVirFloatRegister();
            Inst2->setOpWInstFOpread(Inst);

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
    curMfunc->getAllocas().push_back(inst);
    // auto storeRecord = curMfunc->getStoreRecord();
    // storeRecord[inst] = nullptr;
    // return storeRecord[inst];
    return nullptr;
}

// problem solved
// todo Deal the problem
void RISCVContext::extraDealBrInst(RISCVInst*& RInst,RISCVInst::ISA op,Instruction* inst,
                                    Instruction* CmpInst)
{
    // RISCVFunction
    if (inst->GetPrevNode()->GetOperand(0)->GetType() == IntType::NewIntTypeGet())
    {
        RInst = CreateInstAndBuildBind(op, inst);
        auto val = CmpInst->GetOperand(0);
        auto val2 = CmpInst->GetOperand(1);
        auto LoadRInst = mapTrans(val)->as<RISCVInst>();
        auto val2Inst = mapTrans(val2)->as<RISCVInst>();
        // std::shared_ptr<RISCVOp> cmpOp1 = RInst->GetPrevNode()->GetPrevNode()->getOpreand(0);
        // std::shared_ptr<RISCVOp> cmpOp2 = RInst->GetPrevNode()->getOpreand(0);
        std::shared_ptr<RISCVOp> cmpOp1 = nullptr;
        std::shared_ptr<RISCVOp> cmpOp2 = nullptr;
        if (auto it = dynamic_cast<ConstantData*> (val))
        {
            cmpOp1 = RInst->GetPrevNode()->GetPrevNode()->getOpreand(0);
            //cmpOp1 = Imm::GetImm(it);
        }

        if (auto it = dynamic_cast<ConstantData*> (val2))
        {
            cmpOp2 = RInst->GetPrevNode()->getOpreand(0);
            //cmpOp2 = Imm::GetImm(it);
        }

        if (LoadRInst != nullptr )
        {
            cmpOp1 = LoadRInst->getOpreand(0);
        }
        if(val2Inst != nullptr)
        {
            cmpOp2 = val2Inst->getOpreand(0);
        }

        RInst->push_back(cmpOp1);
        RInst->push_back(cmpOp2);

        auto Label = inst->GetOperand(2);
        auto bb = mapTrans(Label);
        auto ptr = std::make_shared<RISCVOp>(bb->getName());
        RInst->push_back(ptr);

        auto JumpLabel = inst->GetOperand(1);
        auto Jumpbb = mapTrans(JumpLabel);
        auto bbptr = std::make_shared<RISCVOp>(Jumpbb->getName());
        auto JumpInst = CreateInstAndBuildBind(RISCVInst::_j,inst);
        JumpInst->push_back(bbptr);


        // chageOrder 函数
        getCurFunction()->getLabelInsts().push_back(RInst);
        BasicBlock* succbbI = inst->GetOperand(1)->as<BasicBlock>();
        BasicBlock* succbbII = inst->GetOperand(2)->as<BasicBlock>();
        auto& vec = getCurFunction()->getBrInstSuccBBs();
        vec.emplace_back(inst,std::make_pair(succbbI,succbbII));
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
            // RISCVInst *RInst = mapTrans(CmpInst)->as<RISCVInst>();
            // if (RInst == nullptr)
            //     auto cmpOp2 = RInst->getOpreand(0);
            // Instruction
            switch (CmpInst->GetInstId())
            {
// beq =    bge >=      bgt >      ble <=      blt <      bne !=
            case Instruction::Eq:
                extraDealBrInst(RInst,RISCVInst::_bne, inst, CmpInst);
                break;
            case Instruction::Ne:
                extraDealBrInst(RInst,RISCVInst::_beq, inst, CmpInst);
                break;
            case Instruction::Ge:
                extraDealBrInst(RInst,RISCVInst::_blt, inst, CmpInst);
                break;
            case Instruction::L:
                extraDealBrInst(RInst,RISCVInst::_bge, inst, CmpInst);
                break;
            case Instruction::Le:
                extraDealBrInst(RInst,RISCVInst::_bgt, inst, CmpInst);
                break;
            case Instruction::G:
                extraDealBrInst(RInst,RISCVInst::_ble, inst, CmpInst);
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

void RISCVContext::extraDealIntBinary(RISCVInst* & RInst,BinaryInst* inst, RISCVInst::ISA Op)
{
    Value *valOp1 = inst->GetOperand(0);
    Value *valOp2 = inst->GetOperand(1);

    auto Immop1 = dynamic_cast<ConstantData*> (valOp1);
    auto Immop2 = dynamic_cast<ConstantData*> (valOp2);
    RISCVInst* ImmOneInst = nullptr;
    RISCVInst* ImmTwoInst = nullptr;

    auto RISCVop1 = mapTrans(valOp1)->as<RISCVInst>();
    auto RISCVop2 = mapTrans(valOp2)->as<RISCVInst>();

    if (Immop1 != nullptr) {
        ImmOneInst = CreateInstAndBuildBind(RISCVInst::_li,inst);
        ImmOneInst->SetVirRegister();
        ImmOneInst->SetImmOp(Immop1);
    }

    if (Immop2 != nullptr) {
        ImmTwoInst = CreateInstAndBuildBind(RISCVInst::_li,inst);
        ImmTwoInst->SetVirRegister();
        ImmTwoInst->SetImmOp(Immop2);
    }
    RInst = CreateInstAndBuildBind(Op, inst);
    if(ImmOneInst != nullptr) {
        if (ImmTwoInst != nullptr) {
            RInst->setThreeRegs(ImmOneInst->getOpreand(0), ImmTwoInst->getOpreand(0));
        } else {
            RInst->setThreeRegs(ImmOneInst->getOpreand(0), RISCVop2->getOpreand(0));
        }
    } else {  // RISCVop1 != nullptr
        if(ImmTwoInst != nullptr){
            RInst->setThreeRegs(RISCVop1->getOpreand(0),ImmTwoInst->getOpreand(0));
        } else {
            RInst->setThreeRegs(RISCVop1->getOpreand(0), RISCVop2->getOpreand(0));
        }
    }
}

void RISCVContext::extraDealFloatBinary(RISCVInst* & RInst,BinaryInst* inst, RISCVInst::ISA Op)
{
    Value *valOp1 = inst->GetOperand(0);
    Value *valOp2 = inst->GetOperand(1);

    auto Immop1 = dynamic_cast<ConstantData*> (valOp1);
    auto Immop2 = dynamic_cast<ConstantData*> (valOp2);
    RISCVInst* ImmOneInst = nullptr;
    RISCVInst* ImmTwoInst = nullptr;

    auto RISCVop1 = mapTrans(valOp1)->as<RISCVInst>();
    auto RISCVop2 = mapTrans(valOp2)->as<RISCVInst>();

    if (Immop1 != nullptr) {
        ImmOneInst = CreateInstAndBuildBind(RISCVInst::_li,inst);
        ImmOneInst->SetVirFloatRegister();
        ImmOneInst->SetImmOp(Immop1);
    }

    if (Immop2 != nullptr) {
        ImmTwoInst = CreateInstAndBuildBind(RISCVInst::_li,inst);
        ImmTwoInst->SetVirFloatRegister();
        ImmTwoInst->SetImmOp(Immop2);
    }

    RInst = CreateInstAndBuildBind(Op, inst);
    if(ImmOneInst != nullptr) {
        if (ImmTwoInst != nullptr) {
            RInst->setThreeFRegs(ImmOneInst->getOpreand(0), ImmTwoInst->getOpreand(0));
        } else {
            RInst->setThreeFRegs(ImmOneInst->getOpreand(0), RISCVop2->getOpreand(0));
        }
    } else {  // RISCVop1 != nullptr
        if(ImmTwoInst != nullptr){
            RInst->setThreeFRegs(RISCVop1->getOpreand(0),ImmTwoInst->getOpreand(0));
        } else {
            RInst->setThreeFRegs(RISCVop1->getOpreand(0), RISCVop2->getOpreand(0));
        }
    }
}

void RISCVContext::extraDealFlCmp(RISCVInst* & RInst,BinaryInst* inst, RISCVInst::ISA Op,op op1,op op2)
{
    RInst = CreateInstAndBuildBind(Op, inst);
    RInst->SetVirRegister();
    RInst->push_back(op1);
    RInst->push_back(op2);
}

void RISCVContext::extraDealCmp(RISCVInst* & RInst,BinaryInst* inst, RISCVInst::ISA Op)
{
    Value* valOp1 = inst->GetOperand(0);
    Value* valOp2 = inst->GetOperand(1);
    if (valOp2->GetType() == IntType::NewIntTypeGet())  // 对整数的处理 int 
    {
        if (dynamic_cast<ConstantData*>(valOp1))
        {
            auto Inst = CreateInstAndBuildBind(RISCVInst::_li, inst);
            Inst->SetVirRegister();
            Inst->SetImmOp(valOp1);
            valToRiscvOp[valOp1] = Inst;
        }
        if (dynamic_cast<ConstantData*>(valOp2))
        {
            auto Inst = CreateInstAndBuildBind(RISCVInst::_li, inst);
            Inst->SetVirRegister();
            Inst->SetImmOp(valOp2);
            valToRiscvOp[valOp2] = Inst;
        }
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
            Inst1->SetVirRegister();
            Inst1->setStoreOp(RInst);
            break;
        }
        default:
            break;
        }
    }
    else {
        // std::cout << Op << std::endl;
        // assert("Op_Add || Op_Sub || ... failed");
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
                extraDealIntBinary(RInst,inst,RISCVInst::_addw);
            else if(inst->GetType() == FloatType::NewFloatTypeGet())
                extraDealFloatBinary(RInst,inst,RISCVInst::_fadd_s);
            else { assert("other conditions"); }
            break;
        case BinaryInst::Op_Sub:
            if(inst->GetType() == IntType::NewIntTypeGet())
                extraDealIntBinary(RInst,inst,RISCVInst::_subw);
            else if(inst->GetType() == FloatType::NewFloatTypeGet())
                extraDealFloatBinary(RInst,inst,RISCVInst::_fsub_s);
            else { assert("other conditions"); }
            break;
        case BinaryInst::Op_Mul:
            if(inst->GetType() == IntType::NewIntTypeGet())
                extraDealIntBinary(RInst,inst,RISCVInst::_mulw);
            else if(inst->GetType() == FloatType::NewFloatTypeGet())
                extraDealFloatBinary(RInst,inst,RISCVInst::_fmul_s);
            else { assert("other conditions"); }
            break;
        case BinaryInst::Op_Div:
            if(inst->GetType() == IntType::NewIntTypeGet())
                extraDealIntBinary(RInst,inst,RISCVInst::_divw);
            else if(inst->GetType() == FloatType::NewFloatTypeGet())
                extraDealFloatBinary(RInst,inst,RISCVInst::_fdiv_s);
            else { assert("other conditions"); }
            break;
        case BinaryInst::Op_Mod:
            extraDealIntBinary(RInst,inst,RISCVInst::_remw);
            break;
        // 比较在这里不分区别，在 UnCond 的时候才有分明
        case BinaryInst::Op_L:
        case BinaryInst::Op_G: 
        case BinaryInst::Op_LE:
        case BinaryInst::Op_GE:
        case BinaryInst::Op_E:
        case BinaryInst::Op_NE:
            if (dynamic_cast<Instruction*>(valOp1) && dynamic_cast<Instruction*>(valOp2)) {
                break;
            }
            else {
                extraDealCmp(RInst, inst);
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
        Inst3->SetVirRegister();
        Inst3->setStoreOp(Inst3->GetPrevNode());
        Inst3->push_back(std::make_shared<RISCVOp>("rtz"));
    } else {
        Inst = CreateInstAndBuildBind(RISCVInst::_fcvt_w_s, inst);
        Inst->SetVirRegister();
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
        Inst2->SetVirFloatRegister();
        Inst2->setStoreOp(Inst2->GetPrevNode());
        Inst->DealMore(Inst2);
    } else {
        Inst = CreateInstAndBuildBind(RISCVInst::_fcvt_s_w, inst);
        Inst->SetVirFloatRegister();
        auto val = inst->GetOperand(0);
        auto RISCVinst = mapTrans(val)->as<RISCVInst>();
        if (RISCVinst == nullptr)
            Inst->setStoreOp(Inst->GetPrevNode());
        else {
            Inst->setStoreOp(RISCVinst);
        }
    }

    return Inst;
}

RISCVInst *RISCVContext::DealMemcpyFunc(CallInst *inst)
{
    Value *tmpAlloc = inst->GetOperand(1);
    Value *glVal = inst->GetOperand(2);
    getCurFunction()->getGepGloblToLocal()[tmpAlloc] = glVal;

    Value *val = inst->GetOperand(3);

    RISCVInst *laInst = CreateInstAndBuildBind(RISCVInst::_la, inst);
    laInst->SetVirRegister();
    laInst->SetAddrOp(inst->GetOperand(2));

    int size = std::stoi(val->GetName()) + curMfunc->arroffset;
    RISCVInst *saveS0Inst = nullptr;
    if (size <= 2047 && size >= -2048)
    {
        saveS0Inst = CreateInstAndBuildBind(RISCVInst::_addi, inst);
        saveS0Inst->SetVirRegister();
        saveS0Inst->SetRealRegister("s0");
    }
    else
    {
        saveS0Inst = CreateInstAndBuildBind(RISCVInst::_li, inst);
        saveS0Inst->SetVirRegister();

        RISCVInst *addInst = CreateInstAndBuildBind(RISCVInst::_add, inst);
        addInst->push_back(saveS0Inst->getOpreand(0));
        addInst->SetRealRegister("s0");
        addInst->push_back(saveS0Inst->getOpreand(0));
    }
    // saveS0Inst->SetstackOffsetOp("-"+std::to_string(std::stoi(val->GetName())+ 16));
    getCurFunction()->getCurFuncArrStack(saveS0Inst, val, tmpAlloc);
    // how to imm

    RISCVInst *para0 = CreateInstAndBuildBind(RISCVInst::_mv, inst);
    para0->SetRealRegister("a0");
    para0->getOpsVec().push_back(saveS0Inst->getOpreand(0));

    RISCVInst *para1 = CreateInstAndBuildBind(RISCVInst::_mv, inst);
    para1->SetRealRegister("a1");
    para1->getOpsVec().push_back(laInst->getOpreand(0));

    RISCVInst *para2 = CreateInstAndBuildBind(RISCVInst::_li, inst);
    para2->SetRealRegister("a2");
    para2->SetImmOp(val);

    RISCVInst* Inst = CreateInstAndBuildBind(RISCVInst::_call, inst);
    Inst->push_back(std::make_shared<RISCVOp>("memcpy@plt"));

    return nullptr;
}

RISCVInst* RISCVContext::CreateCInst(CallInst *inst)
{
    RISCVInst* Inst = nullptr;
    RISCVInst* param = nullptr;
    RISCVInst* ret = nullptr;
    auto name = inst->GetOperand(0)->GetName();
    if (name == "llvm.memcpy.p0.p0.i32")  // 特殊函数从处理
    {
        return DealMemcpyFunc(inst);
    }

    getCurFunction()->getNeadStackForparam() = inst->GetOperandNums() -1;
    // param  callee_saved  caller_saved  call  ret
    int Num = inst->GetOperandNums() - 1;
    if (Num >= 8)
    {
        for (int paramNum = 9; paramNum < inst->GetOperandNums(); paramNum++)
        {
            if (paramNum > 8)
            { // 逻辑不严谨，sd 可能需要处理
                Value *val = inst->GetOperand(paramNum);
                if (dynamic_cast<CallInst *>(val))
                    continue;
                if (dynamic_cast<ConstantData *>(val))
                {
                    param = CreateInstAndBuildBind(RISCVInst::_li, inst);
                    param->SetVirRegister();
                    param->SetImmOp(val);

                    RISCVInst *sdInst = CreateInstAndBuildBind(RISCVInst::_sd, inst);
                    sdInst->push_back(param->getOpreand(0));
                    size_t num = (paramNum - 9) * 8;
                    sdInst->SetstackOffsetOp(std::to_string(num) + "(sp)");
                }
                else
                {
                    auto lwInst = mapTrans(val)->as<RISCVInst>();
                    if (lwInst == nullptr) // need to deal
                        continue;
                    
                    RISCVInst *sdInst = CreateInstAndBuildBind(RISCVInst::_sd, inst);
                    sdInst->push_back(lwInst->getOpreand(0));
                    size_t num = (paramNum - 9) * 8;
                    sdInst->SetstackOffsetOp(std::to_string(num) + "(sp)");
                }
            }
        }
    }

    for(int paramNum = 1; paramNum < inst->GetOperandNums() ; paramNum++)
    {
        if (paramNum > 8) {  // 逻辑不严谨，sd 可能需要处理

        }
        else {
            Value *val = inst->GetOperand(paramNum);
            if (dynamic_cast<CallInst *>(val))
                continue;
            if (dynamic_cast<ConstantData *>(val))
            {
                param = CreateInstAndBuildBind(RISCVInst::_li, inst);
                param->SetRealRegister("a" + std::to_string(paramNum - 1));
                param->SetImmOp(val);
            }
            else
            {
                auto lwInst = mapTrans(val)->as<RISCVInst>();
                if (lwInst == nullptr) // need to deal
                    continue;
                // auto op = std::make_shared<RealRegister> ("a" + std::to_string(paramNum - 1));
                // lwInst->replacedIndexWithVal(0,op);
                //lwInst->getOpreand(0)->as<VirRegister>()->reWriteRegWithReal("a" + std::to_string(paramNum - 1));
                param = CreateInstAndBuildBind(RISCVInst::_mv, inst);
                param->SetRealRegister("a" + std::to_string(paramNum - 1));
                param->push_back(lwInst->getOpreand(0));
            }
        }
    }
    // call
    Inst = CreateInstAndBuildBind(RISCVInst::_call,inst);
    Inst->push_back(std::make_shared<RISCVOp>(inst->GetOperand(0)->GetName()));
    if (param != nullptr)
        Inst->DealMore(param);
    // ret
    ret = CreateInstAndBuildBind(RISCVInst::_mv,inst);
    ret->SetVirRegister();
    ret->SetRealRegister("a0");
    Inst->DealMore(ret);
   
    return ret;
}


size_t RISCVContext:: getSumOffset(Value* globlVal,GepInst *inst)
{
    size_t sum = 0;
    int counter = 2;
    int size = 0;
    std::vector<int> numsRecord, findRecord;
    PointerType *Pointer = dynamic_cast<PointerType *>(globlVal->GetType());
    ArrayType *arry = dynamic_cast<ArrayType *>(Pointer->GetSubType());
    int layer = arry->GetLayer();
    // if (dynamic_cast<ConstIRInt*> (inst->GetOperand(counter)))
    while (arry && arry->GetLayer() != 0)
    {
        numsRecord.emplace_back(arry->GetNum());
        arry = dynamic_cast<ArrayType *>(arry->GetSubType());
    }
    while (layer--)
    {
        if ( dynamic_cast<ConstIRInt *>(inst->GetOperand(counter)))
        { // 取值确定
            findRecord.emplace_back(inst->GetOperand(counter)->as<ConstIRInt>()->GetVal());
            counter++;
            if ( counter == inst->GetOperandNums())
                break;
        }
    }
    size = findRecord.size();
    for (int i = 0; i < size - 1; i++)
    {
        int tmpSum = 1;
        for (int j = 1 + i; j <= size - 1; j++)
            tmpSum *= numsRecord[j];
        tmpSum *= findRecord[i];
        sum += tmpSum;
    }
    sum += findRecord[size - 1], sum *= 4;
    return sum;
}

void RISCVContext::getDynmicSumOffset(Value* globlVal,GepInst *inst,RISCVInst *addiInst,RISCVInst*& RInst)
{
    std::vector<int> numsRecord;
    PointerType *Pointer = dynamic_cast<PointerType *>(globlVal->GetType());
    ArrayType *arry = dynamic_cast<ArrayType *>(Pointer->GetSubType());
    int layer = arry->GetLayer();
    // if (dynamic_cast<ConstIRInt*> (inst->GetOperand(counter)))
    while (arry && arry->GetLayer() != 0)
    {
        numsRecord.emplace_back(arry->GetNum());
        arry = dynamic_cast<ArrayType *>(arry->GetSubType());
    }
    for (int i = inst->GetOperandNums() - 1; i >= 2; i--)
    {
        int Rsubscript = i - 1;
        if (auto val = dynamic_cast<ConstIRInt *>(inst->GetOperand(i)))
        { // 取值确定
            int sum = 1;
            for (int j = Rsubscript; j < numsRecord.size() ; j++)
            {
                sum *= numsRecord[j];
            }
            if (i == numsRecord.size() +1 && val->GetVal() != 0) 
                sum = val->GetVal();
            else  
                sum *= val->GetVal();
            sum *= 4;
            if (sum != 0)
            {
                if ( sum <= 2047 && sum >= -2048) {
                    RInst = CreateInstAndBuildBind(RISCVInst::_addi, inst);
                    RInst->push_back(RInst->GetPrevNode()->getOpreand(0));
                    RInst->push_back(RInst->GetPrevNode()->getOpreand(0));
                    RInst->SetImmOp(ConstIRInt::GetNewConstant(sum));
                } else {
                    RISCVInst* liInst = CreateInstAndBuildBind(RISCVInst::_li, inst);
                    liInst->SetVirRegister();
                    liInst->SetImmOp(ConstIRInt::GetNewConstant(sum));

                    RInst = CreateInstAndBuildBind(RISCVInst::_add, inst);
                    RInst->push_back(RInst->GetPrevNode()->GetPrevNode()->getOpreand(0));
                    RInst->push_back(RInst->GetPrevNode()->GetPrevNode()->getOpreand(0));
                    RInst->push_back(liInst->getOpreand(0));
                }
            }
        }
        else
        {
            auto loadInst = dynamic_cast<Instruction *>(inst->GetOperand(i));
            if (loadInst == nullptr)
                LOG(ERROR,"what happened");
            RISCVInst *RLInst = mapTrans(loadInst)->as<RISCVInst>();
            int sum = 1;
            for (int j = numsRecord.size() - 1; j > i-2; j--)
            {
                sum *= numsRecord[j];
            }

            if (sum != 1) {
                RISCVInst *liInst = CreateInstAndBuildBind(RISCVInst::_li, inst);
                liInst->SetVirRegister();
                liInst->SetImmOp(ConstIRInt::GetNewConstant(sum));


                RISCVInst *mulInst = CreateInstAndBuildBind(RISCVInst::_mul,inst);
                mulInst->push_back(RLInst->getOpreand(0));
                mulInst->push_back(RLInst->getOpreand(0));
                mulInst->push_back(liInst->getOpreand(0));

                RISCVInst *slliInst = CreateInstAndBuildBind(RISCVInst::_slli, inst);
                slliInst->push_back(RLInst->getOpreand(0));
                slliInst->push_back(RLInst->getOpreand(0));
                slliInst->SetImmOp(ConstIRInt::GetNewConstant(2));

                RInst = CreateInstAndBuildBind(RISCVInst::_add, inst);
                RInst->push_back(slliInst->getOpreand(0));
                RInst->push_back(liInst->GetPrevNode()->getOpreand(0));
                RInst->push_back(slliInst->getOpreand(0));
            }
            else {
                RISCVInst *slliInst = CreateInstAndBuildBind(RISCVInst::_slli, inst);
                slliInst->push_back(RLInst->getOpreand(0));
                slliInst->push_back(RLInst->getOpreand(0));
                slliInst->SetImmOp(ConstIRInt::GetNewConstant(2));

                RInst = CreateInstAndBuildBind(RISCVInst::_add, inst);
                RInst->push_back(slliInst->getOpreand(0));
                RInst->push_back(addiInst->getOpreand(0));
                RInst->push_back(slliInst->getOpreand(0));
            }
        }
    }
}

// 处理数组  全局的数组应该 la 进行首地址的加载
// 局部数组已经通过了 memcpy 拷贝到了栈本地，因此不需要再 la 
// 而且局部数组的加载开销要小很多
RISCVInst* RISCVContext::CreateGInst(GepInst *inst)
{
    Value* globlVal = inst->GetOperand(0);
    RISCVInst* RInst = nullptr;

    if (globlVal != nullptr && globlVal->isGlobal()) {  // 全局数组的处理
        RISCVInst* laInst = CreateInstAndBuildBind(RISCVInst::_la, inst);
        laInst->SetVirRegister();
        laInst->SetAddrOp(globlVal);

        Instruction *nextInst = inst->GetNextNode();
        if (dynamic_cast<LoadInst *>(nextInst) || dynamic_cast<StoreInst *>(nextInst)
            || dynamic_cast<CallInst*>(nextInst) )
        {
            // auto &gepRecord = getCurFunction()->getRecordGepOffset();
            int flag = 0;
            for (int i = inst->GetOperandNums() - 1; i >= 2; i--)
            {
                if (auto val = dynamic_cast<ConstIRInt *>(inst->GetOperand(i)))
                { }
                else  {
                    flag = 1;
                    break;
                }
            }
            if (flag == 0)
            {
                int sum = getSumOffset(globlVal, inst);
                if (sum <= 2047 && sum >= -2048) {
                    RInst = CreateInstAndBuildBind(RISCVInst::_addi, inst);
                    RInst->SetVirRegister();
                    RInst->push_back(laInst->getOpreand(0));
                    RInst->SetImmOp(ConstIRInt::GetNewConstant(sum));
                } else {

                    RISCVInst* liInst = CreateInstAndBuildBind(RISCVInst::_li, inst);
                    liInst->SetVirRegister();
                    liInst->SetImmOp(ConstIRInt::GetNewConstant(sum));

                    RInst = CreateInstAndBuildBind(RISCVInst::_add, inst);
                    RInst->SetVirRegister();
                    RInst->push_back(laInst->getOpreand(0));
                    RInst->push_back(liInst->getOpreand(0));
                }
            }
            else
            {
                getDynmicSumOffset(globlVal, inst, laInst, laInst);
            }
        }
        if (RInst == nullptr)
            RInst = laInst;
    } 
    else if((getCurFunction()->getGepGloblToLocal()[globlVal]) != nullptr)
    {
        // 找出 栈帧开辟此数组的首地址
        size_t arroffset = getCurFunction()->getLocalArrToOffset()[globlVal];
        RISCVInst *addiInst = nullptr;
        if (arroffset <= 2047 )
        {
            addiInst = CreateInstAndBuildBind(RISCVInst::_addi, inst);
            addiInst->SetVirRegister();
            addiInst->SetRealRegister("s0");
            addiInst->SetstackOffsetOp("-" + std::to_string(arroffset));
        } else {
            addiInst = CreateInstAndBuildBind(RISCVInst::_li, inst);
            addiInst->SetVirRegister();
            addiInst->SetstackOffsetOp("-" + std::to_string(arroffset));

            RISCVInst *addInst = CreateInstAndBuildBind(RISCVInst::_add, inst);
            addInst->push_back(addiInst->getOpreand(0));
            addInst->SetRealRegister("s0");
            addInst->push_back(addiInst->getOpreand(0));
        }

        int flag = 0;
        for(int i = inst->GetOperandNums()-1 ; i >= 2 ; i--) {
            if (auto val = dynamic_cast<ConstIRInt *>(inst->GetOperand(i))) 
            {    }
            else {
                flag = 1;
                break;
            }
        }
        if (flag == 0) {   // addi 了 之后 0(t0) 这样的形式
            int size = getSumOffset(globlVal, inst);
            if (size <= 2047 && size >= -2048)
            {
                RInst = CreateInstAndBuildBind(RISCVInst::_addi, inst);
                RInst->SetVirRegister();
                RInst->push_back(addiInst->getOpreand(0));
                RInst->SetImmOp(ConstIRInt::GetNewConstant(size));
            }
            else  {
                RISCVInst *liInst = CreateInstAndBuildBind(RISCVInst::_li, inst);
                liInst->SetVirRegister();
                liInst->SetImmOp(ConstIRInt::GetNewConstant(size));

                RInst = CreateInstAndBuildBind(RISCVInst::_add, inst);
                RInst->SetVirRegister();
                RInst->push_back(addiInst->getOpreand(0));
                RInst->push_back(liInst->getOpreand(0));
            }
        }
        else {
            getDynmicSumOffset(globlVal, inst,addiInst, RInst);
        }
    }  
    else  {    // 指针和特殊的数组
        if (auto allocInst = dynamic_cast<AllocaInst*>(globlVal))
        {
            PointerType *Pointer = dynamic_cast<PointerType *>(allocInst->GetType());
            ArrayType *arry = dynamic_cast<ArrayType *>(Pointer->GetSubType());
            int layer = arry->GetLayer();
            std::vector<int> numsRecord;
            while (arry && arry->GetLayer() != 0)
            {
                numsRecord.emplace_back(arry->GetNum());
                arry = dynamic_cast<ArrayType *>(arry->GetSubType());
            }
            size_t val = 1;
            for(int i = 0; i < numsRecord.size(); i++)
            {
                val *= numsRecord[i];
            }
            val = 4 * val;
            RISCVInst* addInst = nullptr;
            if (!getCurFunction()->getLocalArrToOffset()[globlVal]) {
                int size = std::stoi(ConstIRInt::GetNewConstant(val)->GetName());
                // 维护一种关系
                if (size <= 2047 && size >= -2048) {
                    addInst = CreateInstAndBuildBind(RISCVInst::_addi, inst);
                    addInst->SetVirRegister();
                    addInst->SetRealRegister("s0");
                    getCurFunction()->getCurFuncArrStack(addInst, ConstIRInt::GetNewConstant(val), allocInst);
                } else {
                    RISCVInst* liInst = CreateInstAndBuildBind(RISCVInst::_li,inst);
                    liInst->SetVirRegister();
                    getCurFunction()->getCurFuncArrStack(liInst, ConstIRInt::GetNewConstant(val), allocInst);

                    addInst = CreateInstAndBuildBind(RISCVInst::_add,inst);
                    addInst->push_back(liInst->getOpreand(0));
                    addInst->SetRealRegister("s0");
                    addInst->push_back(liInst->getOpreand(0));
                }
            } else {
                int size = getCurFunction()->getLocalArrToOffset()[globlVal];
                if (size <= 2047 && size >= -2048) {
                    addInst = CreateInstAndBuildBind(RISCVInst::_addi, inst);
                    addInst->SetVirRegister();
                    addInst->SetRealRegister("s0");
                    addInst->SetstackOffsetOp("-" + std::to_string(getCurFunction()->getLocalArrToOffset()[globlVal]));
                } else {
                    RISCVInst* liInst = CreateInstAndBuildBind(RISCVInst::_li,inst);
                    liInst->SetVirRegister();
                    liInst->SetstackOffsetOp("-" + std::to_string(getCurFunction()->getLocalArrToOffset()[globlVal]));

                    addInst = CreateInstAndBuildBind(RISCVInst::_add,inst);
                    addInst->push_back(liInst->getOpreand(0));
                    addInst->SetRealRegister("s0");
                    addInst->push_back(liInst->getOpreand(0));
                }
            }

            int flag = 0;
            for (int i = inst->GetOperandNums() - 1; i >= 2; i--)
            {
                if (auto val = dynamic_cast<ConstIRInt *>(inst->GetOperand(i)))
                { }
                else {
                    flag = 1;
                    break;
                }
            }
            if (flag == 0)
            { 
                int size = getSumOffset(globlVal, inst);
                if (size <= 2047 && size >= -2048) {
                    RInst = CreateInstAndBuildBind(RISCVInst::_addi, inst);
                    RInst->SetVirRegister();
                    RInst->push_back(addInst->getOpreand(0));
                    RInst->SetImmOp(ConstIRInt::GetNewConstant(size));
                } else {
                    RISCVInst* liInst = CreateInstAndBuildBind(RISCVInst::_li,inst);
                    liInst->SetVirRegister();
                    liInst->SetImmOp(ConstIRInt::GetNewConstant(size));

                    RInst = CreateInstAndBuildBind(RISCVInst::_add, inst);
                    RInst->SetVirRegister();
                    RInst->push_back(addInst->getOpreand(0));
                    RInst->push_back(liInst->getOpreand(0));
                }
            }
            else
            {
                getDynmicSumOffset(globlVal, inst, addInst, RInst);
            }
        }
        else  {  // 函数参数指针的偏移到这里了
            if (dynamic_cast<ConstIRInt *>(inst->GetOperand(inst->GetOperandNums() - 1)))
            {
                RISCVInst *liInst = CreateInstAndBuildBind(RISCVInst::_li, inst);
                liInst->SetVirRegister();
                liInst->SetImmOp(inst->GetOperand(inst->GetOperandNums() - 1));

                RISCVInst *slliInst = CreateInstAndBuildBind(RISCVInst::_slli, inst);
                slliInst->push_back(liInst->getOpreand(0));
                slliInst->push_back(liInst->getOpreand(0));
                slliInst->SetImmOp(ConstIRInt::GetNewConstant(2));

                RInst = CreateInstAndBuildBind(RISCVInst::_add, inst);
                RInst->push_back(RInst->GetPrevNode()->getOpreand(0));
                RInst->push_back(RInst->GetPrevNode()->GetPrevNode()->GetPrevNode()->getOpreand(0));
                RInst->push_back(RInst->GetPrevNode()->getOpreand(0));
            }
            else
            {
                RISCVInst *slliInst = CreateInstAndBuildBind(RISCVInst::_slli, inst);
                Instruction *LInst = dynamic_cast<Instruction *>(inst->GetPrevNode()->GetPrevNode());
                // LInst 理论上来说是下标的加载
                RISCVInst *RLInst = mapTrans(LInst)->as<RISCVInst>();
                if (RLInst == nullptr)
                    assert("error!!!");
                slliInst->push_back(RLInst->getOpreand(0));
                slliInst->push_back(RLInst->getOpreand(0));
                slliInst->SetImmOp(ConstIRInt::GetNewConstant(2));

                RInst = CreateInstAndBuildBind(RISCVInst::_add, inst);
                RInst->push_back(RInst->GetPrevNode()->getOpreand(0));
                RInst->push_back(RInst->GetPrevNode()->GetPrevNode()->getOpreand(0));
                RInst->push_back(RInst->GetPrevNode()->getOpreand(0));
            }
        }
    }
    return RInst;
}


RISCVInst* RISCVContext::CreateZInst(ZextInst *inst) 
{   // andi a5, a5, 1
    int value = 0;
    auto op = inst->GetOperand(0);
    auto it = dynamic_cast<ConstIRBoolean*> (inst->GetOperand(0));
    if (it == nullptr)
    {
        auto prevInst = dynamic_cast<BinaryInst*> (inst->GetPrevNode());
        auto XorInst = CreateInstAndBuildBind(RISCVInst::_xor, inst);
        XorInst->push_back(XorInst->GetPrevNode()->getOpreand(0));
        XorInst->push_back(XorInst->GetPrevNode()->getOpreand(0));
        XorInst->push_back(XorInst->GetPrevNode()->GetPrevNode()->getOpreand(0));
        auto SeqzInst = CreateInstAndBuildBind(RISCVInst::_seqz,inst);
        SeqzInst->push_back(XorInst->getOpreand(0));
        SeqzInst->push_back(XorInst->getOpreand(0));
        return SeqzInst;
    }
    if (it->GetVal())
        value = 1;
    RISCVInst* liInst = CreateInstAndBuildBind(RISCVInst::_li,inst);
    liInst->SetVirRegister();
    liInst->SetImmOp(ConstIRInt::GetNewConstant(value));

    RISCVInst* andiInst = CreateInstAndBuildBind(RISCVInst::_andi, inst);
    andiInst->push_back(liInst->getOpreand(0));
    andiInst->push_back(andiInst->getOpreand(0));
    andiInst->SetImmOp(ConstIRInt::GetNewConstant(1));
    return andiInst; 
}

// maybe need not to deal those!!!
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
        // auto it = new RISCVBlock(block,".BB"+RISCVBlock::getCounter());
        auto it = new RISCVBlock(block,block->GetName());
        //离谱的api
        // auto  func = block->GetParent();
        auto parent = mapTrans(block->GetParent())->as<RISCVFunction> ();
        
        // 把BB父亲设置为func
        // func 插入 BB
        parent->push_back(it);
        parent->getRecordBBs().push_back(it);  // 打印顺序
        it->getIRbb() = block;
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
    if(valToRiscvOp.find(val) == valToRiscvOp.end() && val->isGlobal()) {
        auto op  = new RISCVOp(val->GetName());
        valToRiscvOp[val] = op;   // bug:: delete when???
    }

    if(valToRiscvOp.find(val) == valToRiscvOp.end()){
        valToRiscvOp[val] = Create(val);
    }
    return valToRiscvOp[val];
};

// void RISCVContext::DealFunctionParam(Function *func)
// {
//     RealRegister::realReg counter = RealRegister::a0;
//     getCurFunction()->getparamNum() = func->GetParams().size();
//     for (auto &parm : func->GetParams())
//     {
//         if (counter > RealRegister::a7) // 通过栈帧传递
//         {
//             getCurFunction()->getSpilledParamType().push_back( parm->GetTypeEnum());
//         }
//         else {
//             // 前 7 个参数通过 a0 - a7 传递
//             auto op = RealRegister::GetRealReg(counter);
//             valToRiscvOp[parm.get()] = op;
//             counter = RealRegister::realReg(counter + 1);
//         }
//     }
// }

void RISCVContext::DealFunctionParam(Function *func)
{
    RealRegister::realReg Intcounter = RealRegister::a0;
    RealRegister::realReg Floatcounter = RealRegister::fa0;
    getCurFunction()->getparamNum() = func->GetParams().size();
    for (auto &parm : func->GetParams())
    {
        if(parm->GetType() == FloatType::NewFloatTypeGet()) 
        {

            if (Floatcounter > RealRegister::fa7) // 通过栈帧传递
            {
                getCurFunction()->getSpilledParamType().push_back(parm->GetTypeEnum());
            }
            else  {
                // 前 7 个参数通过 fa0 - fa7 传递
                auto op = RealRegister::GetRealReg(Floatcounter);
                valToRiscvOp[parm.get()] = op;
                Floatcounter = RealRegister::realReg(Floatcounter + 1);
            }
        }
        else {  // 数组没有处理，全都是按照 a0-7 处理d
            if (Intcounter > RealRegister::a7) // 通过栈帧传递
            {
                getCurFunction()->getSpilledParamType().push_back(parm->GetTypeEnum());
            }
            else
            {
                // 前 7 个参数通过 a0 - a7 传递
                auto op = RealRegister::GetRealReg(Intcounter);
                valToRiscvOp[parm.get()] = op;
                Intcounter = RealRegister::realReg(Intcounter + 1);
            }
        }
    }
}
