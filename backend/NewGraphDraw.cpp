#include "../include/Backend/RISCVFrameContext.hpp"
#include "../include/Backend/RISCVMIR.hpp"
#include "../include/Backend/RISCVMOperand.hpp"
#include "../include/Backend/RISCVRegister.hpp"
#include "../include/Backend/RISCVType.hpp"
#include "../include/Backend/RegAlloc.hpp"
#include "../util/my_stl.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <unordered_set>
#include <unordered_map>
//入口
void GraphColor::RunOnFunc(){
    bool condition=true;
    GC_init();//初始化图着色相关结构
    for(auto b:*m_func){
        CalCulateSucc(b);//计算每个基本块后继用于构建控制流图
    }
    CaculateTopu(m_func->front());//对基本块做拓补排序,计算支配关系等
    std::reverse(topu.begin(),topu.end());//逆序方便数据流分析
    while (condition) {
        condition = false;
        CaculateLiveness();//活跃变量分析,构建干涉图
        MakeWorklist();//将变量分到不同的工作集里（简化、冻结、合并、溢出）
        do {
          if (!simplifyWorkList.empty()){
            simplify();// 简化图，删除度数小于寄存器数的节点    
          }else if (!worklistMoves.empty()){
            coalesce();// 合并move相关的变量，减少move指令
          }else if (!freezeWorkList.empty()){
            freeze();// 冻结move指令相关的节点，改为普通节点
          }else if (!spillWorkList.empty()){
            spill();// 溢出某个变量到内存
          }
        } while (!simplifyWorkList.empty() || !worklistMoves.empty() ||!freezeWorkList.empty() || !spillWorkList.empty());
        for (auto sp : SpillStack) {//这是处理spilled nodes 的部分
          auto it = std::find(selectstack.begin(), selectstack.end(), sp);//看它是否在着色选择栈中
          if (it == selectstack.end())
            assert(0);//如果找不到,那就不合理,
          selectstack.erase(it);//移除,因为接下来要调整溢出变量的优先级,放在栈的""底部"
        }
        std::reverse(SpillStack.begin(), SpillStack.end());
        //为什么要反转？ 因为 selectstack 是栈结构（后进先出），我们希望 spill 的变量 优先级低，放在最后被处理，所以要反转它的顺序。
        selectstack.insert(selectstack.end(), SpillStack.begin(), SpillStack.end());
        // 把所有 spill 的变量重新加回 selectstack 的栈底（末尾）。
        // 作用是：
        // 确保 spill 的变量出现在颜色选择过程中。
        // 但因为它们优先级最低，会最后尝试分配寄存器。
        AssignColors();//实际分配颜色(物理寄存器)
        if (!spilledNodes.empty()) {
          SpillNodeInMir();// 将spill节点在MIR中插入load/store指令
          condition = true;//由于插入了新的虚拟寄存器，要重新图着色
        }
      }
      RewriteProgram();//将寄存器分配的结果写入MIR
}

void GraphColor::MakeWorklist(){
    for(auto node:initial){
        //添加溢出节点
        if(Degree[node] > GetRegNums(node)){
            spillWorkList.insert(node);//度数过高，先假设溢出
        }else if(MoveRelated(node).size()!=0){
            freezeWorkList.insert(node);//与move指令有关，暂时冻结
        }else{
            simplifyWorkList.push_back(node);//可以直接简化的节点
        }
    }
}
//判断某个变量是否与尚未处理的 move 指令有关。
std::unordered_set<RISCVMIR*> GraphColor::MoveRelated(MOperand v){
  std::unordered_set<RISCVMIR*> tmp;
  if(moveList.find(v)==moveList.end()){
    return tmp;
  }
  for(auto inst:moveList[v]){
    auto iter=std::find(worklistMoves.begin(),worklistMoves.end(),inst);
    if(activeMoves.find(inst)!=activeMoves.end()||iter!=worklistMoves.end()){
      tmp.insert(inst);//还未完成，加到tmp中后续处理
    }
  }
  return tmp;
}

