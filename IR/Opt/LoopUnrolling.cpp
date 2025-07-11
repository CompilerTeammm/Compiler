#include "../../include/IR/Opt/LoopUnrolling.hpp"
#include <memory>

bool LoopUnrolling::run()
{
  DominantTree dom(_func);
  dom.BuildDominantTree(); // 计算支配关系
  for(auto e:*_func)
  {
    auto vec = dom.getIdomVec(e);
  }

  // auto loopTest = std::make_shared<LoopInfoAnalysis>(_func, &dom, DeleteLoop);
  LoopInfoAnalysis *loopAnalysis = new LoopInfoAnalysis(_func, &dom, DeleteLoop);
  std::vector<Loop *> loopAnalysis_Unroll{loopAnalysis->loopsBegin(), loopAnalysis->loopsEnd()};

  for (auto iter = loopAnalysis_Unroll.begin(); iter != loopAnalysis_Unroll.end();)
  {
    auto currLoop = *iter;
    ++iter;
    if (!CanBeUnroll(currLoop))
    {
      continue; // 跳过不可展开的循环
    }
    auto unrollbody = GetLoopBody(currLoop);
    if (unrollbody)
    {
      auto bb = Unroll(currLoop, unrollbody);
      CleanUp(currLoop, bb);
      return true;
    }
  }
  return false;
}

bool LoopUnrolling::CanBeUnroll(Loop *loop)
{
  auto body = loop->getLoopBody();
  auto header = loop->getHeader();
  auto latch = loopAnalysis->getLatch(loop);
  if (header != latch)
    return false;
  if (!dynamic_cast<ConstIRInt *>(loop->trait.initial) || !dynamic_cast<ConstIRInt *>(loop->trait.boundary))
    return false;

  int Lit_count = 0;
  auto initial = dynamic_cast<ConstIRInt *>(loop->trait.initial)->GetVal();
  auto bound = dynamic_cast<ConstIRInt *>(loop->trait.boundary)->GetVal();
  auto bin = dynamic_cast<BinaryInst *>(loop->trait.change); //
  auto op = bin->GetOp();
  auto step = loop->trait.step;

  switch (op)
  {
  case BinaryInst::Op_Add:
    Lit_count = (bound - initial + step + (step > 0 ? -1 : 1)) / step;
    break;
  case BinaryInst::Op_Sub:
    Lit_count = (initial - bound + step + (step > 0 ? -1 : 1)) / step;
    break;
  case BinaryInst::Op_Mul:
    Lit_count = std::log(bound / initial) / std::log(step);
    break;
  case BinaryInst::Op_Div:
    Lit_count = std::log(initial / bound) / std::log(step);
    break;
  default:
    assert(0 && "循环展开,步长op,不支持的操作符");
  }

  int cost = CaculatePrice(body, _func, Lit_count);
  if (cost > MaxInstCost)
    return false;
  return true;
}

