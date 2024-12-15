#ifndef _BASE_AST_
#define _BASE_AST_
#include<iostream>
#include <memory>
#include <string>
#include "common/position.h"
class BaseAST
{
public:
  virtual ~BaseAST() = default;
  virtual void Dump() const = 0;
  virtual bool GetConstVal(int &val)const { return true;}
  virtual bool GetConstVal(float &val)const { return true;}
  virtual std::string type(void) const
  {
    std::unique_ptr<std::string> TypeStr(new std::string("BaseAST"));
    return *TypeStr;
  }
  loc position;
};

#endif 