#include "CoreBaseClass.hpp"
#include <vector>
#include "BiscBlock.hpp"
class BasicBlock;

// Function 类是用来储存bb类的
class Function:public Value
{
private:
    std::vector<BasicBlock*> bbs;
    int BB_num = 0;
public:
    int Get_BB_num()  {return BB_num;}
    std::vector<BasicBlock*>&  GetBBVector() {return &bbs;} 
};