CallInst *LoopUnrolling::GetLoopBody(Loop *loop)
{
  ///// 识别循环变量
  // 循环分析
  auto body = loop->getLoopBody();
  auto header = loop->getHeader();
  auto latch = loop->getLatch();
  auto preheader = loop->getPreHeader();

  // 处理循环头部，获得Phi函数节点
  auto phi = dynamic_cast<PhiInst *>(header->GetLastInsts());
  if (phi == nullptr || phi->getNumIncomingValues() != 2)
  {
    return nullptr;
  }

  std::vector<PhiInst *> validPhis;
  for (auto inst : *header)
  {
    auto phi = dynamic_cast<PhiInst *>(inst);
    if (!phi)
      break;
    if (inst == loop->trait.indvar)
      continue;
    validPhis.push_back(phi);
  }
  if (validPhis.size() > 2) // 结点过多就得报错
  {
    return nullptr;
  }
  /*  PhiInst *res = nullptr;
   int count = 0;
   for (auto inst : *header)
   {
     if (auto phi = dynamic_cast<PhiInst *>(inst))
     {
       count++;
       if (inst == loop->trait.indvar) // 归纳变量
         continue;                     // 与循环条件和步长有关,无需处理
       res = phi;
       if (count > 2)
       {
         _DEBUG(std::cerr << "too many phi" << std::endl;)
         return nullptr;
       }
     }
     else
     {
       break;
     }
   } */
  loop->trait.res = validPhis[0];
  auto res = validPhis[0];
  // loop->trait.indvar = dynamic_cast<PhiInst *>(header->GetLastInsts());
  // loop->trait.call = dynamic_cast<CallInst *>(header->GetLastInsts());
  // 创建新的函数体用于存放展开后的循环体
  // 获取函数类型、创建函数
  IR_DataType ty = IR_DataType::IR_Value_VOID; // 默认返回void
  if (res)
    ty = res->GetTypeEnum();
  auto unrollFunc = new Function(ty, "loop_unroll"); // 构造函数loop_unroll
  // module实例下管理一个函数列表
  Singleton<Module>().push_func(std::unique_ptr<Function>(unrollFunc));
  // 设置函数的参数，初始化
  unrollFunc->clear();                    // 清空可能存在的默认内容
  unrollFunc->tag = Function::UnrollBody; // 标记为循环展开生成的函数
  // 获取 i
  auto indvar = loop->trait.indvar;
  Val2Arg[indvar] = new Var(Var::Param, indvar->GetType(), "");
  if (res)
    Val2Arg[res] = new Var(Var::Param, res->GetType(), "");

  // 定义在循环外部但是在循环内实用的
  // 需要作为参数传入循环展开的变量
  auto Judge = [&](Value *target)
  {
    // 去除基本块和函数和内置函数（不需要修改）
    if (dynamic_cast<BasicBlock *>(target))
      return false;
    if (dynamic_cast<Function *>(target))
      return false;
    // 去除内置函数
    auto targetName = target->GetName();
    if (targetName == "getint" || targetName == "getch" || targetName == "getfloat" ||
        targetName == "getfarray" || targetName == "putint" || targetName == "putfloat" ||
        targetName == "putarray" || targetName == "putfarray" || targetName == "putf" ||
        targetName == "getarray" || targetName == "putch" || targetName == "_sysy_starttime" ||
        targetName == "_sysy_stoptime" || targetName == "llvm.memcpy.p0.p0.i32")
      return false;
    if (targetName == "llvm.memset.p0.i32" || targetName == "llvm.memmove.p0.p0.i32")
      return false;
    if (targetName == "llvm.lifetime.start" || targetName == "llvm.lifetime.end")
      return false;
    if (targetName == "llvm.dbg.declare" || targetName == "llvm.dbg.value")
      return false;
    if (targetName == "llvm.dbg.value" || targetName == "llvm.dbg.declare")
      return false;
    if (Val2Arg.count(target)) // 如果 target 已经在 Val2Arg 中
      return false;
    // 如果是变量或者全局变量
    if (target->isConst() || target->isGlobal())
      return false;
    auto user = dynamic_cast<User *>(target);
    assert(user);
    // if (loop->ContainBB(user->....->GetParent()))
    //   return false;
    return true;
  };

  for (auto basic_block : body)
  {
    for (auto instruction : *basic_block)
    {
      if (Val2Arg.count(instruction))
        continue;
      for (int i = 0; i < instruction->GetUserUseListSize(); i++)
      {
        auto operand = instruction->GetOperand(i);
        if (Judge(operand)) // 判断操作数是否需要参数化
          Val2Arg[operand] = new Var(Var::Param, operand->GetType(), "");
      }
    }
  }

  /*
  int sum = 0;
  for (int i = 0; i < n; i++)
  {
    sum += array[i]; // 依赖外部变量 array 和 n
  }
    */
  for (const auto &[val, arg] : Val2Arg)
    unrollFunc->PushParam(val->GetName(), arg);
  /*
    int sum = 0;
    int *array = ...;
    int n = ...;
    for (int i = 0; i < n; i++)
    {
      sum += array[i];
    }
    */
  // 创建函数体的基本块、移动phi函数结点到newheader
  auto newHeader = new BasicBlock();
  newHeader->SetName(header->GetName() + ".newHeader");
  _func->AddBBs(newHeader);
  for (auto it = header->begin(); it != header->end();)
  {
    auto inst = *it; // 获取当前指令
    ++it;            // 提前递增迭代器（避免失效）
    if (dynamic_cast<PhiInst *>(inst))
    {
      inst->Node::EraseFromManager(); // 从原header移除
      newHeader->push_back(inst);     // 添加到替代块
    }
    else
    {
      break; // 遇到非PHI节点立即终止
    }
  }

  // 通过新的callinst来调用这个unroll函数，参数化调用
  std::vector<Value *> args;
  for (const auto &[val, arg] : Val2Arg)
    args.push_back(val);
  auto callinst = new CallInst(unrollFunc, args);
  newHeader->push_back(callinst);

  // 复制并替换原有循环中的关键状态变量
  // 克隆一个与循环状态变化相关的指令（如 PHI 节点）
  // 插入到新的基本块中，并将循环结构指向这个新指令
  auto loopchange = dynamic_cast<User *>(loop->trait.change);
  assert(loopchange && "Expected a User instruction for change");
  auto change = loopchange->CloneInst();
  newHeader->push_back(dynamic_cast<Instruction *>(change));
  loop->trait.change = change;

  // 获取latch块中的条件判断指令
  auto loopcmp = dynamic_cast<User *>(latch->GetBack());
  assert(loopcmp && "clone latch failed");
  auto cmp = loopcmp->CloneInst();
  newHeader->push_back(dynamic_cast<Instruction *>(cmp));

  // 从 latch 块中提取原始的条件跳转逻辑
  // 复用它的判断条件，并创建一个新的跳转指令指向退出块
  BasicBlock *exit = nullptr;
  if (auto cond = dynamic_cast<CondInst *>(latch->GetLastInsts()))
  {
    for (int i = 1; i < 3; i++)
    {
      if (cond->GetOperand(i) != latch)
        exit = dynamic_cast<BasicBlock *>(cond->GetOperand(i));
    }
  }
  auto newcmp = new CondInst(cmp, newHeader, exit);
  newHeader->push_back(newcmp);

  // 转换use和user链条
  for (auto &use : cmp->GetUserUseList())
  {
    if (use->GetValue() == loopchange)
      cmp->Use2Value(use.get(), change);
  }

  // 将原有循环头部的后继块指向新的基本块
  for (auto now : _dom->getPredBBs(header))
  {
    if (now != latch)
    {
      if (auto cond = dynamic_cast<CondInst *>(now->GetLastInsts()))
      {
        // 替换条件指令中的操作数
        for (int i = 1; i < 3; i++)
        {
          if (cond->GetOperand(i) == header)
            cond->SetOperand(i, newHeader);
        }
      }
      else if (auto uncond = dynamic_cast<UnCondInst *>(now->GetLastInsts()))
      {
        uncond->SetOperand(0, newHeader);
      }
    }
  }

  // 更新所有 Phi 指令中来自 latch 块的记录
  // 指向新的 newHeader 块和对应的值
  // 遍历latch的后继结点
  bool NoUse = false;
  for (auto des : _dom->getSuccBBs(latch))
  {
    if (des != header) // 只关心非 header 的下游块,比如 exit
    {
      for (auto it = des->begin(); it != des->end() && dynamic_cast<PhiInst *>(*it); ++it) // 遍历当前块开头的所有 Phi 指令
      {
        auto phi = dynamic_cast<PhiInst *>(*it);
        struct Finder
        {
          BasicBlock *latch;
          Finder(BasicBlock *latch) : latch(latch) {}
          bool operator()(const std::pair<int, std::pair<Value *, BasicBlock *>> &ele)
          {
            return ele.second.second == latch;
          }
        };

        auto it1 = std::find_if(
            phi->PhiRecord.begin(),
            phi->PhiRecord.end(),
            Finder(latch) // 传入latch构造函子
        );
        // auto it1 = std::find_if(phi->PhiRecord.begin(), phi->PhiRecord.end(), [latch](const std::pair<int, std::pair<Value *, BasicBlock *>> &ele)
        //                         { return ele.second.second == latch; });
        if (it1 != phi->PhiRecord.end()) // 有一条输入来自于循环的 latch 块
        {
          it1->second.second = newHeader;
          if (it1->second.first == loopchange)
          {
            phi->SetOperand(it1->first, change);
            it1->second.first = change;
          }
        }
        // 更新PHI节点中来自 latch 块的输入值，使其指向 callinst 的结果
        else if (res && it1->second.first == res->ReturnValIn(latch))
        {
          phi->SetOperand(it1->first, callinst);
          it1->second.first = callinst;
        }
        // 当前 Phi 指令的一个输入值是一个定义在 latch 块中的 Phi 指令
        else if (!res && dynamic_cast<PhiInst *>(it1->second.first))
        {
          auto _res = dynamic_cast<PhiInst *>(it1->second.first);
          if (_res->GetParent() == latch)
          {
            res = _res;
            unrollFunc->SetType(res->GetType());
            callinst->SetType(res->GetType());
            _res->SetOperand(it1->first, callinst);
            it1->second.first = callinst;
            NoUse = true;
          }
        }
        else
        {
          assert(0);
        }
      }
    }
  }

  // 根据循环结果变量 res 和标志位 NoUse 的状态，构造一个合适的返回指令（RetInst）；
  // 替换掉 latch 块中的最后一条指令（原本可能是无条件跳转或其他指令），插入新的 RetInst
  // 需要让这个函数返回正确的值
  RetInst *ret = nullptr;
  if (res && !NoUse) // res 是一个 Phi 指令，并且它在 latch 块中有一个输入值
  {
    ret = new RetInst(res->ReturnValIn(latch));
  }
  /*
  % sum.next = phi[% sum.add, % body], [ % init, % preheader ];
  在 latch 中返回 % sum.next 的值 ret i32 % sum.next
  */
  else if (res && NoUse) // res 是直接定义在 latch 块中的 Phi 指令
  {
    ret = new RetInst(res);
  }
  /*
  latch:
    % res = phi[% callinst, % substitute], [ undef, % header ] ret i32 % res
  */
  else
  {
    ret = new RetInst();
  }
  delete latch->GetBack();
  latch->push_back(ret);

  for (auto it = newHeader->begin(); it != newHeader->end() && dynamic_cast<PhiInst *>(*it); ++it)
  {
    auto phi = dynamic_cast<PhiInst *>(*it);
    for (int i = 0; i < phi->PhiRecord.size(); i++)
    {
      if (phi->PhiRecord[i].second == latch)
      {
        phi->Del_Incomes(i--);
        phi->FormatPhi();
      }
    }
  }

  for (auto bb : body)
  {
    for (auto inststruction : *bb)
    {
      for (auto &use : inststruction->GetUserUseList())
      {
        if (Val2Arg.find(use->GetValue()) != Val2Arg.end())
        {
          if (auto phi = dynamic_cast<PhiInst *>(inststruction))
            phi->ReplaceVal(use.get(), Val2Arg[use->GetValue()]);
          else
            inststruction->Use2Value(use.get(), Val2Arg[use->GetValue()]);
        }
      }
    }
  }

  if (res && !NoUse)
    res->addIncoming(callinst, newHeader);
  loop->trait.indvar->addIncoming(loop->trait.change, newHeader);
  newHeader->num = header->num;
  // _func->GetBBs() = newHeader;
  // _dom->getNode(header) = newHeader;
  std::vector<BasicBlock *> tmp{body.begin(), body.end()};
  for (auto iter = tmp.begin(); iter != tmp.end();)
  {
    auto bb = *iter;
    ++iter;
    bb->EraseFromManager();
    unrollFunc->PushBothBB(bb);
    if (bb != header)
      loopAnalysis->deleteBB(bb); // 删除原有循环体中的基本块
    else
    {
      loopAnalysis->newBB(bb, newHeader);
      loop->addHeader(newHeader);
      loopAnalysis->setLoop(newHeader, loop);
    }
  }
  return callinst;
}

