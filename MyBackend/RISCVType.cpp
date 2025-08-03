#include "../include/MyBackend/RISCVType.hpp"

RISCVType TransType(Type* tp) {
    if(dynamic_cast<PointerType*>(tp)) return RISCVType::riscv_ptr;
    else if(tp == IntType::NewIntTypeGet()) return RISCVType::riscv_32int;
    else if(tp == FloatType::NewFloatTypeGet()) return RISCVType::riscv_32float;
    else if(tp == BoolType::NewBoolTypeGet()) return RISCVType::riscv_32bool;
    LOG(ERROR,"this type can't be trans");
    return RISCVType::_nono;
}

std::string floatToString(float tmpf)
{

    uint32_t n;
    memcpy(&n, &tmpf, sizeof(float)); // 直接复制内存位模式
    std::string name = std::to_string(n);
    return name;
}
