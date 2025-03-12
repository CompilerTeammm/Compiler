//使用联合体共享内存空间
#pragma once
#include <bitset>
#include <iostream>

union FloatBits{
    float floatValue;
    unsigned int intBits;
};
int BinaryToDecimal(const std::string& binStr);