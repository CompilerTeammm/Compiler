#pragma once
#include "Passbase.hpp"
#include "../../lib/CFG.hpp"
#include "../Analysis/Dominant.hpp"
// #include "../Analysis/LoopInfo.hpp"
#include <typeindex>
#include <any>

template <typename T, typename = void>
struct has_GetResult : std::false_type
{
};

template <typename T>
struct has_GetResult<T, std::void_t<decltype(std::declval<T &>().GetResult(std::declval<Function *>()))>> : std::true_type
{
};

template <typename T, typename = void>
struct has_GetResultModule : std::false_type
{
};

template <typename T>
struct has_GetResultModule<T, std::void_t<decltype(std::declval<T &>().GetResult())>> : std::true_type
{
};

class LoopInfo;
// class AnalysisManager : public _AnalysisBase<AnalysisManager, Function>
// {

// public:
//     void run();

//     template<typename Pass>
//     const auto& get()
//     {
//         using Result = typename Pass::Result;
//         return nullptr;
//     }

//     DominantTree* getTree()
//     {
//         return nullptr;
//     }

//     AnalysisManager();
//     ~AnalysisManager() = default;
// };

// 通用删除器，支持不同类型void*智能指针删除
struct PassDeleter
{
    void (*deleter)(void *);
    PassDeleter() : deleter(nullptr) {}
    template <typename T>
    PassDeleter(std::nullptr_t) : deleter(nullptr) {}
    template <typename T>
    PassDeleter(std::unique_ptr<T> *)
    {
        deleter = [](void *p)
        { delete static_cast<T *>(p); };
    }
    void operator()(void *p) const
    {
        if (deleter && p)
            deleter(p);
    }
};
// struct PassDeleter {
//     void (*deleter)(void*) = nullptr;
//     template<typename T>
//     PassDeleter(T*) {
//         deleter = [](void* p) { delete static_cast<T*>(p); };
//     }
//     void operator()(void* p) const {
//         if (deleter && p) deleter(p);
//     }
// };

// Function
/* class FunctionAnalysisManager
{
private:
    std::unordered_map<std::type_index, std::unique_ptr<void, PassDeleter>> passes;

public:
    FunctionAnalysisManager() = default;
    ~FunctionAnalysisManager() = default;

    // 获取或创建分析Pass（函数分析）
    template <typename Pass, typename... Args>
    Pass *get(Function *func, Args &&...args)
    {
        std::type_index ti(typeid(Pass));
        Pass *pass = nullptr; // 先统一声明

        auto it = passes.find(ti);
        if (it != passes.end())
        {
            pass = static_cast<Pass *>(it->second.get());
        }
        else
        {
            pass = new Pass(func, std::forward<Args>(args)...);
            passes[ti] = std::unique_ptr<void, PassDeleter>(pass, PassDeleter());
        }

        // 统一返回
        if constexpr (has_GetResult<Pass>::value)
        {
            auto *result = pass->GetResult(func);
            return static_cast<Pass *>(result);
        }
        else
        {
            return pass;
        }
    }

    // 查询已存在的Pass，找不到返回nullptr
    template <typename Pass>
    Pass *getIfExists()
    {
        std::type_index ti(typeid(Pass));
        auto it = passes.find(ti);
        if (it != passes.end())
            return static_cast<Pass *>(it->second.get());
        return nullptr;
    }

    void clear()
    {
        passes.clear();
    }
    template <typename Pass>
    void add(Function *func, Pass *pass)
    {
        std::type_index ti(typeid(Pass));
        passes[ti] = std::unique_ptr<void, PassDeleter>(pass, PassDeleter());
    }
}; */

class FunctionAnalysisManager
{
private:
    // 每个 Function 对应一个 Pass 缓存
    std::unordered_map<
        Function *,
        std::unordered_map<std::type_index, std::unique_ptr<void, PassDeleter>>>
        funcPasses;

public:
    FunctionAnalysisManager() = default;
    ~FunctionAnalysisManager() = default;

    // 获取或创建分析 Pass
    template <typename Pass, typename... Args>
    Pass *get(Function *func, Args &&...args)
    {
        auto &passMap = funcPasses[func]; // 取出 func 对应的 map
        auto ti = std::type_index(typeid(Pass));

        Pass *pass = nullptr;
        auto it = passMap.find(ti);
        if (it != passMap.end())
        {
            passMap.erase(it);
        }
        pass = new Pass(func, std::forward<Args>(args)...);
        passMap[ti] = std::unique_ptr<void, PassDeleter>(pass, PassDeleter());

        // 如果 Pass 定义了 GetResult，返回结果对象
        if constexpr (has_GetResult<Pass>::value)
        {
            auto *result = pass->GetResult(func);
            return static_cast<Pass *>(result);
        }
        else
        {
            return pass;
        }
    }

    // 查询已存在的 Pass，找不到返回 nullptr
    template <typename Pass>
    Pass *getIfExists(Function *func)
    {
        auto funcIt = funcPasses.find(func);
        if (funcIt == funcPasses.end())
        {
            return nullptr;
        }

        auto &passMap = funcIt->second;
        auto ti = std::type_index(typeid(Pass));
        auto it = passMap.find(ti);
        if (it == passMap.end())
        {
            return nullptr;
        }

        return static_cast<Pass *>(it->second.get());
    }

    // 添加一个 Pass
    template <typename Pass>
    void add(Function *func, Pass *pass)
    {
        auto &passMap = funcPasses[func]; // 自动创建 func 对应的 map
        auto ti = std::type_index(typeid(Pass));
        passMap[ti] = std::unique_ptr<void, PassDeleter>(pass, PassDeleter());
    }

    // 清空所有缓存
    void clear()
    {
        funcPasses.clear();
    }
};