//查找一个变量合并后的代表节点（类似并查集）
MOperand GraphColor::GetAlias(MOperand v){
  //当前v存在于coalescedNodes，则返回v的alias
  if(coalescedNodes.find(v)!=coalescedNodes.end()){
    return GetAlias(alias[v]);
    // 向上传递直到找到根节点
  }else{
    return v;
  }
}

// 合并 src 和 dst 可以省去 mov dst, src 这样的指令；
// 但合并后可能导致寄存器分配失败（图着色冲突）；
// 所以我们引入两个启发式判断方法：

// George：强调 src 的邻居对 dst 是否友好（安全）；

// Briggs：评估合并后整体邻居中度数过高的数量是否可控。
bool GraphColor::GeorgeCheck(MOperand dst,MOperand src,RISCVType type){
  //George 启发式适用于 src 是普通节点、dst 是预着色节点。
  if(type==riscv_i32||type==riscv_ptr||type==riscv_i64){
    auto x=Adjacent(src);//获取src的邻接节点
    for(auto tmp:x){
      // bool ok=false;
      // if(Degree[tmp] < reglist.GetReglistInt().size()){
      //   ok|=true;
      // }
      // if(Precolored.find(tmp) != Precolored.end()){
      //   ok|=true;
      // }
      // auto &adj=adjSet[dst];
      // if(adj.find(tmp)!=Precolored.end()){
      //   ok|=true;
      // }
      // if(ok!=true){
      //   return false;
      // }
      //等价简洁版：

      // 判断当前 tmp 是否对 dst 是“安全”的邻居：
      // 满足任意一个条件即可：
      // 1. tmp 的度数小于可用寄存器数 ⇒ 易于着色
      // 2. tmp 是预着色的（物理寄存器） ⇒ 不冲突
      // 3. tmp 与 dst 已经相邻 ⇒ 不会新增冲突边
      auto &adj = adjSet[dst];
      if (!(Degree[tmp] < reglist.GetReglistInt().size() ||
            Precolored.find(tmp) != Precolored.end() ||
            adj.find(tmp) != adj.end())){
          return false; // 只要有一个邻居不安全，就不能合并
      }
    }
  }else if(type==riscv_float32){
    auto x=Adjacent(src);
    for(auto tmp:x){
      // bool ok = false;
      // if (Degree[tmp] < reglist.GetReglistFloat().size())
      //   ok |= true;
      // if (Precolored.find(tmp) != Precolored.end())
      //   ok |= true;
      // auto &adj = adjSet[dst];
      // if (adj.find(tmp) != adj.end())
      //   ok |= true;
      // if (ok != true)
      //   return false;
      //等价简洁版:
      auto &adj = adjSet[dst];
      if (!(Degree[tmp] < reglist.GetReglistFloat().size() ||
            Precolored.find(tmp) != Precolored.end() ||
            adj.find(tmp) != adj.end())){
          return false;
      }
    }
  }else{
    assert(0 && "type must be either int or float");
  }
  return true;
}

bool GraphColor::BriggsCheck(MOperand dst,MOperand src,RISCVType type){
  auto tmp=Adjacent(dst);
  std::unordered_set<MOperand> target{tmp.begin(), tmp.end()};
  tmp = Adjacent(src);
  target.insert(tmp.begin(), tmp.end());//此时 target 包含了 dst 和 src 的合并后邻接集合。
  if(type==riscv_i32||type==riscv_ptr||type==riscv_i64){
    int num=0;
    for(auto node : target){
      if(Degree[node]>=reglist.GetReglistInt().size()){
        num++;//记录邻接集合中“冲突严重”的节点数（即度数大于等于寄存器数量的点）。
      }
    }
    return (num<reglist.GetReglistInt().size());
    //如果合并后冲突严重的邻接节点数少于寄存器数 ⇒ 可以安全合并；
    //反之，合并可能导致溢出（spill），不建议合并。
  }else if(type==riscv_float32){
    int num = 0;
    for (auto node : target) {
      if (Degree[node] >= reglist.GetReglistFloat().size()){
        num++;
      }
    }
    return (num < reglist.GetReglistFloat().size());
  }else{
    return true;
  }
}

