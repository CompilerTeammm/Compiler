#pragma once
#include "../../include/lib/Type.hpp"
#include <cassert>
//枚举表示riscv的类型，
enum RISCVType{
    riscv_i32,
    riscv_i64,
    riscv_float32,
    riscv_ptr,
    riscv_none,
};
//通过判断type类型返回对应的riscvtype，指针判断利用动态类型检查dynamic_cast
inline RISCVType RISCVTyper(Type* type){
    if(dynamic_cast<PointerType*>(type)){
        return RISCVType::riscv_ptr;
    }else if(type==IntType::NewIntTypeGet()){
        return RISCVType::riscv_i32;
    }else if(type==Int64Type::NewInt64TypeGet()){
        return RISCVType::riscv_i64;
    }else if(type==FloatType::NewFloatTypeGet()){
        return RISCVType::riscv_float32;
    }else if(type==BoolType::NewBoolTypeGet()){
        return RISCVType::riscv_i32;
    }else if(type==VoidType::NewVoidTypeGet()){
        return RISCVType::riscv_none;
    }
    assert(0&&"Invalid Type");
}