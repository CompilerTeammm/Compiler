#pragma once
#include "CoreClass.hpp"
#include <map>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <cassert>
#include <string>

/// @brief same layer with same name is illegal
class SymbolTable
{
private:
    using recoder = std::vector<Value *>;
    std::map<std::string, std::unique_ptr<recoder>> mp;
    std::vector<std::vector<recoder *>> rec;
    std::unordered_map<std::string, Value *> cache; // 缓存最近访问的符号
    std::mutex mtx;                                 // 用于线程安全

    // 版本管理：支持符号重定义
    std::unordered_map<std::string, int> version_counter;

public:
    // 增加作用域层级
    void layer_increase()
    {
        rec.push_back(std::vector<recoder *>());
    }

    // 减少作用域层级
    void layer_decrease()
    {
        for (auto &i : rec.back())
        {
            if (!i->empty())
            {
                i->pop_back(); // 弹出栈顶元素
            }
        }
        rec.pop_back();
    }

    // 根据名字获取符号的值
    Value *GetValueByName(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(mtx); // 加锁确保线程安全

        // 优先从缓存获取
        if (cache.find(name) != cache.end())
        {
            return cache[name];
        }

        auto &i = mp[name];
        assert(i != nullptr && !i->empty());
        Value *val = i->back(); // 获取栈顶值
        cache[name] = val;      // 缓存
        return val;
    }

    // 注册一个新的符号，处理符号重定义
    void Register(const std::string &name, Value *val)
    {
        std::lock_guard<std::mutex> lock(mtx); // 加锁确保线程安全

        auto &i = mp[name];
        if (i == nullptr)
        {
            i.reset(new recoder()); // 初始化栈
        }
        if (!rec.empty())
        {
            rec.back().push_back(i.get()); // 将当前栈加入当前作用域
        }

        // 处理重定义：增加版本号并设置版本
        version_counter[name]++;
        val->SetVersion(version_counter[name]);

        i->push_back(val); // 将符号值压入栈中
    }

    // 返回某个符号的 IR 编号，用于唯一标识符的处理
    int IR_number(const std::string &str)
    {
        static std::unordered_map<std::string, int> cnt;
        return cnt[str]++;
    }
};