//将一个非预着色（非物理寄存器）节点 v 加入 simplifyWorkList
void GraphColor::AddWorkList(MOperand v){
  if (Precolored.find(v) == Precolored.end() && Degree[v] < GetRegNums(v) && MoveRelated(v).size() == 0) {
    //如果v不是预着色寄存器(也就不是物理寄存器),也不是高阶节点(度数小于可用寄存器数)且不参与任何move指令(不是合并候选者)
    freezeWorkList.erase(v);
    PushVecSingleVal(simplifyWorkList, v);
    //那就移除,说明不需要冻结,压入s后准备进行简化
  }
}
//用于计算活跃信息的主函数
void GraphColor::CaculateLiveness() {
  LiveInfoInit();//初始化活跃性信息数据结构
  RunOnFunction();//函数级别进行活跃性分析
  //计算IG,并且添加precolored集合
  Build();
  RunOnFunc_();//运行后处理
  for (const auto b : *m_func) {
    CaculateLiveInterval(b);//遍历基本块,建立虚拟寄存器的活跃区间信息
  }
}

//计算活跃区间并合并成全局结果
void GraphColor::CaculateLiveInterval(RISCVBasicBlock *bb){
  auto &IntervInfo=GetRegLiveInterval(bb);
  for(auto &[val, vec] : IntervInfo){
    //遍历所有寄存器val和其在当前块的活跃区间vec
    if (!GlobalLiveRange.count(val)) {
      GlobalLiveRange[val].start = INT32_MAX;
      GlobalLiveRange[val].end = INT32_MIN;
    }//如果这个寄存器还没有全局活跃区间记录,则初始化
    unsigned int length = 0;
    for (auto v : vec) {
      if (v.start < GlobalLiveRange[val].start){
        GlobalLiveRange[val].start = v.start;
      }
      if (v.end > GlobalLiveRange[val].end){
        GlobalLiveRange[val].end = v.end;
      }
      length += v.end - v.start;
    }
    ValsInterval[val] = length;
  }
}
//在 spillWorkList 中选择一个“最适合溢出”的虚拟寄存器，根据多个启发式因素评分：活跃长度、度数、惩罚值。
MOperand GraphColor::HeuristicSpill(){
  float max=0;//记录当前最大权重
  Register *sp=nullptr;
  for(auto spill:spillWorkList){
    auto vspill = dynamic_cast<VirRegister *>(spill);
    float weight=0;//初始化当前节点的综合权重

    //degree
    int degree=Degree[spill];
    if(degree<0){
      assert(0);
    }
    weight=weight+(degree*DegreeWeight);
    //interval
    int intervalLength=ValsInterval[spill];
    weight=weight+(intervalLength * livenessWeight);
    //考虑 penalty 惩罚：spill 和 reload 的开销
    weight=weight+(vspill->GetPenaltySpill() + vspill->GetPenaltyReload()) * SpillWeight;
    //比较替换
    if (max < weight) {
      max = weight;
      sp = spill;
      continue;
    }
  }
  if (!sp)
  for (auto spill : spillWorkList){
    return spill;
  }
  return sp;
}

//启发式选择一个可以冻结的寄存器，目前实现是：直接选第一个节点
MOperand GraphColor::HeuristicFreeze() {
  return *(freezeWorkList.begin()); 
}
void GraphColor::SetRegState(PhyRegister *reg, RISCVType ty) {
  RegType[reg] = ty;//标记物理寄存器类型,在获取类型相关寄存器数量时会用到
}
int GraphColor::GetRegNums(MOperand v) {
  if (v->GetType() == riscv_i32 || v->GetType() == riscv_ptr || v->GetType() == riscv_i64)
    return reglist.GetReglistInt().size();
  else if (v->GetType() == riscv_float32)
    return reglist.GetReglistFloat().size();
  else if (v->GetType() == riscv_none) {
    auto preg = dynamic_cast<PhyRegister *>(v);
    auto tp = RegType[preg];
    assert(tp != RISCVType::riscv_none && "error");
    return (tp == riscv_i32 || tp == riscv_i64) ? reglist.GetReglistInt().size()
                           : reglist.GetReglistFloat().size();
  }
  assert("excetion case");
  return 0;
}
int GraphColor::GetRegNums(RISCVType ty) {
  if (ty == riscv_i32 || ty == riscv_ptr || ty == riscv_i64)
    return reglist.GetReglistInt().size();
  if (ty == riscv_float32)
    return reglist.GetReglistFloat().size();
  assert(0);
}

