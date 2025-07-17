#pragma once
#include <string>
#include "../lib/CFG.hpp"
#include "../lib/CoreClass.hpp"
#include "MIR.hpp"
#include "RISCVContext.hpp"

class RISCVContext;
class TextSegment 
{
public:
    enum DataType 
    {
        bss,
        data
    };

    TextSegment(Value* val):value(val) { TextInit(); }

    void  FillTheWord(size_t );
    void TextInit();
    void TextPrint();
    std::string translateType();
    void generate_array_init(Initializer* arry_init, Type* basetype);
private:
    std::string name;
    DataType type;
    size_t align;
    size_t size; 
    std::vector<std::string> word;
    Value* value;
    std::vector<std::variant<int , float>> init_vector;
};

class RISCVPrint
{
    std::string _fileName;
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