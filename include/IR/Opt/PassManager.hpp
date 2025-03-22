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


// 我这里是参考了他们的用队列来储存我自己的优化Passes的名字，依次出队列进行遍历
enum PassName
{
    mem2reg_pass,
    inline_pass,
    dce_pass,
};

// 仅仅是叫Manager 用来管理Passes
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
    template<typename MyPass,typename MyType>
    bool RunImpl(MyType mytype)
    {
        DominantTree* tree;
        auto pass = std::make_unique<MyPass> (mytype,tree);
        return pass->run();
    }
};

// 遍历可以执行的优化
void PassManager::RunOnTest()
{
    while(!Passque.empty())
    {   //针对于module 的优化
        auto name = Passque.front();
        Passque.pop();
        switch (name)
        {
        case dce_pass:{

        }
            break;
        default:
            // 测试的阶段 针对于 Function 的优化
            for(int i = 0; i < _mod->GetFuncTion().size(); i++)
            {
                switch (name)
                {
                case mem2reg_pass:
                {
                    auto& funcVector = _mod->GetFuncTion();
                    for(auto& e : funcVector)
                    {
                        _func = e.get();
                        RunImpl<Mem2reg,Function*>(_func);
                    }
                }
                    break;
                
                default:
                    break;
                }
            }
        }
    }
}