//节点合并
void GraphColor::combine(MOperand rd,MOperand rs){
  //合并时确保类型一致
  if(rd->GetType() == riscv_none && rs->GetType() != riscv_none){
    RegType[dynamic_cast<PhyRegister *>(rd)] = rs->GetType();
  }else if(rd->GetType() != riscv_none && rs->GetType() == riscv_none){
    RegType[dynamic_cast<PhyRegister *>(rs)] = rd->GetType();
  }
  //合并后，rs 不再是活跃候选节点，所以从 freeze 或 spill 工作列表中移除。
  if (freezeWorkList.find(rs) != freezeWorkList.end()){
    freezeWorkList.erase(rs);
  }else{
    spillWorkList.erase(rs);
  }
  coalescedNodes.insert(rs);
  alias[rs] = rd;
  //更新合并后的movelist,干涉图
  for (auto mv : moveList[rs]) {
    moveList[rd].insert(mv);
  }
  for (auto mov : MoveRelated(rs)){
    if (activeMoves.find(mov) != activeMoves.end()) {
      activeMoves.erase(mov);
      PushVecSingleVal(worklistMoves, mov);
    }
  }
  auto t = Adjacent(rs);
  std::unordered_set<MOperand> tmp(t.begin(), t.end());
  // EnableMove
  for (auto node : AdjList[rs]) {
    // Add Edge
    AddEdge(node, rd);
    DecrementDegree(node);
  }
  if (Degree[rd] >= GetRegNums(rs) &&
      (freezeWorkList.find(rd) != freezeWorkList.end())) {
    freezeWorkList.erase(rd);
    spillWorkList.insert(rd);
  }
}

//将某个变量 freeze 所关联的所有 move 指令标记为 frozen（不再尝试合并），并更新工作列表。
void GraphColor::FreezeMoves(MOperand freeze) {
  auto tmp = MoveRelated(freeze);
  for (auto mov : tmp) {
    // mov: dst<-src
    MOperand dst = dynamic_cast<MOperand>(mov->GetDef());
    MOperand src = dynamic_cast<MOperand>(mov->GetOperand(0));
    MOperand value;
    //找到除了freeze的另外一个操作数
    if (GetAlias(src) == GetAlias(freeze)) {
      value = GetAlias(dst);
    } else {
      value = GetAlias(src);
    }
    activeMoves.erase(mov);
    frozenMoves.insert(mov);
    if (MoveRelated(value).size() == 0) {
      if ((value->GetType() == riscv_i32 &&Degree[value] < reglist.GetReglistInt().size()) ||(value->GetType() == riscv_i64 && Degree[value] < reglist.GetReglistInt().size()) || (value->GetType() == riscv_float32 && Degree[value] < reglist.GetReglistFloat().size()) || (value->GetType() == riscv_ptr && Degree[value] < reglist.GetReglistInt().size())) {
        freezeWorkList.erase(value);
        PushVecSingleVal(simplifyWorkList, value);
      }
    }
  }
}
//将swl中的一个变量弹出准备分配颜色
void GraphColor::simplify(){
  auto val = simplifyWorkList.back();
  simplifyWorkList.pop_back();
  selectstack.push_back(val);

  _DEBUG(std::cerr << "SelectStack Insert: " << val->GetName() << std::endl;)

  auto adj = Adjacent(val);
for (auto target : AdjList[val]) {
  DecrementDegree(target);
}
//遍历 val 的邻居，对邻接节点执行 DecrementDegree（度数减 1，并更新其状态）
}

