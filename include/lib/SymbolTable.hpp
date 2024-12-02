#pragma once
#include "CoreBaseClass.hpp"
#include<stack>
#include<string>
#include<map>

//  std::vector<std::map<std::string, Type>> 
//  std::map<std::string, std::vector<Type>>.
class SymbolTable
{
protected:
    // scope 作用域
    using scope = std::stack<Value*>*;
    std::map<std::string,std::unique_ptr<std::stack<Value*>>> table;
    // 使用二维数组的原因是为了记录不同层次的作用域
    std::vector<std::vector<scope>> record;
public:
    void layer_increase()
    {
        record.push_back(std::vector<scope>());
    }

    void layer_decrease()
    {
        // record.back()是获取了一维数组，里面装着该层次中栈的指针
        for(auto &e:record.back())
        {
            if(!e->empty()) 
                e->pop();
        }
        record.pop_back();
    }

    Value* GetValueByName(std::string name)
    {
        auto &i = table[name];
        assert(i!=nullptr && !i->empty());
        return i->top();
    }

    void Register(std::string name,Value* val)
    {
        auto &i = table[name];
        if(i==nullptr)
            i.reset(new std::stack<Value*>());
        if(!table.empty()) 
            record.back().push_back(i.get());
        i->push(val);
    }
    // 适合用于生成与特定字符串相关的唯一编号或标识符  
    int IR_number(std::string str)
    {
        static std::unordered_map<std::string,int> cnt;
        return cnt[str]++;
    }
};