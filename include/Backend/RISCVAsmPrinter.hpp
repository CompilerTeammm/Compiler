#pragma once
#include <fstream>
#include <iostream>
#include <sstream>
// #include "../../include/Backend/RISCVFrameContext.hpp"
// #include "../../include/Backend/RISCVMIR.hpp"
// #include "../../include/Backend/RISCVContext.hpp"
// #include "../../include/Backend/FloatToDex.hpp"
// #include "../../include/lib/MagicEnum.hpp"

class globlvar;
class tempvar;
class functionSegment;
class textSegment;
class dataSegment;
enum SegmentType
{
    TEXT,
    DATA,
    BSS,
    RODATA
};

SegmentType ChangeSegmentType(SegmentType newtype);
void PrintSegmentType(SegmentType newtype, SegmentType *oldtype);

class RISCVAsmPrinter
{
public:
    RISCVAsmPrinter(std::string filename, Moudle *unit, RISCVLoweringContext &ctx);
    ~RISCVAsmPrinter() = default;
    void SetTextSegment(textSegment *);
    dataSegment *&GetData();
    void printAsmGlobal();
    void printAsm();

protected:
    std::string filename; // 输出文件名
    dataSegment *data;    // 数据段
    textSegment *text;    // 代码段
};

class dataSegment
{
public:
    dataSegment(Moudle *moudle, RISCVLoweringContext &ctx);
    ~dataSegment() = default;
    void GenerateGloblvarList(Module *module, RISCVLoweringContext &ctx);
    void GenerateTempvarList(RISCVLoweringContext &ctx);
    std::vector<tempvar *> get_tempvar_list();
    void Change_LoadConstFloat(RISCVMIR *inst, tempvar *tempfloat, List<RISCVBasicBlock, RISCVMIR>::iterator it, Imm *used);
    void PrintDataSegment_Globval();
    void PrintDataSegment_Tempvar();
    void LegalizeGloablVar(RISCVLoweringContext &);

private:
    std::vector<globlvar *> globlvar_list;
    int num_lable;
    std::vector<tempvar *> tempvar_list;
};

class textSegment
{
private:
    std::vector<functionSegment *> function_list;

public:
    textSegment(RISCVLoweringContext &ctx);
    void GenerateFuncList(RISCVLoweringContext &ctx);
    void PrintTextSwgment();
} class functionSegment
{
}