void GraphColor::coalesce(){
  for(int i=0;i<worklistMoves.size();i++){
    using MoveOperand = std::pair<MOperand, MOperand>;
    MoveOperand m;
    RISCVMIR *mv = worklistMoves[i];
      // rd <- rs
    MOperand rd = dynamic_cast<MOperand>(mv->GetDef()->ignoreLA());
    MOperand rs = dynamic_cast<MOperand>(mv->GetOperand(0)->ignoreLA());
    rd = GetAlias(rd);
    rs = GetAlias(rs);
    //如果某个寄存器之前已经合并,则返回其别名
    //始终让 m.first 是 Precolored 的（如果有）。
    if (Precolored.find(rs) != Precolored.end()) {
      m = std::make_pair(rs, rd);
    } else {
      m = std::make_pair(rd, rs);
    }
    vec_pop(worklistMoves,i);//移除已经处理的move指令,避免重复处理
    if(m.first==m.second){
      coalescedMoves.push_back(mv);
      AddWorkList(m.first);
    }else if(Precolored.find(m.second) != Precolored.end() ||adjSet[m.first].find(m.second) != adjSet[m.first].end()){
      //如果都已经预着色,或者在干涉图中相邻,那就不能合并
      constrainedMoves.insert(mv);
      AddWorkList(m.first);
      AddWorkList(m.second);
    }else if((Precolored.find(m.first) != Precolored.end() &&
    GeorgeCheck(m.second, m.first, m.second->GetType())) ||
   (Precolored.find(m.first) == Precolored.end() &&
    BriggsCheck(m.second, m.first, m.first->GetType()))){
      coalescedMoves.push_back(mv);
      combine(m.first, m.second);
      AddWorkList(m.first);
    }else{
      activeMoves.insert(mv);
    }
  }
}

void GraphColor::freeze() {
  auto freeze = HeuristicFreeze();
  freezeWorkList.erase(freeze);
  PushVecSingleVal(simplifyWorkList, freeze);
  //放弃对这个freeze节点合并的希望，将他看成是传送无关节点
  FreezeMoves(freeze);
}
// Spill 的意义：
// 溢出变量并不代表马上生成 spill 指令。

// 它会正常参与图着色过程。

// 若最终着色失败，才会真正生成 store/load 指令。
void GraphColor::spill() {
  auto spill = HeuristicSpill();
  if (spill == nullptr)
    return;
  spillWorkList.erase(spill);
  _DEBUG(std::cerr << "Choose To Spill element: " << spill->GetName()
                   << std::endl;)
  SpillStack.push_back(spill);
  PushVecSingleVal(simplifyWorkList, spill);
  FreezeMoves(spill);
}

//终于到了,分配实际物理寄存器!
void GraphColor::AssignColors(){
  while(!selectstack.empty()){
    MOperand select = selectstack.front();
    RISCVType ty = select->GetType();
    selectstack.erase(selectstack.begin());
    std::unordered_set<MOperand> int_assist{reglist.GetReglistInt().begin(),reglist.GetReglistInt().end()};
    std::unordered_set<MOperand> float_assist{reglist.GetReglistFloat().begin(), reglist.GetReglistFloat().end()};
    //遍历所有的冲突点，查看他们分配的颜色，保证我们分配的颜色一定是不同的
      for(auto adj:AdjList[select]){
        if (coloredNode.find(GetAlias(adj)) != coloredNode.end() ||
            Precolored.find(GetAlias(adj)) != Precolored.end()) {
          if (color.find(dynamic_cast<MOperand>(GetAlias(adj))) == color.end())
            assert(0);
          if (ty == riscv_i32 || ty == riscv_ptr || ty == riscv_i64)
            int_assist.erase(color[GetAlias(adj)]);
          else if (ty == riscv_float32)
            float_assist.erase(color[GetAlias(adj)]);
        }
      }
      bool flag=false;
      if(ty==riscv_i32||ty==riscv_ptr||ty==riscv_i64){
        if(int_assist.size()==0){
          spilledNodes.insert(select);
        }else{
          coloredNode.insert(select);
          color[select]=SelectPhyReg(select,ty,int_assist);

          _DEBUG(std::cerr << "Assign reg(int): " << color[select]->GetName()
                         << " To " << select->GetName() << std::endl;)
        }
      }else if(ty==riscv_float32){
        if (float_assist.size() == 0){
        spilledNodes.insert(select);
        }else{
          coloredNode.insert(select);
          color[select] = SelectPhyReg(select, ty, float_assist);
          _DEBUG(std::cerr << "Assign reg(float): " << color[select]->GetName()
                          << " To " << select->GetName() << std::endl;)
        }
      }
  }
  for (auto caols : coalescedNodes) {
    // if (caols->GetName() == ".31"){
    //   int i = 0;
    // }调试语句，用于打断点
    color[caols] = color[GetAlias(caols)];
  }
}

