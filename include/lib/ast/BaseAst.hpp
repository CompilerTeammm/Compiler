#ifndef _BASE_AST_
#define _BASE_AST_
#include <iostream>
#include <memory>
#include <string>
#include <cassert>
#include "../Position.hpp"
class BaseAST : public loc
{
public:
  virtual ~BaseAST() = default;
  // virtual void Dump() const = 0;
  virtual void codegen()
  {
    std::cerr << "codegen() is not implemented for this AST node.\n";
    assert(0);
  };
  virtual void print(int x)
  {
    for (int i = 0; i < x; i++)
      std::cout << "  ";
    std::cout << typeid(*this).name() << " at " << toString() << std::endl;
  }
};

#endif