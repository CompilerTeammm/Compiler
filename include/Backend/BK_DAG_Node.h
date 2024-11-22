#include <iostream>
#include <vector>
#include <sstream>
#include <map>
#include <fstream>

#include <memory>

/*
define i32 @main() {
entry:
  %a = alloca i32, align 4
  store i32 42, i32* %a, align 4
  %b = load i32, i32* %a, align 4
  ret i32 %b
}

define i32 @add(i32 %a, i32 %b) #0 {
entry:
  %addtmp = add nsw i32 %a, %b
  ret i32 %addtmp
}

%1 = add i32 %2, %3
%2 = sub i32 %4, %5
%3 = mul i32 %6, %7

*/

// 定义DAG的结点
class DAG_Node
{
public:
  DAG_Node(std::string op, std::string value) : op(op), value(value)
  {
  }

  std::string op; // 操作符
  std::vector<std::shared_ptr<DAG_Node>> ops;
  std::string value;
};
