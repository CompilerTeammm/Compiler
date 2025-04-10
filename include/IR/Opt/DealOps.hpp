#pragma once
#include "ConstantFold.hpp"
#include "ConstantProp.hpp"

// provide some tools to deal constTypes
class DealConstType
{
public:
    // Undef
    static ConstantData* DealUndefBinary(BinaryInst* inst,ConstantData* LHS,ConstantData* RHS);
    static ConstantData* DealUndefAdd(ConstantData* LHS,ConstantData* RHS);


    static ConstantData* DealIRIntAndFloat(ConstantData* LHS,ConstantData* RHS,int FLAG =0);
};

