#include "../include/lib/CFG.hpp"
#include "../include/lib/ast/AstNode.hpp"

#include <algorithm>
std::string ast_type_to_string(AST_Type type)
{
  switch (type)
  {
  case AST_INT:
    return "AST_INT";
  case AST_FLOAT:
    return "AST_FLOAT";
  case AST_VOID:
    return "AST_VOID";
  case AST_ADD:
    return "AST_ADD";
  case AST_SUB:
    return "AST_SUB";
  case AST_MUL:
    return "AST_MUL";
  case AST_MODULO:
    return "AST_MODULO";
  case AST_DIV:
    return "AST_DIV";
  case AST_GREAT:
    return "AST_GREAT";
  case AST_GREATEQ:
    return "AST_GREATEQ";
  case AST_LESS:
    return "AST_LESS";
  case AST_LESSEQ:
    return "AST_LESSEQ";
  case AST_EQ:
    return "AST_EQ";
  case AST_ASSIGN:
    return "AST_ASSIGN";
  case AST_NOTEQ:
    return "AST_NOTEQ";
  case AST_NOT:
    return "AST_NOT";
  case AST_AND:
    return "AST_AND";
  case AST_OR:
    return "AST_OR";
  default:
    return "UNKNOWN";
  }
}

void AstToType(AST_Type type)
{
  switch (type)
  {
  case AST_INT:
    Singleton<IR_DataType>() = IR_Value_INT;
    break;
  case AST_FLOAT:
    Singleton<IR_DataType>() = IR_Value_Float;
    break;
  case AST_VOID:
  default:
    std::cerr << "void as variable is not allowed!\n";
    assert(0);
  }
}

BaseDef::BaseDef(std::string _name, Exps *_ad, InitVal *_initval)
    : name(_name), array_descriptor(_ad), initval(_initval) {}

BasicBlock *BaseDef::GetInst(GetInstState state)
{
  if (array_descriptor)
  {
    auto tmp = array_descriptor->GenerateArrayTypeDescriptor();
    auto alloca = state.cur_block->GenerateAlloca(tmp, name);

    if (initval)
    {
      Operand init = initval->GetOperand(tmp, state.cur_block);
      std::vector<Operand> args;
      auto src = new Var(Var::Constant, tmp, "");
      src->add_use(init);
      args.push_back(alloca);
      args.push_back(src);
      args.push_back(ConstIRInt::GetNewConstant(tmp->GetSize()));
      args.push_back(ConstIRBoolean::GetNewConstant(false));
      state.cur_block->GenerateCallInst("llvm.memcpy.p0.p0.i32", args, 0);
      std::vector<int> temp;
      static_cast<Initializer *>(init)->Var2Store(state.cur_block, name, temp);
    }
  }
  else
  {
    auto ir_data_type = Singleton<IR_DataType>();
    auto decl_type = NewTypeFromIRDataType(ir_data_type);
    bool isConstDecl = Singleton<IR_CONSTDECL_FLAG>().flag == 1;
    if (isConstDecl)
    {
      Operand var = (initval)                          ? initval->GetFirst(nullptr)
                    : (ir_data_type == IR_Value_INT)   ? ConstIRInt::GetNewConstant()
                    : (ir_data_type == IR_Value_Float) ? ConstIRFloat::GetNewConstant()
                                                       : (assert(0), Operand());

      var = (ir_data_type == IR_Value_INT) ? ToInt(var, nullptr) : ToFloat(var, nullptr);
      Singleton<Module>().Register(name, var);
    }
    else
    {
      auto alloca = state.cur_block->GenerateAlloca(decl_type, name);
      if (initval)
      {
        state.cur_block->GenerateStoreInst(initval->GetFirst(state.cur_block), alloca);
      }
    }
  }
  return state.cur_block;
}

