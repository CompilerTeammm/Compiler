#pragma once
#include"MIR.hpp"
#include "../../Log/log.hpp"
#include "RISCVPrint.hpp"
#include <memory>

// RISCVContext 存储所有的信息
// value*   ->  RISCVOp*   这个主要问题是 Value  与 RISCVOp 不是一一匹配的
class RISCVInst;
class TextSegment;
class RISCVContext
{
    // Value*   ---->    RISCVOp*
public:
    using op = std::shared_ptr<RISCVOp>;
private:
    std::map<Value*,RISCVOp*> valToRiscvOp;

    // texts  arr 的维护
    using TextPtr = std::shared_ptr<TextSegment>; 
    std::vector<TextPtr> Texts;
    std::map<Value*,TextPtr> valToText;

    // moudle 里面维护好Mfuncs
    using MFuncPtr = std::shared_ptr<RISCVFunction>;
    std::vector<MFuncPtr> Mfuncs;
    RISCVFunction* curMfunc;
    RISCVContext() = default;
public:
    void addText(TextPtr text) {
        Texts.push_back(text);
    }
    void addFunc(MFuncPtr func) {
        Mfuncs.push_back(func);
    }

    static std::shared_ptr<RISCVContext>& getCTX()
    {
        static std::shared_ptr<RISCVContext> ctx(new RISCVContext);
        return ctx;
    }

    bool dealGlobalVal(Value* val);

    std::vector<TextPtr>& getTexts() { return Texts; }
    std::vector<MFuncPtr>& getMfuncs() {    return Mfuncs;  }

    void setCurFunction(RISCVFunction* func) {   curMfunc = func;  }
    RISCVFunction* getCurFunction() { return curMfunc; }

    RISCVOp* Create(Value*);
    RISCVOp* mapTrans(Value* val);

    // Deals with Insts
    RISCVInst* CreateLInst(LoadInst* inst);
    RISCVInst* CreateSInst(StoreInst* inst);
    RISCVInst* CreateAInst(AllocaInst* inst);
    RISCVInst* CreateCInst(CallInst* inst);
    RISCVInst* CreateRInst(RetInst* inst);
    RISCVInst* CreateCondInst(CondInst* inst);
    RISCVInst* CreateUCInst(UnCondInst* inst);
    RISCVInst* CreateBInst(BinaryInst* inst);
    RISCVInst* CreateZInst(ZextInst* inst);
    RISCVInst* CreateSInst(SextInst* inst);
    RISCVInst* CreateTInst(TruncInst* inst);
    RISCVInst* CreateMaxInst(MaxInst* inst);
    RISCVInst* CreateMinInst(MinInst* inst);
    RISCVInst* CreateSelInst(SelectInst* inst);
    RISCVInst* CreateGInst(GepInst* inst);
    RISCVInst* CreateF2IInst(FP2SIInst *inst);
    RISCVInst* CreateI2Fnst(SI2FPInst *inst);

    // 为了 Insts 专门封装的方法，主要是为了建立 RISCVBlock 与 Instrution 之间的关系
    void operator()(Instruction* Inst,RISCVInst* RCInst)
    {
        BasicBlock *BB = Inst->GetParent();
        auto it = mapTrans(BB)->as<RISCVBlock>();
        it->push_back(RCInst);
        // Inst 把 BB 设为 Parent，貌似 在 push_back 之后已经建立了父子关系 
        // RCInst->SetManager(it);
    }

    RISCVInst* CreateInstAndBuildBind(RISCVInst::ISA op,Instruction* inst);
    void extraDealStoreInst(RISCVInst* RISCVinst,StoreInst* inst);
    void extraDealLoadInst(RISCVInst* RISCVinst,LoadInst* inst);

    void extraDealBrInst(RISCVInst*& RInst,RISCVInst::ISA op,Instruction* inst,Instruction* CmpInst);
    void extraDealBeqInst(RISCVInst*& RInst,RISCVInst::ISA op,Instruction* inst);    


    // BinanryInst 
    void extraDealBinary(RISCVInst* & RInst, BinaryInst* inst, RISCVInst::ISA Op);
    // CmpInst 
    void extraDealCmp(RISCVInst* & RInst,BinaryInst* inst, RISCVInst::ISA Op=RISCVInst::_li);
    void extraDealFlCmp(RISCVInst* & RInst,BinaryInst* inst, RISCVInst::ISA Op,
                                            op op1,op op2);

    size_t getSumOffset(Value* globlVal,GepInst *inst,RISCVInst *addInst);
    void getDynmicSumOffset(Value* globlVal,GepInst *inst,RISCVInst *addiInst,RISCVInst*& RInst);

    template<typename T>
    T& as()
    {
        return dynamic_cast<T> (this);
    }
};

