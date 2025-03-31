#include "../include/backend/RISCVMIR.hpp"
#include "../include/backend/RISCVMOperand.hpp"
#include "../include/backend/RISCVRegister.hpp"
#include "../include/backend/Liveness.hpp"
#include "../util/my_stl.hpp"
#include <algorithm>
#include <map>
#include <regex>
#include <set>
using BlockInfo = Liveness;
using InterVal = LiveInterval;
using OpType = RISCVMIR::RISCVISA;
void BlockInfo::RunOnFunction(){
    for(RISCVBasicBlock *block:*m_func){
        BlockLivein[block].clear();
        BlockLiveout[block].clear();
        GetBlockLivein(block);
        GetBlockLiveout(block);
    }
    bool modified=true;//是否需要迭代
    while(modified){
        modified=false;
        //逆序遍历计算livein liveout
        for(auto block=m_func->rbegin();block!=m_func->rend();--block){
            RISCVBasicBlock *cur = *block;
            std::unordered_set<MOperand> oldin = BlockLivein[cur];//记录当前livein
            GetBlockLiveout(cur);//计算当前基本块liveout
            BlockLivein[cur] = BlockLiveout[cur];//先将out赋值给in
            GetBlockLivein(cur);//计算livein
            if (BlockLivein[cur] != oldin)
                modified = true;
        }
    }
}
//计算后继
void BlockInfo::CalCulateSucc(RISCVBasicBlock *block){
    for(auto inst=block->rbegin();inst!=block->rend();){
        OpType Opcode=(*inst)->GetOpcode();
        if(Opcode==OpType::_j){
            RISCVBasicBlock *succ=dynamic_cast<RISCVBasicBlock*>((*inst)->GetOperand(0));
            SuccBlocks[block].push_front(succ);
            --inst;
            continue;
    }else if(Opcode == OpType::_beq || Opcode == OpType::_bne || Opcode == OpType::_blt || Opcode == OpType::_bge ||Opcode == OpType::_bltu || Opcode == OpType::_bgeu || Opcode == OpType::_bgt || Opcode == OpType::_ble){
        RISCVBasicBlock *succ=dynamic_cast<RISCVBasicBlock*>((*inst)->GetOperand(2));
        SuccBlocks[block].push_front(succ);
        --inst;
        continue;
    }
    else{
        return;
    }
    }
}
void BlockInfo::GetBlockLiveout(RISCVBasicBlock *block){
    for(RISCVBasicBlock *succ:SuccBlocks[block]){
        BlockLiveout[block].insert(BlockLivein[succ].begin(),BlockLivein[succ].end());
        //某个基本块的LiveOut集合是所有后继基本块的LiveIn集合的并集。
    }
}
void BlockInfo::GetBlockLivein(RISCVBasicBlock *block){
    for(auto inst =block->rbegin();inst!=block->rend();--inst){
        OpType Opcode=(*inst)->GetOpcode();
//处理def(定义变量)        
// 获取该指令定义的寄存器（GetDef()）。
// 如果该寄存器 reg 存在：
// 从 LiveIn[block] 集合中删除 reg（因为它在这里被重新定义）。
// 分类处理：

// 虚拟寄存器（VirRegister）：
// 加入 initial 集合，表示它是需要分配颜色的寄存器。

// 物理寄存器（PhyRegister）：
// 加入 Precolored 集合，表示它是 预着色寄存器（已经分配固定颜色的寄存器）。
// 直接给 color[phy] = phy，意味着它的颜色固定。
        if(auto def=(*inst)->GetDef()){
            if(auto reg=def->ignoreLA()){
                BlockLivein[block].erase(reg);
                if(dynamic_cast<VirRegister*>(reg)){
                    initial.insert(reg);
                }else if(auto phy=dynamic_cast<PhyRegister*>(reg)){
                    Precolored.insert(phy);
                    color[phy]=phy;
                }
            }
        }
//处理use(使用变量)
// 获取两个操作数 val1 和 val2，并加入 LiveIn。
// 分类处理：
// 虚拟寄存器：加入 initial，表示需要分配颜色。

// 物理寄存器：加入 Precolored，并固定颜色。       
        if(Opcode==OpType::_j){
            continue;
        }else if(Opcode == OpType::_beq || Opcode == OpType::_bne || Opcode == OpType::_blt || Opcode == OpType::_bge ||Opcode == OpType::_bltu || Opcode == OpType::_bgeu || Opcode == OpType::_bgt ||Opcode == OpType ::_ble){
            if(auto val1=(*inst)->GetOperand(0)->ignoreLA()){
                BlockLivein[block].insert(val1);
                if(dynamic_cast<VirRegister*>(val1)){
                    initial.insert(val1);
                }else if(auto phy=dynamic_cast<PhyRegister*>(val1)){
                    Precolored.insert(phy);
                    color[phy]=phy;
                }
            }
            if(auto val2 = (*inst)->GetOperand(1)->ignoreLA()){
                BlockLivein[block].insert(val2);
                if(dynamic_cast<VirRegister*>(val2)){
                    initial.insert(val2);
                }else if(auto phy =dynamic_cast<PhyRegister*>(val2)){
                    Precolored.insert(phy);
                    color[phy]=phy;
                }
            }
        }else if(Opcode==OpType::ret){
//处理ret(返回指令)
//如果 ret 指令有返回值（a0 或 fa0），那么该寄存器应该加入 LiveIn，并标记为 预着色寄存器。
            if((*inst)->GetOperandSize() != 0){
                auto phy = (*inst)->GetOperand(0)->as<PhyRegister>();
                assert(phy!=nullptr&&(phy->Getregenum() == PhyRegister::a0 || phy->Getregenum() == PhyRegister::fa0));
                BlockLivein[block].insert(phy);
                color[phy]=phy;
                Precolored.insert(phy);
            }
        }else{
            //处理call(函数调用)
            if(Opcode==OpType::call){
                for(auto reg : reglist.GetReglistCaller()){
                    Precolored.insert(reg);
                    color[reg]=reg;
                }
            }
            //处理普通指令的use
            for(int i=0;i < (*inst)->GetOperandSize(); i++){
                if((*inst)->GetOperand(i)){
                    if(auto reg=(*inst)->GetOperand(i)->ignoreLA()){
                        BlockLivein[block].insert(reg);
                        if(dynamic_cast<VirRegister*>(reg)){
                            initial.insert(reg);
                        }else if(auto phy=dynamic_cast<PhyRegister*>(reg)){
                            Precolored.insert(phy);
                            color[phy]=phy;
                        }
                    }
                }
            }
        }
    }
}
void BlockInfo::Build(){
    for(RISCVBasicBlock *block :*m_func){
        std::unordered_set<MOperand> live =BlockLivein[block];
        for(auto inst_=block->rbegin();inst_ !=block->rend();){
            RISCVMIR *inst=*inst_;
            OpType op=inst->GetOpcode();
            //寄存器复制指令
            if(op==OpType::mv||op==OpType::_fmv_s){
                //删除被移动的源操作数
                if(auto val=inst->GetOperand(0)->ignoreLA()){
                    live.erase(val);
                    if(!NotMove.count(inst)){
                        moveList[val].insert(inst);
                    }
                }
                //处理目标寄存器
                if(auto def=inst->GetDef()->ignoreLA()){
                    if(!NotMove.count(inst)){
                        moveList[def].insert(inst);
                    }
                }//movelist记录mv指令，以便后续寄存器合并
                if(!NotMove.count(inst)){
                    worklistMoves.push_back(inst);
                }
            }else if(op==OpType::call){//处理call指令
                for(auto reg:reglist.GetReglistCaller()){
                    live.insert(reg);
                }
                for(auto reg:reglist.GetReglistCaller()){
                    for(auto v:live){
                        AddEdge(reg,v);//添加干涉边，确保reg不能被分配给v
                    }
                }
                for(auto reg:reglist.GetReglistCaller()){
                    live.erase(reg);//移除caller—Saved，表明call指令结束后它们已经失效
                }
            }
            if(auto def_val=inst->GetDef()){
                if(auto def=def_val->ignoreLA()){
                    live.insert(def);
                    for(auto v:live){
                        AddEdge(def,v);
                    }
                    live.erase(def);
                }
            }
            //遍历所有操作数，将use加入live集合
            for(int i=0;i<inst->GetOperandSize();i++){
                if(auto val=inst->GetOperand(i)->ignoreLA()){
                    live.insert(val);
                }
            }
            InstLive[inst]=live;
            --inst_;
        }
    }
}
//用于 调试寄存器干涉图，帮助分析 哪些寄存器相互干涉，确保构造正确。
void BlockInfo::PrintEdge()
{
    for (auto &[key, val] : adjSet)
    {
        std::cout << "--------%" << key->GetName() << " Edge --------" << std::endl;
        int count = 0;
        for (auto v : val)
        {
            if (dynamic_cast<VirRegister *>(v))
                std::cout << "%" << v->GetName() << " ";
        }
        std::cout << std::endl;
    }
}
void BlockInfo::AddEdge(Register *u, Register *v)
{
    if (u == v)
        return;
    if (adjSet[u].count(v))
        return;
    adjSet[v].insert(u);
    adjSet[u].insert(v);
    if (Precolored.find(v) == Precolored.end())
    {
        AdjList[v].insert(u);
        Degree[v]++;
    }
    if (Precolored.find(u) == Precolored.end())
    {
        AdjList[u].insert(v);
        Degree[u]++;
    }
}
//打印每个basic block的livein liveout集合，用于调试活跃变量分析
void BlockInfo:PrintPass(){
    std::cout << "--------BlockLiveInfo--------" << std::endl;
    for (RISCVBasicBlock *_block : *m_func){
        std::cout << "--------Block:" << _block->GetName() << "--------" << std::endl;
        std::cout << "        Livein" << std::endl;
        for (RISCVMOperand *_value : BlockLivein[_block])
        {
            // if (dynamic_cast<VirRegister *>(_value))
            //     _value->print();
            // else
            //     _value->print();
            // std::cout << " ";
            _value->print();
            std::cout<<" ";
        }
        std::cout << std::endl;
        std::cout << "        Liveout" << std::endl;
        for (RISCVMOperand *_value : BlockLiveout[_block])
        {
            // if (dynamic_cast<VirRegister *>(_value))
            //     _value->print();
            // else
            //     _value->print();
            // std::cout << " ";
            _value->print();
            std::cout<<" ";
        }
        std::cout << std::endl;
    }    
}
//为每条指令赋予唯一编号
void InterVal::init(){
    int curr = 0;
    for (RISCVBasicBlock *block : *func){
        for (RISCVMIR *inst : *block)
        {
            instNum[inst] = curr;
            curr++;
        }
    }
}
//计算活跃区间
void InterVal::computeLiveIntervals(){
    for(RISCVBasicBlock *block:*func){
        std::unordered_map<MOperand,std::vector<InterVal>> CurrentRegLiveinterval;
        int begin=-1;//当前基本块的起始指令编号,用于区间计算
        for(RISCVMIR *inst:*block){
            int Curr =instNum[inst];
            if(inst==block->front()){
                begin=instNum[inst];//记录当前基本块第一条指令的编号
            }
            for(MOperand Op:InstLive[inst]){
                //如果Op没有活跃区间
                // 创建一个新的 Interval 记录 Op 的活跃区间起点。
                // 终点 end = -1，表示还未结束。
                // 把 Interval 存入 CurrentRegLiveinterval。
                if(!CurrentRegLiveinterval.count(Op)){
                    Interval interval;
                    interval.start=Curr;
                    interval.end=-1;
                    CurrentRegLiveinterval[Op].push_back(interval);
                }//如果当前指令 inst 是基本块的最后一条指令，并且区间还未结束，则更新区间终点 end = Curr
                 //如果 Op 的前一个区间已经结束，并且 Op 在 inst 这条指令中被使用（count(Op, inst) 为真），则创建一个新的活跃区间。
                else{
                    if(CurrentRegLiveinterval[Op].back().end==-1&&inst==block->back()){
                        CurrentRegLiveinterval[Op].back().end=Curr;
                    }else if(CurrentRegLiveinterval[Op].back().end != -1 && count(Op, inst)){
                        Interval interval;
                        interval.start=Curr;
                        interval.end=-1;
                        CurrentRegLiveinterval[Op].push_back(interval);
                    }
                }
            }
            for (auto &[Op, intervals] : CurrentRegLiveinterval){
                if (intervals.back().end == -1 && inst == block->back() && !count(Op, inst))
                    CurrentRegLiveinterval[Op].back().end = Curr - 1;//使活跃区间终止于Curr-1
                else if (intervals.back().end == -1 && inst == block->back())
                    CurrentRegLiveinterval[Op].back().end = Curr;
                else if (intervals.back().end == -1 && !count(Op, inst))
                    intervals.back().end = Curr - 1;
                else if (intervals.back().end == -1 && count(Op, inst))
                    continue;
            }
        }
        if(verify(CurrentRegLiveinterval)){
            for(auto &[op, intervals] : CurrentRegLiveinterval){
                auto curr=intervals.begin();
                for(auto iter = intervals.begin(); iter != intervals.end(); ++iter){
                    if(iter==curr){
                        continue;
                    }
                    if(curr->start < iter->start){
                        ++curr;
                        *curr=*iter;
                    }else{
                        curr->end=std::max(curr->end,iter->end);
                    }
                }
                intervals.erase(std::next(curr),intervals.end());
            }
            RegLiveness[block]=CurrentRegLiveinterval;
        }
    }
}
bool InterVal::verify(std::unordered_map<MOperand, std::vector<Interval>> Liveinterval){
    int num = 0;
    for (auto &[op, intervals] : Liveinterval){
        for (auto &i : intervals){
            if (i.start > i.end){
                return false;
            }
            if (num > i.end){
                return false;
            }
        }
    }
    return true;
}
void InterVal::PrintAnalysis(){
    std::cout << "--------InstLive--------" << std::endl;
    for (RISCVBasicBlock *block : *func){
        std::cout << "-----Block " << block->GetName() << "-----" << std::endl;
        for (RISCVMIR *inst : *block){
            std::cout << "inst" << instNum[inst] << "Liveness:";
            for (RISCVMOperand *Op : InstLive[inst]){
                if (dynamic_cast<VirRegister *>(Op)){
                    Op->print();
                }else{
                    Op->print();
                }
            }
            std::cout << std::endl;
        }
    }
    for (RISCVBasicBlock *block : *func){
        std::cout << "--------LiveInterval--------" << std::endl;
        std::cout << "--------Block:" << block->GetName() << "--------" << std::endl;
        for (auto &[op, intervals] : RegLiveness[block]){
            if (dynamic_cast<VirRegister *>(op)){
                op->print();
            }else{
                op->print();
            }
            for (auto &i : intervals){
                std::cout << "[" << i.start << "," << i.end << "]";
            }
            std::cout << std::endl;
        }
    }
}
void InterVal::RunOnFunc_(){
    init();  // 初始化指令编号
    computeLiveIntervals();  // 计算活跃区间
}