// 该函数遍历所有基本块和指令，把被 spill 的虚拟寄存器：

// 替换成内存加载/存储指令（load/store）

// 用新的临时虚拟寄存器来保存中间值

// 更新 mir 指令中的使用（operand）和定义（def）
void GraphColor::SpillNodeInMir(){
  //记录 spill 过程中 **临时新生成的虚拟寄存器**（用于加载/保存）
  std::unordered_set<VirRegister*> temps;
  for(const auto mbb: topu){
    for(auto mir_begin=mbb->begin(),mir_end=mbb->end();mir_begin!=mir_end){
      auto mir=*mir_begin;
      if(mir->GetOpcode()==RISCV::call||mir->GetOpcode()==RISCV::ret||mir->GetOpcode()==RISCV::_j){
        ++mir_begin;
        continue;//跳过特定指令
      }
      if(mir->GetDef()!=nullptr&& dynamic_cast<VirRegister*>(mir->GetDef())&& spilledNodes.find(dynamic_cast<VirRegister*>(mir->GetDef()))!=spilledNodes.end()){
        auto op = dynamic_cast<VirRegister *>(mir->GetDef());
        auto sd = CreateSpillMir(mir->GetDef(), temps);
        mir_begin.insert_after(sd);
        _DEBUG(std::cerr
                   << "Spilling "
                   << dynamic_cast<VirRegister *>(mir->GetDef())->GetName()
                   << ", Use Vreg "
                   << dynamic_cast<VirRegister *>(sd->GetOperand(0))->GetName()
                   << " To Replace" << std::endl;)
        mir->SetDef(sd->GetOperand(0));
          //r10 = add r3, r4   // r10 溢出了
          //belike:
          //vTemp = add r3, r4   // 替换 r10 为临时寄存器
          //sw vTemp, offset(sp) // 把 vTemp 写回内存（spill）

        if (mir->GetOpcode() == RISCVMIR::RISCVISA::mv ||
            mir->GetOpcode() == RISCVMIR::RISCVISA::_fmv_s){
          NotMove.insert(mir);
        }
      }
      for(int i=0;i<mir->GetOperandSize();i++){
        auto operand=mir->GetOperand(i);
        if(mir->GetOperand(i) != nullptr &&
        dynamic_cast<VirRegister *>(mir->GetOperand(i)) &&
        spilledNodes.find(dynamic_cast<VirRegister *>(
            mir->GetOperand(i))) != spilledNodes.end()){
              auto op = dynamic_cast<VirRegister *>(mir->GetOperand(i));
              auto ld = CreateLoadMir(mir->GetOperand(i), temps);
              mir_begin.insert_before(ld);
              _DEBUG(
                std::cerr
                    << "Find a Spilled Node "
                    << dynamic_cast<VirRegister *>(mir->GetOperand(i))->GetName()
                    << ", Use Vreg "
                    << dynamic_cast<VirRegister *>(ld->GetDef())->GetName()
                    << " To Replace" << std::endl;)
              mir->SetOperand(i, ld->GetDef());
              //  add r1, r2, r3
              //  ld vTemp, offset(sp)
              //add r1, vTemp, r3   现在 r2 被 vTemp 替代
              if(mir->GetOpcode() == RISCVMIR::RISCVISA::mv ||
              mir->GetOpcode() == RISCVMIR::RISCVISA::_fmv_s){
                NotMove.insert(mir);
              }
            }
      }
      ++mir_begin;
    }
  }
  spilledNodes.clear();
  initial.clear();
  initial.insert(coloredNode.begin(), coloredNode.end());
  initial.insert(coalescedNodes.begin(), coalescedNodes.end());
  initial.insert(temps.begin(), temps.end());
  coalescedNodes.clear();
  coloredNode.clear();
}

