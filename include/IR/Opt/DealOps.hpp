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

    static ConstantData* DealIRIntOrFloat(BinaryInst::Operation Op,ConstantData* LHS,ConstantData* RHS,int FLAG =0);
    // flag = 0 ----> int    flag = 1 -----> float
    static ConstantData* ConstFoldBinaryInt(BinaryInst::Operation Op,ConstantData*LHS,ConstantData* RHS,int flag=0);
    static ConstantData* ConstFoldBinaryFloat(BinaryInst::Operation Op,ConstantData*LHS,ConstantData* RHS,int flag=0);
    static ConstantData* ConstFoldBinaryFloatAndInt(BinaryInst::Operation Op,ConstantData*F,ConstantData* I,int flag=0);
    static ConstantData* ConstFoldInt(BinaryInst::Operation Op,int LVal,int RVal,int flag=0);
};

