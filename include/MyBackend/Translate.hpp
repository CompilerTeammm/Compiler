#pragma once
#include "../lib/CFG.hpp"
#include "../lib/CoreClass.hpp"
#include "RISCVContext.hpp"
#include "RISCVPrint.hpp"
#include <memory>

// example.c:
// int main()  { return 0; }

template<typename TYPE>
class TranBase 
{
public:
    virtual bool run(TYPE*) = 0;
};

// ctx printer this is only one in the pragram
class TransModule:public TranBase<Module>
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

class TransFunction:public TranBase<Function>
{
    std::shared_ptr<RISCVContext>& ctx;
    std::shared_ptr<RISCVPrint>& printer;
public:
    TransFunction(std::shared_ptr<RISCVContext>& _ctx,
                  std::shared_ptr<RISCVPrint>& _printer)
                :ctx(_ctx),printer(_printer)   {}

    bool run(Function*) override;
};

class TransBlock:public TranBase<BasicBlock>
{
    std::shared_ptr<RISCVContext>& ctx;
    std::shared_ptr<RISCVPrint>& printer;
public:
    TransBlock(std::shared_ptr<RISCVContext> &_ctx,
               std::shared_ptr<RISCVPrint> &_printer)
              : ctx(_ctx), printer(_printer) {}
            
    bool run(BasicBlock *) override;
};
