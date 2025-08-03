#pragma once
#include <list>
#include <memory>
#include "../lib/CFG.hpp"
#include "../lib/CoreClass.hpp"
#include "../lib/MyList.hpp"
#include <string.h>
#include <string>
#include "../../Log/log.hpp"

enum RISCVType
{
    riscv_32int,
    riscv_32float,
    riscv_32bool,
    riscv_ptr,
    _nono
};

RISCVType TransType(Type* tp);
std::string floatToString(float tmpf);
