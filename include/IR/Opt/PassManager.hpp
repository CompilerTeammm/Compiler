#pragma once
#include<queue>
#include"Passbase.hpp"
#include "CoreClass.hpp"
#include "CFG.hpp"
#include <memory>
#include <type_traits>
#include"Singleton.hpp"
#include "Mem2reg.hpp"
#include "MemoryToRegister.hpp"
#include "DCE.hpp"

class Mem2reg;
//我这里是参考了他们的用队列来储存我自己的优化Passes的名字，依次出队列进行遍历
enum PassName
{
    mem2reg_pass,
    inline_pass,
    dce_pass,
};


// PassManager 用来管理Passes
class PassManager
{   
private:
    std::queue<PassName> Passque;
    Function* _func;
    Module* _mod;
public:
    void addPass(PassName pass) { Passque.emplace(pass); }
    PassName pushPass()
    {
        PassName pass = Passque.front();
        Passque.pop();
        return pass;
    }
    // 我这里从前端获取到内存形式的 M-SSA 
    PassManager() { _mod = &Singleton<Module>(); }
    void RunOnTest();

    // 这个我希望可以作为我设计的核心函数，MyPass 代表我的passes ， 而 MyType 我这里将 Function 和 Module泛型化了
    // template<typename MyPass,typename MyType>
    // bool RunImpl(MyType mytype)
    // {
        
    // }
};

void PassManager:: RunOnTest()
{
    auto& funcVec = _mod->GetFuncTion();
    for(auto& function : funcVec)
    {
        auto fun = function.get();
        DominantTree tree(fun);
        Mem2reg(fun,&tree).run();
    }
}