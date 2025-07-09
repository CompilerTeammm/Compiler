#include "../../include/IR/Opt/SimplifyInst.hpp"

bool SimplifyInst::simplifyInst(Function* func){
    bool changed=false;

    for(auto& bb_ptr:func->GetBBs()){
        for(auto i=bb_ptr->begin();i!=bb_ptr->end();){
            Instruction* inst= *i;
            ++i;

            auto op=inst->id;
            //二元运算处理
            if(inst->GetOperandNums()==2){
                auto lhs=inst->GetOperand(0);
                auto rhs=inst->GetOperand(1);

                //ADD:x+0=x
                if(op==Instruction::Op::Add){
                    if(lhs->isConstZero()){
                        inst->ReplaceAllUseWith(rhs);
                        bb_ptr->erase(inst);
                        changed=true;
                        continue;
                    }
                    if(rhs->isConstZero()){
                        inst->ReplaceAllUseWith(lhs);
                        bb_ptr->erase(inst);
                        changed=true;
                        continue;
                    }
                }

                //MUL:x*1=x,x*0=0
                if(op==Instruction::Op::Mul){
                    if(lhs->isConstOne()){
                        inst->ReplaceAllUseWith(rhs);
                        bb_ptr->erase(inst);
                        changed=true;
                        continue;
                    }
                    if(rhs->isConstOne()){
                        inst->ReplaceAllUseWith(lhs);
                        bb_ptr->erase(inst);
                        changed=true;
                        continue;
                    }
                    if(lhs->isConstZero()||rhs->isConstZero()){
                        inst->ReplaceAllUseWith(ConstIRInt::GetNewConstant(0));
                        bb_ptr->erase(inst);
                        changed=true;
                        continue;
                    }
                }

                //SUB:x-0=x,x-x=0
                if(op==Instruction::Op::Sub){
                    if(rhs->isConstZero()){
                        inst->ReplaceAllUseWith(lhs);
                        bb_ptr->erase(inst);
                        changed=true;
                        continue;
                    }
                    if(lhs==rhs){
                        inst->ReplaceAllUseWith(ConstIRInt::GetNewConstant(0));
                        bb_ptr->erase(inst);
                        changed=true;
                        continue;
                    }
                }

                //DIV:x/1=x
                if(op==Instruction::Op::Div){
                    if(rhs->isConstOne()){
                        inst->ReplaceAllUseWith(lhs);
                        bb_ptr->erase(inst);
                        changed=true;
                        continue;
                    }
                }
            }
            //一元比较运算
            if(inst->IsCmpInst() && inst->GetOperandNums()==2){
                auto lhs=inst->GetOperand(0);
                auto rhs=inst->GetOperand(1);
                if(lhs==rhs){
                    if(op==Instruction::Op::Eq){
                        inst->ReplaceAllUseWith(ConstIRInt::GetNewConstant(1));
                        bb_ptr->erase(inst);
                        changed=true;
                        continue;
                    }
                    if(op==Instruction::Op::Ne){
                        inst->ReplaceAllUseWith(ConstIRInt::GetNewConstant(0));
                        bb_ptr->erase(inst);
                        changed=true;
                        continue;
                    }
                }
            }
        }
    }
    return changed;
}
bool SimplifyInst::run(){
    return simplifyInst(func);
}