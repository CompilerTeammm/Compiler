#ifndef _BASE_AST_
#define _BASE_AST_
#include <iostream>
#include <memory>
#include <string>
#include <cassert>
class BaseAST
{
public:
  virtual ~BaseAST() = default;
  virtual void codegen()
  {
    std::cerr << "codegen() is not implemented for this AST node.\n";
    assert(0);
  };
};

#endif