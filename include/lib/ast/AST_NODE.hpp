#pragma once
#include "../cfg/cfgg.hpp"
#include "base_ast.hpp"

enum AST_Type
{
  AST_INT,
  AST_FLOAT,
  AST_VOID,
  AST_ADD,
  AST_SUB,
  AST_MUL,
  AST_MODULO,
  AST_DIV,
  AST_GREAT,
  AST_GREATEQ,
  AST_LESS,
  AST_LESSEQ,
  AST_EQ,
  AST_ASSIGN,
  AST_NOTEQ,
  AST_NOT,
  AST_AND,
  AST_OR,
};

struct GetInstState
{
  BasicBlock *cur_block;
  BasicBlock *break_block;
  BasicBlock *continue_block;
};
