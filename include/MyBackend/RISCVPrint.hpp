#pragma once
#include <string>
#include "../lib/CFG.hpp"
#include "../lib/CoreClass.hpp"
#include "MIR.hpp"
#include "RISCVContext.hpp"

class TestSegment 
{

};

class DataSegment 
{

};

class RISCVPrint
{
    std::string _fileName;
    TestSegment _text;
    DataSegment _data;
    std::shared_ptr<RISCVContext>& _context;
public:
    RISCVPrint(std::string fileName, Module* moudle,std::shared_ptr<RISCVContext>& ctx)
            :_fileName(fileName),_context(ctx)
            {    }

    // 总的
    void printAsm();
    // 架构目标等
    void printPrefix();
    void printFuncfix(std::string);

    void printFuncPro(RISCVFunction* mfunc);
    void printFuncEpi(RISCVFunction* mfunc);
    // 输出每条语句
    void printInsts(RISCVInst* inst);
};