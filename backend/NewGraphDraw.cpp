#include "../include/Backend/RISCVFrameContext.hpp"
#include "../include/Backend/RISCVMIR.hpp"
#include "../include/Backend/RISCVMOperand.hpp"
#include "../include/Backend/RISCVRegister.hpp"
#include "../include/Backend/RISCVType.hpp"
#include "../include/Backend/RegAlloc.hpp"
#include "../util/my_stl.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <unordered_set>

//入口
void GraphColor::RunOnFunc(){
    bool condition=true;
    GC_init();//初始化图着色相关结构
    for(auto b:*m_func){
        CalCulateSucc(b);//计算每个基本块后继用于构建控制流图
    }
    CaculateTopu(m_func->front());//对基本块做拓补排序,计算支配关系等
    std::reverse(topu.begin(),topu.end());//逆序方便数据流分析
    
}