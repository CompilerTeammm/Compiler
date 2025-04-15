#pragma once
#include "ConstantFold.hpp"
#include "ConstantProp.hpp"

// provide some tools to deal constTypes
class DealConstType
{
public:
    // Undef
    static ConstantData* DealUndefBinary(BinaryInst* inst,ConstantData* LHS,ConstantData* RHS);
    static ConstantData* DealUndefCalcu(BinaryInst::Operation Op,ConstantData* LHS,ConstantData* RHS);
    static ConstantData* DealCmp(BinaryInst::Operation Op,ConstantData* LHS,ConstantData* RHS);

    // flag = 0 ----> int    flag = 1 -----> float
    static ConstantData* DealIROpsIntOrFloat(BinaryInst::Operation Op,ConstantData* LHS,ConstantData* RHS,int FLAG =0);

    template<typename TYPE1,typename TYPE2>
    static ConstantData* ConstFoldCaclu(BinaryInst::Operation Op,TYPE1 LVal,TYPE2 RVal,int flag);
};

