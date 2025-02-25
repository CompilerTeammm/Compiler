#pragma once
#include<queue>
#include"Passbase.hpp"
#include "CoreClass.hpp"
#include <memory>
#include <type_traits>

// 我这里是参考了他们的用队列来储存我自己的优化Passes的名字，依次出队列进行遍历
enum PassName
{
    mem2reg ,
    pre,
    inline,
};

// 仅仅是叫Manager 用来管理Passes
class PassManager
{   
private:
    std::queue<PassName> Passque;
    Function* func;
    Module* mod;
public:
    void addPass(PassName pass) { Passque.emplace(pass); }
    PassName pushPass()
    {
        PassName pass = Passque.front();
        Passque.pop();
        return pass;
    }
    // 我这里从前端获取到内存形式的 M-SSA 
    PassManager() { mod = &Singleton<Module>(); }
    void RunOnTest();

    // 这个我希望可以作为我设计的核心函数，MyPass 代表我的passes ， 而 MyType 我这里将 Function 和 Module泛型化了
    template<typename MyPass,typename MyType>
    bool RunImpl(MyType *func)
    {
        auto pass = std::make_unique<MyPass>(func);
        return pass->run();
    }
};

void PassManager::RunOnTest()
{
    while(!Passque.empty())
    {
        auto name = Passque.front();
        Passque.pop();
        switch (name)
        {
        case Inline:{

        }
            break;
        default:
            for(int i = 0; i < mod->GetFuncTion().size(); i++)
            {
                switch (name)
                {
                case Mem2reg:
                {

                }
                    break;
                
                default:
                    break;
                }
            }
        }
    }
}