class ModuleAnalysisManager
{
private:
    std::unordered_map<std::type_index, std::unique_ptr<void, PassDeleter>> passes;

public:
    ModuleAnalysisManager() = default;
    ~ModuleAnalysisManager() = default;

    // 获取或创建分析 Pass（模块分析）
    template <typename Pass, typename... Args>
    Pass *get(Module *mod, Args &&...args)
    {
        std::type_index ti(typeid(Pass));
        auto it = passes.find(ti);

        // 如果已经存在，先删除旧的 Pass
        if (it != passes.end())
        {
            passes.erase(it);
        }

        // 创建新的 Pass
        Pass *pass = new Pass(mod, std::forward<Args>(args)...);
        passes[ti] = std::unique_ptr<void, PassDeleter>(pass, PassDeleter());

        // 如果 Pass 定义了 GetResult，返回结果对象
        if constexpr (has_GetResultModule<Pass>::value)
        {
            auto *result = pass->GetResult(); // 无参数
            return static_cast<Pass *>(result);
        }
        else
        {
            return pass;
        }
    }

    // 查询已存在的 Pass，找不到返回 nullptr
    template <typename Pass>
    Pass *getIfExists()
    {
        std::type_index ti(typeid(Pass));
        auto it = passes.find(ti);
        if (it != passes.end())
            return static_cast<Pass *>(it->second.get());
        return nullptr;
    }

    // 添加一个 Pass（手动管理）
    template <typename Pass>
    void add(Module *mod, Pass *pass)
    {
        std::type_index ti(typeid(Pass));
        passes[ti] = std::unique_ptr<void, PassDeleter>(pass, PassDeleter());
    }

    // 清空所有缓存
    void clear()
    {
        passes.clear();
    }
};

/* // Module
class ModuleAnalysisManager
{
private:
    std::unordered_map<std::type_index, std::unique_ptr<void, PassDeleter>> passes;

public:
    ModuleAnalysisManager() = default;
    ~ModuleAnalysisManager() = default;

    // 获取或创建分析Pass（模块分析）
    template <typename Pass, typename... Args>
    Pass *get(Module *mod, Args &&...args)
    {
        std::type_index ti(typeid(Pass));
        auto it = passes.find(ti);
        if (it != passes.end())
        {
            return static_cast<Pass *>(it->second.get());
        }
        Pass *pass = new Pass(mod, std::forward<Args>(args)...);
        passes[ti] = std::unique_ptr<void, PassDeleter>(pass, PassDeleter());
        return pass;
    }

    template <typename Pass>
    Pass *getIfExists()
    {
        std::type_index ti(typeid(Pass));
        auto it = passes.find(ti);
        if (it != passes.end())
            return static_cast<Pass *>(it->second.get());
        return nullptr;
    }

    void clear()
    {
        passes.clear();
    }
    template <typename Pass>
    void add(Module *mod, Pass *pass)
    {
        std::type_index ti(typeid(Pass));
        passes[ti] = std::unique_ptr<void, PassDeleter>(pass, PassDeleter());
    }
}; */

enum LoopAttr
{
    Normal,
    Simplified,
    Lcssa,
    Rotate
};
// 综合分析管理器
class AnalysisManager
{
private:
    FunctionAnalysisManager funcMgr;
    ModuleAnalysisManager modMgr;

private:
    std::vector<std::any> Contain;
    std::vector<LoopInfo *> loops;
    std::unordered_map<BasicBlock *, std::set<LoopAttr>> LoopForm;
    std::unordered_set<BasicBlock *> UnrollRecord;

public:
    AnalysisManager() = default;
    ~AnalysisManager() = default;

    // Function级Pass获取
    template <typename Pass, typename... Args>
    Pass *get(Function *func, Args &&...args)
    {
        return funcMgr.get<Pass>(func, std::forward<Args>(args)...);
    }
    template <typename Pass>
    Pass *getIfExists(Function *func)
    {
        (void)func; // 函数参数为了统一接口，这里未使用
        return funcMgr.getIfExists<Pass>();
    }

    // Module级Pass获取
    template <typename Pass, typename... Args>
    Pass *get(Module *mod, Args &&...args)
    {
        return modMgr.get<Pass>(mod, std::forward<Args>(args)...);
    }
    template <typename Pass>
    Pass *getIfExists(Module *mod)
    {
        (void)mod;
        return modMgr.getIfExists<Pass>();
    }
    void clear()
    {
        funcMgr.clear();
        modMgr.clear();
    }
    template <typename Pass>
    void add(Function *func, Pass *pass)
    {
        funcMgr.add<Pass>(func, pass);
    }

    template <typename Pass>
    void add(Module *mod, Pass *pass)
    {
        modMgr.add<Pass>(mod, pass);
    }

    void AddAttr(BasicBlock *LoopHeader, LoopAttr attr)
    {
        LoopForm[LoopHeader].insert(attr);
    }

    void Unrolled(BasicBlock *LoopHeader) { UnrollRecord.insert(LoopHeader); }

    bool IsUnrolled(BasicBlock *LoopHeader)
    {
        return UnrollRecord.find(LoopHeader) != UnrollRecord.end();
    }
    bool FindAttr(BasicBlock *bb, LoopAttr attr)
    {
        if (LoopForm.find(bb) != LoopForm.end())
        {
            if (LoopForm[bb].find(attr) != LoopForm[bb].end())
                return true;
        }
        return false;
    }

    void ChangeLoopHeader(BasicBlock *Old, BasicBlock *New)
    {
        if (LoopForm.find(Old) == LoopForm.end())
            return;
        LoopForm[New] = std::move(LoopForm[Old]);
        LoopForm.erase(Old);
    }
};