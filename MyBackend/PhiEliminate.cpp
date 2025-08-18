#include "../include/MyBackend/PhiEliminate.hpp"
#include "MIR.hpp"
#include <forward_list>
#include <queue>

// RISCVBlock* PhiEliminate::find_copy_block(BasicBlock* from,BasicBlock* to){
//     if(CopyBlockFinder.find(from)==CopyBlockFinder.end())
//         CopyBlockFinder[from] = std::map<BasicBlock*,RISCVBlock*>();
//     if(CopyBlockFinder[from].find(to)==CopyBlockFinder[from].end()){
//         auto mfunc= ctx->mapTrans(from->GetParent())->as<RISCVFunction>();
//         auto mfrom=ctx->mapTrans(from)->as<RISCVBlock>();
//         auto mto=ctx->mapTrans(to)->as<RISCVBlock>();
        
//         auto critialmbb=RISCVBlock::CreateRISCVBasicBlock();
//         mfrom->replace_succ(mto,critialmbb);
//         auto uncond=new RISCVMIR(RISCVMIR::_j);
//         uncond->AddOperand(mto);
//         critialmbb->push_back(uncond);

//         mfunc->push_back(critialmbb);

//         CopyBlockFinder[from][to]=critialmbb;
//     }
//     return CopyBlockFinder[from][to];
// }

// void PhiEliminate::runOnGraph(BasicBlock* pred,BasicBlock* succ,std::vector<std::pair<RISCVOp*,RISCVOp*>>& vec)
// {
//       /// 如果对 phi 函数的理解没问题的话应该是内向基环树

//     struct Node{
//         std::forward_list<int> nxt;
//         int indo=0;
//     };
//     std::map<RISCVOp*,int> mp;
//     for(int i=0,limi=vec.size();i<limi;i++)
//         mp[vec[i].second]=i;
//     std::vector<Node> graph(vec.size());
//     for(int i=0,limi=vec.size();i<limi;i++)
//         if(mp.find(vec[i].first)!=mp.end()){
//             auto src=i,dst=mp[vec[i].first];
//             graph[src].nxt.push_front(dst);
//             graph[dst].indo++;
//         }
    
//     std::queue<int> que;
    
//     for(int i=0,limi=vec.size();i<limi;i++)
//         if(graph[i].indo==0)
//             que.push(i);

//     std::map<RISCVOp*,RISCVOp*> stagedRegister;

//     /// @note Get the basicblock for emitting copy inst
//     RISCVBlock* emitMBB;
//     if(auto cond=dynamic_cast<CondInst*>(pred->GetBack()))
//         /// @todo try split critial edge
//         // emitMBB=;
//         emitMBB=find_copy_block(pred,succ);
//     else 
//         emitMBB=ctx->mapTrans(pred)->as<RISCVBlock>();
    
//     for(int i=0,j=0,limi=vec.size();i<limi;){
//         auto visit=[&](int ind){
            
//             i++;

//             auto [src,dst]=vec[ind];
//             if(stagedRegister.find(src)!=stagedRegister.end())
//                 src=stagedRegister[src];

//             if(graph[ind].indo!=0){
//                 /// @note stage register
//                 assert(graph[ind].indo==1&&"Unexpected Degree");
//                 graph[ind].indo=0;//手动拆环，防止后面ind再入队
//                 auto stageR=ctx->createVReg(src->GetType());
//                 emitMBB->push_before_branch(RISCVTrival::CopyFrom(stageR,dst));
//                 stagedRegister[dst]=stageR;
//             }

//             emitMBB->push_before_branch(RISCVTrival::CopyFrom(dst->as<VirRegister>(),src));

//             auto& node=graph[ind];
//             for(auto nxt:node.nxt){
//                 auto& nnode=graph[nxt];
//                 nnode.indo--;
//                 if(nnode.indo==0)
//                     que.push(nxt);
//             }
//         };
//         while(!que.empty()){
//             int cur=que.front();que.pop();
//             visit(cur);
//         }
//         if(i>=limi){
//             assert(i==limi);
//             break;
//         }
//         for(;j<limi;j++){
//             if(graph[j].indo){
//                 visit(j);
//                 if(!que.empty())break;
//             }
//         }
//     }  
// }

// void PhiEliminate:: runonBasicBlock(BasicBlock* bb)
// {
//     std::map<BasicBlock*,std::vector<std::pair<RISCVOp*,RISCVOp*>>> phiMap;
//     for(auto inst:*bb){
//         if(auto phi=dynamic_cast<PhiInst*>(inst)){
//             auto& phirec=phi->PhiRecord;
//             for(auto& [idx,pair]:phirec){
//                 auto [val,bb]=pair;
//                 if(val->IsUndefVal())continue;
//                 auto src=ctx->mapTrans(val);
//                 auto dst=ctx->mapTrans(phi);
//                 phiMap[bb].emplace_back(src,dst);
//             }
//         }
//         else break;
//     }
//     for(auto &[src,vec]:phiMap)
//         runOnGraph(src,bb,vec); 
// }


// bool PhiEliminate::run()
// {
//     for (auto bb :*func) 
//         runonBasicBlock(bb);
//     return true;
// }

bool PhiEliminate::run()
{
    return true;
}