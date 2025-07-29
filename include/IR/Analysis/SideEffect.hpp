#pragma once
#include "../../lib/CFG.hpp"
#include "../../lib/CoreClass.hpp"
#include "Dominant.hpp"
#include "../Opt/AnalysisManager.hpp"
// 判断一个函数是否具有“副作用”（Side Effect）。副作用一般包括：
// 修改全局变量

// 修改传入的指针参数

// 调用具有副作用的函数（如 I/O、时间函数）

// 递归或调用递归函数

// 线程相关的原子操作等
class SideEffect:public _AnalysisBase<SideEffect, Module>{
    std::map<Function*, std::set<Function*>> CalleeFuncs; //CallingFuncs[A] = {B, C} 表示 A 调用了 B、C
    std::map<Function*, std::set<Function*>> CallingFuncs; //CalleeFuncs[B] = {A} 表示 B 被 A 调用。
    std::set<Function*> RecursiveFuncs;//RecursiveFuncs 存储递归函数。
    Module* module;

private:

public:
    
};