RISCVMIR *GraphColor::CreateSpillMir(RISCVMOperand *spill,std::unordered_set<VirRegister*> &temps){
  auto vreg=dynamic_cast<VirRegister*>(spill);
  assert(vreg && "the chosen operand must be a vreg");
  
  //特殊用处寄存器
  if(auto specialmop=m_func->GetSpecialUsageMOperand(vreg)){
    auto mir=new RISCVMIR(RISCVMIR::MarkDead);
    auto newreg=ctx.createVReg(vreg->GetType());
    mir->AddOperand(newreg);
    temps.insert(newreg);
    return mir;
  }
  VirRegister *reg = new VirRegister(vreg->GetType());
  temps.insert(reg);
  RISCVMIR *sd = nullptr;
  if (spill->GetType() == RISCVType::riscv_i32 ||
      spill->GetType() == RISCVType::riscv_ptr ||
      spill->GetType() == RISCVType::riscv_i64 )
    sd = new RISCVMIR(RISCVMIR::RISCVISA::_sd);
  else if (spill->GetType() == RISCVType::riscv_float32)
    sd = new RISCVMIR(RISCVMIR::RISCVISA::_fsw);
  sd->AddOperand(reg);
  auto spillnode = m_func->GetFrame()->spill(vreg);
  sd->AddOperand(spillnode);
  return sd;
}

RISCVMIR *GraphColor::CreateLoadMir(RISCVMOperand *load,std::unordered_set<VirRegister *> &temps) {
auto vreg = dynamic_cast<VirRegister *>(load);
assert(vreg && "the chosen operand must be a vreg");

if (auto specialmop = m_func->GetSpecialUsageMOperand(vreg)) {
auto mir = m_func->CreateSpecialUsageMIR(specialmop);
temps.insert(mir->GetDef()->as<VirRegister>());
return mir;
}

VirRegister *reg = new VirRegister(vreg->GetType());
temps.insert(reg);
RISCVMIR *lw = nullptr;
if (load->GetType() == RISCVType::riscv_i32 ||
load->GetType() == RISCVType::riscv_ptr ||
load->GetType() == RISCVType::riscv_i64 )
lw = new RISCVMIR(RISCVMIR::RISCVISA::_ld);
else if (load->GetType() == RISCVType::riscv_float32)
lw = new RISCVMIR(RISCVMIR::RISCVISA::_flw);
auto spillnode = m_func->GetFrame()->spill(vreg);
lw->SetDef(reg);
lw->AddOperand(spillnode);
return lw;
}

void GraphColor::RewriteProgram(){
  for(const auto mbb:topu){
    for(auto mirit=mbb-begin();mirit!=mbb->end();){
      auto mir=*mirit;
      ++mirit;
      //忽略call
      if(mir->GetOpcode()==RISCVMIR::call){
        continue;
      }
      //替换定义的寄存器（Def）
      if (mir->GetDef() != nullptr && dynamic_cast<VirRegister *>(mir->GetDef())) {
        if (color.find(dynamic_cast<MOperand>(mir->GetDef())) == color.end())
          assert(0);
        auto replace = color[dynamic_cast<MOperand>(mir->GetDef())];
        _DEBUG(
            std::cerr << "REPLACE Vreg "
                      << dynamic_cast<VirRegister *>(mir->GetDef())->GetName()
                      << " To Preg " << replace->GetName() << std::endl;)
        mir->SetDef(replace);
      }
      //替换每一个Use
      for(int i=0;i<mir->GetOperandSize();i++){
        auto operand=mir->GetOperand(i);
        //普通寄存器，直接替换为物理寄存器
        if(dynamic_cast<VirRegister*>(operand)){
          if(color.find(dynamic_cast<MOperand>(operand))==color.end()){
            assert(0);
          }
          auto replace=color[dynamic_cast<MOperand>(operand)];
          _DEBUG(std::cerr << "REPLACE Vreg "
            << dynamic_cast<VirRegister *>(operand)->GetName()
            << " To Preg " << replace->GetName() << std::endl;)
          mir->SetOperand(i, replace);
        }else if(){
          
        }
      }
    }
  }
}