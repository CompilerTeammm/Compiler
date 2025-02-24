#pragma once
#include"Passbase.hpp"
#include "CoreClass.hpp"
#include <memory>
#include <type_traits>
// using namespace std;
// 用来管理Passes
class Function;
class Module;
template <> class _PassManagerBase<class Pass, class myType>;

enum PassName {
    mem2reg,

};

class _AnalysisManager:
    public _AnalysisManagerBase<_AnalysisManager,Function>
{
   
};

class _PassManager:
    public _PassManagerBase<_PassManager,Function>
{
    bool Run();

public:
    // 检查条件为 true ： name的类型为void 模板正常实例化
    template<typename Pass, typename name = std::enable_if_t<std::is_base_of_v<_PassManagerBase<Pass,Module>,Pass>>>
    bool RunImpl(Module* mod,_AnalysisManager &AM){
        auto pass = std::make_unique<Pass>(mod,AM);
        return pass->Run();
    }

    template<typename Pass,typename name = std::enable_if_t<std::is_base_of_v<_PassManagerBase<Pass,Function>,Pass>>>
    bool RunImpl(Function* func,_AnalysisManager &AM)
    {
        auto pass = std::make_unique<Pass>(func,AM);
        return pass->Run();
    }
};