void BaseDef::codegen()
{
  if (array_descriptor != nullptr)
  {
    auto desc = array_descriptor->GenerateArrayTypeDescriptor();
    auto var = new Var(Var::GlobalVar, desc, name);
    if (initval != nullptr)
    {
      var->add_use(initval->GetOperand(desc, nullptr));
    }
  }
  else
  {
    auto inner_data_type = Singleton<IR_DataType>();
    auto decl_type = NewTypeFromIRDataType(inner_data_type);
    auto var = new Var(Var::GlobalVar, decl_type, name);
    bool is_const_decl = Singleton<IR_CONSTDECL_FLAG>().flag == 1;

    if (is_const_decl)
    {
      Operand init_value;
      if (initval == nullptr)
      {
        switch (inner_data_type)
        {
        case IR_Value_INT:
          init_value = ConstIRInt::GetNewConstant();
          break;
        case IR_Value_Float:
          init_value = ConstIRFloat::GetNewConstant();
          break;
        default:
          throw std::runtime_error("Unsupported data type in BaseDef::codegen");
        }
      }
      else
      {
        init_value = initval->GetFirst(nullptr);
      }

      if (inner_data_type == IR_Value_INT)
      {
        init_value = ToInt(init_value, nullptr);
      }
      else if (inner_data_type == IR_Value_Float)
      {
        init_value = ToFloat(init_value, nullptr);
      }
      else
      {
        throw std::runtime_error("Unsupported data type in BaseDef::codegen");
      }

      Singleton<Module>().Register(name, init_value);
    }
    else
    {
      if (initval != nullptr)
      {
        var->add_use(initval->GetFirst(nullptr));
      }
    }
  }
}

Exps::Exps(AddExp *_data) : InnerBaseExps(_data) {}

Type *Exps::GenerateArrayTypeDescriptor()
{
  auto base_type = NewTypeFromIRDataType(Singleton<IR_DataType>());

  for (auto it = DataList.rbegin(); it != DataList.rend(); ++it)
  {
    Value *operand = (*it)->GetOperand(nullptr);
    if (auto fuc = dynamic_cast<ConstIRInt *>(operand))
    {
      base_type = ArrayType::NewArrayTypeGet(fuc->GetVal(), base_type);
    }
    else if (auto fuc = dynamic_cast<ConstIRFloat *>(operand))
    {
      base_type = ArrayType::NewArrayTypeGet(fuc->GetVal(), base_type);
    }
    else
    {
      throw std::runtime_error("Unexpected operand type in GetDeclDescipter()");
    }
  }
  return base_type;
}

Type *Exps::GenerateArrayTypeDescriptor(Type *base_type)
{
  for (auto i = DataList.rbegin(); i != DataList.rend(); i++)
  {
    auto con = (*i)->GetOperand(nullptr);
    if (auto fuc = dynamic_cast<ConstIRInt *>(con))
    {
      base_type = ArrayType::NewArrayTypeGet(fuc->GetVal(), base_type);
    }
    else if (auto fuc = dynamic_cast<ConstIRFloat *>(con))
    {
      base_type = ArrayType::NewArrayTypeGet(fuc->GetVal(), base_type);
    }
    else
    {
      throw std::runtime_error("Unexpected operand type in GetDeclDescipter()");
    }
  }
  return base_type;
}

std::vector<Operand> Exps::GetVisitDescripter(BasicBlock *block)
{
  std::vector<Operand> tmp;
  for (auto &i : DataList)
    tmp.push_back(i->GetOperand(block));
  return tmp;
}

InitVal::InitVal(BaseAST *_data)
{
  val.reset(_data);
}

Operand InitVal::GetFirst(BasicBlock *block)
{
  if (auto fuc = dynamic_cast<AddExp *>(val.get()))
    return fuc->GetOperand(block);
  else
    assert(0); // 若非 AddExp，触发断言
}

Operand InitVal::GetOperand(Type *_tp, BasicBlock *block)
{
  if (auto fuc = dynamic_cast<AddExp *>(val.get()))
  {
    return fuc->GetOperand(block);
  }
  else if (auto fuc = dynamic_cast<InitVals *>(val.get()))
  {
    return fuc->GetOperand(_tp, block);
  }
  else
  {
    return new Initializer(_tp);
  }
}

InitVals::InitVals(InitVal *_data)
{
  DataList.push_back(_data);
}

void InitVals::push_back(InitVal *_data)
{
  DataList.push_back(_data);
}

