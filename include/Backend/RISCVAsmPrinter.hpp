#pragma once
#include <fstream>
#include <iostream>
#include <sstream>
#include "../../include/Backend/RISCVFrameContext.hpp"
#include "../../include/Backend/RISCVMIR.hpp"
#include "../../include/Backend/RISCVContext.hpp"
#include "../../include/Backend/FloatToDex.hpp"
#include "../../include/lib/MagicEnum.hpp"

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

    void set_use_cachelookup(bool);//缓存查找优化
    void set_use_cachelookup4(bool);
    void printCacheLookUp();
    void printCacheLookUp4();
//    void printParallelLib();//输出并行计算相关汇编代码

protected:
    std::string filename; // 输出文件名
    dataSegment *data;    // 数据段
    textSegment *text;    // 代码段
    bool use_cachelookup =false;
    bool use_cachelookup4 =false;
    std::string cachefilepath="RISCVLib/Cachelib.s"//缓存库路径
};

class dataSegment
{
public:
    dataSegment(Moudle *moudle, RISCVLoweringContext &ctx);//通过 Module* 获取所有全局变量，并存入 globlvar_list。
    ~dataSegment() = default;
    void GenerateGloblvarList(Module *module, RISCVLoweringContext &ctx);//遍历 Module* 获取所有全局变量。
    void GenerateTempvarList(RISCVLoweringContext &ctx);//收集 RISCVMIR 中浮点数常量，转换成 tempvar 存入 .data 段。
    std::vector<tempvar *> get_tempvar_list();
    void Change_LoadConstFloat(RISCVMIR *inst, tempvar *tempfloat, List<RISCVBasicBlock, RISCVMIR>::iterator it, Imm *used);
    //修改 RISCVMIR 指令，使浮点数存入 tempvar 并存放在 .data 段。
    void PrintDataSegment_Globval();
    void PrintDataSegment_Tempvar();
    void LegalizeGloablVar(RISCVLoweringContext &);//

private:
    std::vector<globlvar *> globlvar_list;//.data存储全局变量
    int num_lable;//变量计数，用于生成唯一标签，给 tempvar 使用，以防止变量名重复。
    std::vector<tempvar *> tempvar_list;//存储浮点数常量
};
class globlvar:public RISCVGlobalObject{
    private:
    int align;
    std::string ty="object";
    int size;
    std::vector<std::variant<int,float>> init_vector;
    public:
    globlvar(Variable* data);
    ~globlvar()=default;
    void generate_array_init(Initializer* arry_init,Type* basetype);
    void PrintGloblvar();
}
class tempvar:public RISCVTempFloatObject{
    private:
    int num_lable;
    int align;
    float init;
    public:
    tempvar(int num_lable,float init);
    ~tempvar()=default;
    std::string Getname();
    float GetInit(){
        return init;
    }
    void PrintTempvar();
}

class textSegment
{
private:
    std::vector<functionSegment *> function_list;

public:
    textSegment(RISCVLoweringContext &ctx);
    void GenerateFuncList(RISCVLoweringContext &ctx);
    void PrintTextSwgment();
} 
class functionSegment
{
}