BasicBlock *LoopUnrolling::Unroll(Loop *loop, CallInst *UnrollBody)
{
  // 1. 验证和提取循环特征
  assert(loop && UnrollBody && "Invalid arguments");
  auto &trait = loop->trait;
  assert(trait.initial && trait.boundary && trait.change && "Invalid loop traits");

  // 2. 获取循环边界值
  int initial_value = dynamic_cast<ConstIRInt *>(trait.initial)->GetVal();
  int boundary_value = dynamic_cast<ConstIRInt *>(trait.boundary)->GetVal();
  int step_value = trait.step;

  // 3. 验证步长指令
  auto *step_inst = dynamic_cast<BinaryInst *>(trait.change);
  assert(step_inst && "Step instruction must be binary");

  // 4. 计算循环迭代次数
  int loop_iterations = 0;
  switch (step_inst->GetOp())
  {
  case BinaryInst::Op_Add:
    loop_iterations = (boundary_value - initial_value + step_value + (step_value > 0 ? -1 : 1)) / step_value;
    break;
  case BinaryInst::Op_Sub:
    loop_iterations = (initial_value - boundary_value + step_value + (step_value > 0 ? -1 : 1)) / step_value;
    break;
  case BinaryInst::Op_Mul:
    loop_iterations = static_cast<int>(std::log(boundary_value / initial_value) / std::log(step_value));
    break;
  case BinaryInst::Op_Div:
    loop_iterations = static_cast<int>(std::log(initial_value / boundary_value) / std::log(step_value));
    break;
  default:
    assert(false && "Unsupported operation type");
  }

  // 5. 获取退出块
  auto exit_blocks = loopAnalysis->getExitingBlocks(loop);
  assert(exit_blocks.size() == 1 && "Only one exit block supported");
  BasicBlock *exit_block = exit_blocks[0];

  // 6. 初始化参数映射
  std::unordered_map<Value *, Value *> ParamToOriginal;
  Value *inductionOrigin = trait.initial;
  Value *resultOrigin = trait.res ? trait.res->ReturnValIn(prehead) : nullptr;

  // 7. 参数映射函数
  auto mapArguments = [&](User *callInst, Value *inductionVal, Value *resultVal)
  {
    for (int idx = 1; idx < callInst->GetUserUseListSize(); ++idx)
    {
      Value *operand = callInst->GetOperand(idx);
      if (auto *phiNode = dynamic_cast<PhiInst *>(operand))
      {
        ParamToOriginal[operand] = (phiNode == trait.indvar) ? inductionVal : (phiNode == trait.res) ? resultVal
                                                                                                     : operand;
      }
      else
      {
        ParamToOriginal[operand] = operand;
      }
    }
  };

  // 8. 初始化映射
  mapArguments(UnrollBody, inductionOrigin, resultOrigin);

  // 9. 准备展开
  BasicBlock::iterator insert_point(UnrollBody);
  std::vector<User *> to_erase_list = {UnrollBody};
  User *current_call = UnrollBody;
  Value *current_result = nullptr;
  BasicBlock *last_block = nullptr;

  // 10. 执行循环展开
  for (int round = 0; round < loop_iterations; ++round)
  {
    // 内联当前调用
    auto inline_result = _func->InlineCall(dynamic_cast<CallInst *>(current_call), ParamToOriginal);

    current_result = inline_result.first;
    last_block = inline_result.second;
    Value *current_step_value = ParamToOriginal[inductionOrigin];

    // 准备下一轮
    ParamToOriginal.clear();
    User *next_call = current_call->CloneInst();
    to_erase_list.push_back(next_call);

    // 插入新调用并更新映射
    auto next_call_ = dynamic_cast<Instruction *>(next_call);
    insert_point = insert_point.InsertAfter(next_call_);
    mapArguments(next_call, current_step_value, current_result);
  }

  // 11. 获取目标函数
  Function *target_func = dynamic_cast<Function *>(UnrollBody->GetOperand(0));

  // 12. 清理和优化
  if (resultOrigin)
  {
    UnrollBody->ReplaceAllUseWith(resultOrigin);
  }

  // 清理归纳变量
  if (trait.indvar)
  {
    trait.indvar->ReplaceAllUseWith(UndefValue::Get(trait.indvar->GetType()));
    delete trait.indvar;
  }

  // 清理结果变量
  if (trait.res)
  {
    trait.res->ReplaceAllUseWith(UndefValue::Get(trait.res->GetType()));
    delete trait.res;
  }

  // 删除临时指令
  for (auto *inst : to_erase_list)
  {
    delete inst;
  }

  // 移除内联函数
  Singleton<Module>().EraseFunction(target_func);

  return last_block;
}