Operand InitVals::GetOperand(Type *_tp, BasicBlock *block)
{
  assert(_tp->GetTypeEnum() == IR_ARRAY);

  auto ret = new Initializer(_tp);
  size_t offs = 0;
  std::map<Type *, Type *> type_map;

  for (ArrayType *atp = dynamic_cast<ArrayType *>(_tp); atp; atp = dynamic_cast<ArrayType *>(atp->GetSubType()))
  {
    type_map[atp->GetSubType()] = atp;
  }

  auto max_type = [&](Type *tp) -> Type *
  {
    while (offs % tp->GetSize() != 0)
    {
      tp = dynamic_cast<ArrayType *>(tp)->GetSubType();
    }
    return tp;
  };

  auto sub = dynamic_cast<ArrayType *>(_tp)->GetSubType();

  for (auto &i : DataList)
  {
    Type *expectedType = max_type(sub); // 缓存对齐的期望类型
    Operand tmp = i->GetOperand(expectedType, block);

    if (tmp->GetType() != expectedType)
    {
      if (Singleton<IR_DataType>() == IR_Value_INT)
        tmp = ToInt(tmp, block);
      else if (Singleton<IR_DataType>() == IR_Value_Float)
        tmp = ToFloat(tmp, block);
      else
        assert(0);
    }

    offs += tmp->GetType()->GetSize();
    ret->push_back(tmp);

    Type *lastType = ret->back()->GetType();
    while (lastType != expectedType)
    {
      auto upper = dynamic_cast<ArrayType *>(type_map[lastType]);
      int ele = upper->GetNum();
      auto omit = new Initializer(upper);

      for (int j = 0; j < ele; j++)
      {
        omit->push_back(ret->back());
        ret->pop_back();
      }
      std::reverse(omit->begin(), omit->end());
      ret->push_back(omit);
      lastType = ret->back()->GetType();
    }
  }

  Type *lastType = ret->back()->GetType();
  while (lastType != sub)
  {
    auto upper = dynamic_cast<ArrayType *>(type_map[lastType]);
    int ele = upper->GetNum();
    auto omit = new Initializer(upper);
    for (int i = 0; i < ele && !ret->empty() && ret->back()->GetType() == upper->GetSubType(); i++)
    {
      omit->push_back(ret->back());
      ret->pop_back();
    }
    std::reverse(omit->begin(), omit->end());
    ret->push_back(omit);
    lastType = ret->back()->GetType();
  }

  return ret;
}

CompUnit::CompUnit(BaseAST *_data)
{
  DataList.push_back(_data);
}

void CompUnit::push_back(BaseAST *_data)
{
  DataList.push_back(_data);
}

void CompUnit::push_front(BaseAST *_data)
{
  DataList.push_front(_data);
}

void CompUnit::codegen()
{
  for (auto &i : DataList)
  {
    std::cout << "n";
    i->codegen();
  }
}

VarDefs::VarDefs(VarDef *_vardef)
{
  DataList.push_back(_vardef);
}

void VarDefs::push_back(VarDef *_data)
{
  DataList.push_back(_data);
}

BasicBlock *VarDefs::GetInst(GetInstState state)
{
  for (auto &i : DataList)
  {
    state.cur_block = i->GetInst(state);
  }
  return state.cur_block;
}

void VarDefs::codegen()
{
  for (auto &i : DataList)
  {
    i->codegen();
  }
}

VarDecl::VarDecl(AST_Type _tp, VarDefs *_vardefs)
    : type(_tp), vardefs(_vardefs) {}

BasicBlock *VarDecl::GetInst(GetInstState state)
{
  Singleton<IR_CONSTDECL_FLAG>().flag = 0;
  AstToType(type);
  return vardefs->GetInst(state);
}

void VarDecl::codegen()
{
  Singleton<IR_CONSTDECL_FLAG>().flag = 0;
  AstToType(type);
  vardefs->codegen();
}

ConstDefs::ConstDefs(ConstDef *_constdef)
{
  DataList.push_back(_constdef);
}

void ConstDefs::push_back(ConstDef *_data)
{
  DataList.push_back(_data);
}

BasicBlock *ConstDefs::GetInst(GetInstState state)
{
  for (auto &i : DataList)
    state.cur_block = i->GetInst(state);
  return state.cur_block;
}

void ConstDefs::codegen()
{
  for (auto &i : DataList)
    i->codegen();
}

