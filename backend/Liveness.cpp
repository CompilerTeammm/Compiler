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
            
            if(op==OpType::mv||op==OpType::_fmv_s){
                if(auto val=inst->GetOperand(0)->ignoreLA()){
                    live.erase(val);
                    if(!NotMove.count(inst)){
                        moveList[val].insert(inst);
                    }
                }
                if(auto def=inst->GetDef()->ignoreLA()){
                    if(!NotMove.count(inst)){
                        moveList[def].insert(inst);
                    }
                }
                if(!NotMove..count(inst)){
                    worklistMoves.push_back(inst);
                }
            }else if(op==OpType::call){
                
            }
        }
    }
}