int LoopUnrolling::CaculatePrice(std::vector<BasicBlock *> body, Function *curfunc, int Lit_count)
{
  int cost = 0;
  for (auto bb : body)
  {
    for (auto inst : *bb)
    {
      if (dynamic_cast<LoadInst *>(inst) || dynamic_cast<StoreInst *>(inst))
      {
        cost += 4;
      }
      else if (dynamic_cast<GepInst *>(inst))
      {
        cost += 2;
      }
      else if (dynamic_cast<CallInst *>(inst))
      {
        auto call = dynamic_cast<Function *>(inst->GetOperand(0));
        if (call)
        {
          if (call == curfunc || call->tag == Function::ParallelBody || call->tag == Function::BuildIn)
            return MaxInstCost + 1;
          cost += CaculatePrice(body, call, Lit_count);
        }
        else
        {
          cost += 2;
        }
      }
      else
      {
        cost++;
      }
    }
  }
  return cost * Lit_count;
}

void LoopUnrolling::CleanUp(Loop *loop, BasicBlock *clean)
{
  auto cond = dynamic_cast<CondInst *>(clean->GetBack());
  auto Inloop = dynamic_cast<BasicBlock *>(cond->GetOperand(1));
  auto Outloop = dynamic_cast<BasicBlock *>(cond->GetOperand(2));
  if (loop->ContainBB(Outloop))
    std::swap(Inloop, Outloop);
  auto uncond = new UnCondInst(Outloop);
  delete cond;
  clean->push_back(uncond);
  loopAnalysis->deleteLoop(loop);
}