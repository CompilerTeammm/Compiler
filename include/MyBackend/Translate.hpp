#pragma once
#include "../lib/CFG.hpp"
#include "../lib/CoreClass.hpp"
#include "RISCVContext.hpp"
#include "RISCVPrint.hpp"
#include <memory>

template<typename TYPE>
class TransBase 
{
public:
    virtual bool run(TYPE*) = 0;
};

// ctx printer this is only one in the pragram
class TransModule:public TransBase<Module>
{
    // ctx 存储所有的内容  printer 打印需要打印的东西
    std::shared_ptr<RISCVContext> ctx;
    std::shared_ptr<RISCVPrint> printer;

public:
    bool run(Module*) override;
    void InitPrintAndCtx(Module* mod);
    TransModule() = default;
    ~TransModule() = default;
};

class TransFunction:public TransBase<Function>
{
    std::shared_ptr<RISCVContext>& ctx;
    std::shared_ptr<RISCVPrint>& printer;
public:
    TransFunction(std::shared_ptr<RISCVContext>& _ctx,
                  std::shared_ptr<RISCVPrint>& _printer)
                :ctx(_ctx),printer(_printer)   {}

    bool run(Function*) override;
};

class TransGlobalVal:public TransBase<Var>
{
    std::shared_ptr<RISCVContext>& ctx;
    std::shared_ptr<RISCVPrint>& printer;

public:
    TransGlobalVal(std::shared_ptr<RISCVContext> &_ctx,
                  std::shared_ptr<RISCVPrint> &_printer)
        : ctx(_ctx), printer(_printer) {}
        
    bool run(Var *) override;
};

