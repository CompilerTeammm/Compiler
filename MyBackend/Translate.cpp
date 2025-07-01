#include "../include/MyBackend/Translate.hpp"
#include "../include/MyBackend/RISCVSelect.hpp"
#include <memory>
#include <cassert>
#include "../include/MyBackend/PhiEliminate.hpp"
#include "../include/MyBackend/ProloAndEpilo.hpp"
#include "../include/MyBackend/RegAllocation.hpp"

extern std::string asmoutput_path;

void TransModule::InitPrintAndCtx(Module* mod)
{
    ctx = std::make_shared<RISCVContext> ();
    printer = std::make_shared<RISCVPrint>(asmoutput_path,mod,ctx);
}

bool TransModule::run(Module* mod) 
{
    InitPrintAndCtx(mod);
    std::shared_ptr<TransFunction> funcTran = std::make_shared<TransFunction>(ctx,printer);
    auto& functions = mod->GetFuncTion();
    for(auto& func : functions)
    {
        bool result = funcTran->run(func.get());
        if(result == false)
        {
            // test 
            func->print();
            fflush(stdout);
            fclose(stdout);
            assert("deal the can't translate function");
            std::cerr<< "WTFC" << std::endl;
        }
    }

    // 输出汇编代码
    printer->printAsm();
    return true;
}

// 对函数的一个主要逻辑
// 中端到后端需要一些合法化
bool TransFunction::run(Function* func)
{
    bool ret = false;
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

    if( ret == false)
        assert("Phi failed");
    // 优化的进行，延后
    //寄存器分配算法
    RegAllocation RA;
    ret = RA.run();

    if( ret == false)
        assert("RA failed");

    //约定与调用，前言与后文
    ProloAndEpilo PAE(mfunc);
    ret = PAE.run();

    return ret;
}