ConstDecl::ConstDecl(AST_Type _tp, ConstDefs *_constdefs)
    : type(_tp), constdefs(_constdefs)
{
}

BasicBlock *ConstDecl::GetInst(GetInstState state)
{
  Singleton<IR_CONSTDECL_FLAG>().flag = 1;
  AstToType(type);
  return constdefs->GetInst(state);
}

void ConstDecl::codegen()
{
  Singleton<IR_CONSTDECL_FLAG>().flag = 1;
  AstToType(type);
  constdefs->codegen();
}

FunctionCall::FunctionCall(std::string _name, CallParams *ptr)
    : name(_name), callparams(ptr)
{
}

Operand FunctionCall::GetOperand(BasicBlock *block)
{
  std::vector<Operand> args;
  if (callparams != nullptr)
    args = callparams->GetParams(block);
  return block->GenerateCallInst(name, args, begin);
}

CallParams::CallParams(AddExp *_addexp)
    : InnerBaseExps(_addexp)
{
}

std::vector<Operand> CallParams::GetParams(BasicBlock *block)
{
  std::vector<Operand> params;
  for (auto &i : DataList)
    params.push_back(i->GetOperand(block));
  return params;
}

FuncParam::FuncParam(AST_Type _tp, std::string _name, bool is_empty, Exps *ptr)
    : tp(_tp), name(_name), empty_square(is_empty), array_subscripts(ptr)
{
}

void FuncParam::GetVar(Function &tmp)
{
  auto get_type = [](AST_Type _tp) -> Type *
  {
    switch (_tp)
    {
    case AST_INT:
      return IntType::NewIntTypeGet();
    case AST_FLOAT:
      return FloatType::NewFloatTypeGet();
    default:
      throw std::invalid_argument("Unknown AST_Type");
    }
  };

  auto GetPointerType = [](Type *innerType) -> Type *
  {
    if (innerType)
    {
      return PointerType::NewPointerTypeGet(innerType);
    }
    return nullptr;
  };

  if (array_subscripts != nullptr)
  {
    auto vec = array_subscripts->GenerateArrayTypeDescriptor(get_type(tp));
    vec = GetPointerType(empty_square ? vec : dynamic_cast<HasSubType *>(vec)->GetSubType());
    tmp.PushParam(name, new Var(Var::Param, vec, name));
  }
  else
  {
    Type *varType = get_type(tp);
    if (empty_square)
    {
      varType = GetPointerType(varType);
    }
    tmp.PushParam(name, new Var(Var::Param, varType, name));
  }
}

FuncParams::FuncParams(FuncParam *ptr)
{
  DataList.push_back(ptr);
}

void FuncParams::push_back(FuncParam *ptr)
{
  DataList.push_back(ptr);
}

void FuncParams::GetVar(Function &tmp)
{
  for (auto &i : DataList)
  {
    i->GetVar(tmp);
  }
}

FuncDef::FuncDef(AST_Type _tp, std::string _name, FuncParams *_params, Block *_func_body)
    : tp(_tp), name(_name), params(_params), func_body(_func_body) {}

void FuncDef::codegen()
{
  auto get_type = [](AST_Type _tp)
  {
    switch (_tp)
    {
    case AST_INT:
      return IR_DataType::IR_Value_INT;
    case AST_FLOAT:
      return IR_DataType::IR_Value_Float;
    case AST_VOID:
      return IR_DataType::IR_Value_VOID;
    default:
      std::cerr << "Wrong Type\n";
      assert(0);
    }
  };
  // std::cout << name << "hhhhhh";
  auto &func = Singleton<Module>().GenerateFunction(get_type(tp), name);
  Singleton<Module>().layer_increase();

  if (params != nullptr)
    params->GetVar(func);

  assert(func_body != nullptr);
  GetInstState state = {func.GetFront(), nullptr, nullptr};
  auto end_block = func_body->GetInst(state);

  if (!end_block->IsEnd())
  {
    if (func.GetType()->GetTypeEnum() == IR_Value_VOID)
      end_block->GenerateRetInst();
    else
    {
      if (func.GetName() == "main")
        end_block->GenerateRetInst(ConstIRInt::GetNewConstant(0));
      else
        end_block->GenerateRetInst(UndefValue::Get(func.GetType()));
    }
  }

  Singleton<Module>().layer_decrease();
}

