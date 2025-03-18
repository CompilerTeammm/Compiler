#pragma once
#include "CoreClass.hpp"
#include <map>
#include <stack>
#include <string>

class SymbolTable
{
protected:
  std::unordered_map<std::string, std::unique_ptr<std::stack<Value *>>> mp;
  std::vector<std::vector<std::stack<Value *> *>> rec;

public:
  void Register(const std::string &name, Value *val)
  {
    auto &i = mp.emplace(name, std::make_unique<std::stack<Value *>>()).first->second;
    i->push(val);
  }

  Value *GetValueByName(const std::string &name)
  {
    auto it = mp.find(name);
    assert(it != mp.end() && !it->second->empty()); // 确保 `name` 存在且非空
    return it->second->top();
  }

  void layer_increase()
  {
    rec.push_back({});
  }

  void layer_decrease()
  {
    if (rec.empty())
      return;

    for (auto &i : rec.back())
    {
      if (i && !i->empty())
      {
        i->pop();
      }
    }
    rec.pop_back();
  }
  int IR_number(std::string str)
  {
    static std::unordered_map<std::string, int> recorder;
    return recorder[str]++;
  }
};