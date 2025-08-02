#include "../include/MyBackend/Translate.hpp"
#include "../include/MyBackend/RISCVSelect.hpp"
#include <memory>
#include <cassert>
#include "../include/MyBackend/PhiEliminate.hpp"
#include "../include/MyBackend/ProloAndEpilo.hpp"
#include "../include/MyBackend/RegAllocation.hpp"
#include "../include/MyBackend/LiveInterval.hpp"
#include <queue>
#include "../include/IR/Analysis/Dominant.hpp"

extern std::string asmoutput_path;

void TransModule::InitPrintAndCtx(Module* mod)
{
    ctx =RISCVContext::getCTX();
    printer = std::make_shared<RISCVPrint>(asmoutput_path,mod,ctx);
}

bool TransModule::run(Module* mod) 
{
    InitPrintAndCtx(mod);

    // constant also is the global
    std::shared_ptr<TransGlobalVal> GlobalValTrans = std::make_shared<TransGlobalVal>(ctx,printer);
    auto& GlobalValList = mod->GetGlobalVariable();
    for(auto& var:GlobalValList) 
    {
        bool result = GlobalValTrans->run(var.get());
        if(result == false)
        {
            LOG(ERROR,"Trans GlobalVal falied");
        }
    }
    std::shared_ptr<TransFunction> funcTrans = std::make_shared<TransFunction>(ctx,printer);
    auto& functions = mod->GetFuncTion();
    for(auto& func : functions)
    {
        bool result = funcTrans->run(func.get());
        if(result == false)
        {
            LOG(ERROR,"Trans Function failed");
        }
    }
    // 输出汇编代码
    printer->printAsm();
    return true;
}

// deal with the global vals
bool TransGlobalVal::run(Var* val)
{
    bool ret =  ctx->dealGlobalVal(val);
    return ret;
}

// 对函数的一个主要逻辑  中端到后端需要一些合法化
bool TransFunction::run(Function* func)
{
    bool ret = true;
    // 构造了 TransFunction
    auto mfunc = ctx->mapTrans(func)->as<RISCVFunction>();
    ctx->setCurFunction(mfunc);
    // RISCVFunc 与 RISCVBlock 建立了联系
    for (BasicBlock *BB : *func)
    {
        ctx->mapTrans(BB);   // 把 BB 存储起来
    }

    //auto RISCVbb = ctx->mapTrans(BB)->as<RISCVBlock>(); 如何找到被存储的BB呢！
    for(BasicBlock *BB :*func) 
    {
        // 把BB中的每一条指令建立联系
        RISCVSelect select(ctx);
        select.MatchAllInsts(BB);
    }

    changeTheOrders(mfunc);

    // 后端优化 phi 函数的消除
    PhiEliminate phi(func);
    ret = phi.run();
    if(!ret)   LOG(ERROR,"Phi failed");

    //寄存器分配算法
    RegAllocation RA(mfunc, ctx);
    ret = RA.run();
    if (!ret)
        LOG(ERROR, "RA failed");
        
    reWritestackOffse(mfunc);

    //约定与调用，前言与后序
    ProloAndEpilo PAE(mfunc);
    ret = PAE.run();

    return ret;
}

void TransFunction::reWritestackOffse(RISCVFunction*& mfunc)
{
    // 重写 global int and float
    auto gloValRecord = mfunc->getGloblValRecord();
    for(auto& inst : gloValRecord) 
    {
        auto RInst = ctx->mapTrans(inst)->as<RISCVInst>();
        std::string regName = RInst->getOpreand(1)->getName();
        RInst->deleteOp(1);
        RInst->SetstackOffsetOp("0(" + regName + ")");
    }

    // 重写 gep 的ops
    auto gepRecord = mfunc->getRecordGepOffset();
    for(auto& [inst,off] : gepRecord)
    {
        auto RInst = ctx->mapTrans(inst)->as<RISCVInst>();
        std::string regName = RInst->getOpreand(1)->getName();
        RInst->deleteOp(1);
        RInst->SetstackOffsetOp(std::to_string(off) +"(" + regName + ")");
    }
}


void TransFunction::changeTheOrders(RISCVFunction*& mfunc)
{
    auto BrVec = mfunc->getBrInstSuccBBs();
    std::map<BasicBlock*,BasicBlock*> curToSuccMap; 
    std::map<Instruction*,BasicBlock*> curToLabelMap; 
    std::map<Instruction*,int> recordOneOrTwo;
    for(auto [inst,pair1] :BrVec) 
    {
        auto curbb = inst->GetParent();
        BasicBlock* tmp = pair1.first;
        int counter = 0;
        for(auto [_,pair2]:BrVec) 
        {
            if (pair2.first == tmp)
                counter++;
            if (counter == 2) {
                tmp = pair1.second;
                break;
            }
        }
        curToSuccMap.emplace(curbb,tmp);
        if (tmp == pair1.first) {
            curToLabelMap.emplace(inst,pair1.second);
            recordOneOrTwo[inst] = 0;
        }
        else {
            curToLabelMap.emplace(inst,pair1.first);
            recordOneOrTwo[inst] = 1;
        }
    }

    auto& bbList = mfunc->getRecordBBs();
    std::list<RISCVBlock*> newList;
    RISCVBlock* succBB = nullptr;
    auto it = bbList.begin();
    while(it != bbList.end())
    {
        RISCVBlock* tmp = *it;
        if (succBB != nullptr)
        {
            if (tmp != succBB) {
                bbList.remove(succBB);
                auto nextIt = std::next(it);
                bbList.insert(nextIt, tmp);
                tmp = succBB;
            }
        }
        int flag = 0;
        for(auto& [curbb,succbb]:curToSuccMap)
        {
            succBB = nullptr;
            if(tmp == ctx->mapTrans(curbb)->as<RISCVBlock>())
            {
                flag = 1;
                succBB = ctx->mapTrans(succbb)->as<RISCVBlock>();
            }
            if (flag == 1) {
                break;
            }
        }
        newList.push_back(tmp);
        ++it;
    }
    bbList = std::move(newList);

    for(auto& [inst,bb] : curToLabelMap)
    {
        RISCVInst* RInst = ctx->mapTrans(inst)->as<RISCVInst> ();
        RInst->getOpreand(2)->setName(ctx->mapTrans(bb)->getName());
    }

    for(auto& [inst,flag] : recordOneOrTwo)
    {
        if (flag) {
            auto RInst = ctx->mapTrans(inst)->as<RISCVInst>();
            RInst->reWriteISA();
        }
    }
}