Block::Block(BlockItems *ptr) : items(ptr) {}

BasicBlock *Block::GetInst(GetInstState state)
{
  if (items == nullptr)
    return state.cur_block;

  Singleton<Module>().layer_increase();
  auto tmp = items->GetInst(state);
  Singleton<Module>().layer_decrease();
  return tmp;
}

BlockItems::BlockItems(Stmt *ptr) { DataList.push_back(ptr); }

void BlockItems::push_back(Stmt *ptr) { DataList.push_back(ptr); }

BasicBlock *BlockItems::GetInst(GetInstState state)
{
  for (auto &i : DataList)
  {
    state.cur_block = i->GetInst(state);
    if (state.cur_block->IsEnd())
      return state.cur_block;
  }
  return state.cur_block;
}

LVal::LVal(std::string _name, Exps *ptr) : name(_name), array_descriptor(ptr) {}

std::string LVal::GetName() { return name; }

Operand LVal::GetPointer(BasicBlock *block)
{
  auto ptr = Singleton<Module>().GetValueByName(name);
  if (ptr->isConst())
    return ptr;

  auto ptrType = dynamic_cast<PointerType *>(ptr->GetType());
  if (!ptrType)
  {
    assert(false && "Invalid pointer type");
    return ptr;
  }

  auto subType = ptrType->GetSubType();
  std::vector<Operand> tmp;
  if (array_descriptor != nullptr)
  {
    tmp = array_descriptor->GetVisitDescripter(block);
  }

  Operand handle;
  if (subType->GetTypeEnum() == IR_ARRAY)
  {
    handle = block->GenerateGepInst(ptr);
    dynamic_cast<GepInst *>(handle)->add_use(ConstIRInt::GetNewConstant());
  }
  else if (subType->GetTypeEnum() == IR_PTR)
  {
    if (array_descriptor == nullptr)
      return ptr;
    handle = block->GenerateLoadInst(ptr);
  }
  else
  {
    assert(tmp.empty());
    return ptr;
  }

  for (auto &i : tmp)
  {
    if (auto gep = dynamic_cast<GepInst *>(handle))
    {
      gep->add_use(i);
    }
    else
    {
      handle = block->GenerateGepInst(handle);
      dynamic_cast<GepInst *>(handle)->add_use(i);
    }
    if (i != tmp.back() && dynamic_cast<PointerType *>(handle->GetType())->GetSubType()->GetTypeEnum() == IR_PTR)
    {
      block->GenerateLoadInst(handle);
    }
  }

  return handle;
}

Operand LVal::GetOperand(BasicBlock *block)
{
  auto ptr = GetPointer(block);
  if (ptr->isConst())
    return ptr;

  if (auto gep = dynamic_cast<GepInst *>(ptr))
  {
    if (dynamic_cast<PointerType *>(gep->GetType())->GetSubType()->GetTypeEnum() == IR_ARRAY)
    {
      gep->add_use(ConstIRInt::GetNewConstant());
      return gep;
    }
  }

  return block->GenerateLoadInst(ptr);
}

AssignStmt::AssignStmt(LVal *m, AddExp *n)
    : lval(std::unique_ptr<LVal>(std::move(m))), exp(std::unique_ptr<AddExp>(std::move(n))) {}

BasicBlock *AssignStmt::GetInst(GetInstState state)
{
  Operand tmp = exp->GetOperand(state.cur_block);
  auto valueptr = lval->GetPointer(state.cur_block);
  state.cur_block->GenerateStoreInst(tmp, valueptr);
  return state.cur_block;
}

ExpStmt::ExpStmt(AddExp *ptr)
    : exp(std::unique_ptr<AddExp>(std::move(ptr))) {}

BasicBlock *ExpStmt::GetInst(GetInstState state)
{
  if (exp != nullptr)
  {
    Operand tmp = exp->GetOperand(state.cur_block);
  }
  return state.cur_block;
}

WhileStmt::WhileStmt(LOrExp *p1, Stmt *p2)
    : cond(std::unique_ptr<LOrExp>(std::move(p1))), stmt(std::unique_ptr<Stmt>(std::move(p2))) {}

