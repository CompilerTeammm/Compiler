#pragma once
#include <fstream>
#include <iostream>
#include <sstream>
// #include "../../include/Backend/RISCVFrameContext.hpp"
// #include "../../include/Backend/RISCVMIR.hpp"
// #include "../../include/Backend/RISCVContext.hpp"
// #include "../../include/Backend/FloatToDex.hpp"
// #include "../../include/lib/MagicEnum.hpp"

class RISCVAsmPrinter{
    public:
    RISCVAsmPrinter(std::string filename,Moudle* unit,RISCVLoweringContext& ctx);
    ~RISCVAsmPrinter() =default;
    protected:
    std::string filename;//输出文件名
    dataSegment* data;//数据段
    //textSegment* text;//代码段
};

class dataSegment{
    public:
    dataSegment(Moudle* moudle,RISCVLoweringContext& ctx);
    private:
}
