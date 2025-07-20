#include "../include/MyBackend/Translate.hpp"
#include "../include/MyBackend/RISCVSelect.hpp"
#include <memory>
#include <cassert>
#include "../include/MyBackend/PhiEliminate.hpp"
#include "../include/MyBackend/ProloAndEpilo.hpp"
#include "../include/MyBackend/RegAllocation.hpp"
#include "../include/MyBackend/LiveInterval.hpp"

extern std::string asmoutput_path;

void TransModule::InitPrintAndCtx(Module* mod)
{
    ctx = std::make_shared<RISCVContext> ();
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
        // 把 BB 存储起来
        ctx->mapTrans(BB);
    }

    //auto RISCVbb = ctx->mapTrans(BB)->as<RISCVBlock>(); 如何找到被存储的BB呢！
    for(BasicBlock *BB :*func) 
    {
        // 把BB中的每一条指令建立联系
        RISCVSelect select(ctx);
        select.MatchAllInsts(BB);
    }

    // 后端优化 phi 函数的消除
    PhiEliminate phi(func);
    ret = phi.run();
    if(!ret)   LOG(ERROR,"Phi failed");

    //寄存器分配算法
    // RegAllocation RA(mfunc, ctx);
    // ret = RA.run();
    // if (!ret)
    //     LOG(ERROR, "RA failed");

    //约定与调用，前言与后序
    ProloAndEpilo PAE(mfunc);
    ret = PAE.run();

    return ret;
}