BasicBlock *WhileStmt::GetInst(GetInstState state)
{
  auto condition_part = state.cur_block->GenerateNewBlock("wc" + std::to_string(begin));
  auto inner_loop = state.cur_block->GenerateNewBlock("wloop." + std::to_string(begin) + "." + std::to_string(end));
  auto nxt_block = state.cur_block->GenerateNewBlock("wn" + std::to_string(begin));

  state.cur_block->GenerateUnCondInst(condition_part); // 跳条件块

  cond->GetOperand(condition_part, inner_loop, nxt_block); // 条件跳循环或出口

  // 生成循环体
  GetInstState loop_state = {inner_loop, nxt_block, condition_part};
  inner_loop = stmt->GetInst(loop_state);
  if (inner_loop && !inner_loop->GetValUseList().is_empty())
  {
    if (!inner_loop->IsEnd())
      inner_loop->GenerateUnCondInst(condition_part); // 循环跳回条件
  }
  else
  {
    delete inner_loop;
    inner_loop = nullptr;
  }

  // 如果出口没人用，删掉
  if (nxt_block->GetValUseList().is_empty())
  {
    delete nxt_block;
    return state.cur_block;
  }

  return nxt_block;
}

IfStmt::IfStmt(LOrExp *p0, Stmt *p1, Stmt *p2)
    : cond(std::unique_ptr<LOrExp>(std::move(p0))), true_stmt(p1), false_stmt(p2) {}

BasicBlock *IfStmt::GetInst(GetInstState state)
{
  BasicBlock *nextBlock = nullptr;
  auto trueBlock = state.cur_block->GenerateNewBlock();
  BasicBlock *falseBlock = nullptr;
  GetInstState t_state = state;
  t_state.cur_block = trueBlock;
  if (false_stmt != nullptr)
    falseBlock = state.cur_block->GenerateNewBlock();

  if (falseBlock != nullptr)
    cond->GetOperand(state.cur_block, trueBlock, falseBlock);
  else
  {
    nextBlock = state.cur_block->GenerateNewBlock();
    cond->GetOperand(state.cur_block, trueBlock, nextBlock);
  }

  auto deleteUnusedBlock = [](BasicBlock *&tmp)
  {
    if (tmp == nullptr)
      return;
    if (tmp->GetValUseList().is_empty())
    {
      delete tmp;
      tmp = nullptr;
    }
  };

  deleteUnusedBlock(trueBlock);
  deleteUnusedBlock(falseBlock);
  deleteUnusedBlock(nextBlock);

  auto makeUncondJump = [&](BasicBlock *tmp)
  {
    if (!tmp->IsEnd())
    {
      if (nextBlock == nullptr)
        nextBlock = state.cur_block->GenerateNewBlock();
      tmp->GenerateUnCondInst(nextBlock);
    }
  };

  if (trueBlock != nullptr)
  {
    trueBlock = true_stmt->GetInst(t_state);
    makeUncondJump(trueBlock);
  }
  if (falseBlock != nullptr)
  {
    GetInstState f_state = state;
    f_state.cur_block = falseBlock;
    falseBlock = false_stmt->GetInst(f_state);
    makeUncondJump(falseBlock);
  }

  return (nextBlock == nullptr ? state.cur_block : nextBlock);
}

BasicBlock *BreakStmt::GetInst(GetInstState state)
{
  state.cur_block->GenerateUnCondInst(state.break_block);
  return state.cur_block;
}

BasicBlock *ContinueStmt::GetInst(GetInstState state)
{
  state.cur_block->GenerateUnCondInst(state.continue_block);
  return state.cur_block;
}

ReturnStmt::ReturnStmt(AddExp *ptr) : ret_val(std::unique_ptr<AddExp>(std::move(ptr))) {}

BasicBlock *ReturnStmt::GetInst(GetInstState state)
{
  if (ret_val != nullptr)
  {
    auto ret = ret_val->GetOperand(state.cur_block);
    state.cur_block->GenerateRetInst(ret);
  }
  else
  {
    state.cur_block->GenerateRetInst();
  }
  return state.cur_block;
}