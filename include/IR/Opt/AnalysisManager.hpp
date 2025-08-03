#pragma once
#include "Passbase.hpp"
#include "../../lib/Singleton.hpp"
#include "../../lib/CFG.hpp"
#include "../Analysis/Dominant.hpp"
#include <typeindex>


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
struct PassDeleter {
    void (*deleter)(void*);
    PassDeleter() : deleter(nullptr) {}
    template<typename T>
    PassDeleter(std::nullptr_t) : deleter(nullptr) {}
    template<typename T>
    PassDeleter(std::unique_ptr<T>*) {
        deleter = [](void* p) { delete static_cast<T*>(p); };
    }
    void operator()(void* p) const {
        if (deleter && p) deleter(p);
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
class FunctionAnalysisManager {
private:
    std::unordered_map<std::type_index, std::unique_ptr<void, PassDeleter>> passes;

public:
    FunctionAnalysisManager() = default;
    ~FunctionAnalysisManager() = default;

    // 获取或创建分析Pass（函数分析）
    template<typename Pass, typename... Args>
    Pass* get(Function* func, Args&&... args) {
        std::type_index ti(typeid(Pass));
        auto it = passes.find(ti);
        if (it != passes.end()) {
            // Pass已存在，直接返回缓存对象
            return static_cast<Pass*>(it->second.get());
        }
        // 新建Pass，缓存并返回
        Pass* pass = new Pass(func, std::forward<Args>(args)...);
        passes[ti] = std::unique_ptr<void, PassDeleter>(pass, PassDeleter());
        return pass;
    }

    // 查询已存在的Pass，找不到返回nullptr
    template<typename Pass>
    Pass* getIfExists() {
        std::type_index ti(typeid(Pass));
        auto it = passes.find(ti);
        if (it != passes.end())
            return static_cast<Pass*>(it->second.get());
        return nullptr;
    }

    void clear() {
        passes.clear();
    }
    template<typename Pass>
    void add(Function* func, Pass* pass) {
        std::type_index ti(typeid(Pass));
        passes[ti] = std::unique_ptr<void, PassDeleter>(pass, PassDeleter());
    }
};


// Module
class ModuleAnalysisManager {
private:
    std::unordered_map<std::type_index, std::unique_ptr<void, PassDeleter>> passes;

public:
    ModuleAnalysisManager() = default;
    ~ModuleAnalysisManager() = default;

    // 获取或创建分析Pass（模块分析）
    template<typename Pass, typename... Args>
    Pass* get(Module* mod, Args&&... args) {
        std::type_index ti(typeid(Pass));
        auto it = passes.find(ti);
        if (it != passes.end()) {
            return static_cast<Pass*>(it->second.get());
        }
        Pass* pass = new Pass(mod, std::forward<Args>(args)...);
        passes[ti] = std::unique_ptr<void, PassDeleter>(pass, PassDeleter());
        return pass;
    }

    template<typename Pass>
    Pass* getIfExists() {
        std::type_index ti(typeid(Pass));
        auto it = passes.find(ti);
        if (it != passes.end())
            return static_cast<Pass*>(it->second.get());
        return nullptr;
    }

    void clear() {
        passes.clear();
    }
    template<typename Pass>
    void add(Module* mod, Pass* pass) {
        std::type_index ti(typeid(Pass));
        passes[ti] = std::unique_ptr<void, PassDeleter>(pass, PassDeleter());
    }
};


// 综合分析管理器
class AnalysisManager {
private:
    FunctionAnalysisManager funcMgr;
    ModuleAnalysisManager modMgr;

public:
    AnalysisManager() = default;
    ~AnalysisManager() = default;

    // Function级Pass获取
    template<typename Pass, typename... Args>
    Pass* get(Function* func, Args&&... args) {
        return funcMgr.get<Pass>(func, std::forward<Args>(args)...);
    }
    template<typename Pass>
    Pass* getIfExists(Function* func) {
        (void)func; // 函数参数为了统一接口，这里未使用
        return funcMgr.getIfExists<Pass>();
    }

    // Module级Pass获取
    template<typename Pass, typename... Args>
    Pass* get(Module* mod, Args&&... args) {
        return modMgr.get<Pass>(mod, std::forward<Args>(args)...);
    }
    template<typename Pass>
    Pass* getIfExists(Module* mod) {
        (void)mod;
        return modMgr.getIfExists<Pass>();
    }
    void clear() {
        funcMgr.clear();
        modMgr.clear();
    }
    template<typename Pass>
    void add(Function* func, Pass* pass) {
        funcMgr.add<Pass>(func, pass);
    }

    template<typename Pass>
    void add(Module* mod, Pass* pass) {
        modMgr.add<Pass>(mod, pass);
    }
};