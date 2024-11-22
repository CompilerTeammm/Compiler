#include "../include/Backend/BK_DAG_Node.h"

// 将.ll文件的一行转换成DAG_Node
std::shared_ptr<DAG_Node> Trans_To_Node(const std::string &line)
{
  // 读取.ll文件中的每一行字符串，将其存入到tokens数组里
  std::istringstream iss(line);
  std::string token;
  std::vector<std::string> tokens;

  while (iss >> token)
  {
    tokens.push_back(token);
  }

  while (tokens.empty())
  {
    return nullptr;
  }

  // 读取数组，将信息存入到DAG_Node中(op中的value)
  std::string op = tokens[0];
  std::string value = tokens[1];

  // std::shared_ptr<DAG_Node> Node = std::make_shared<DAG_Node>(op,value);
  auto Node = std::make_shared<DAG_Node>(op, value);

  for (size_t i = 2; i < tokens.size(); i++)
  {
    // 将操作码进入存储
    if (tokens[i] == "%")
    {
      i++;
      std::string op_value = tokens[i];
      auto op_Node = std::make_shared<DAG_Node>(" ", op_value);
      Node->ops.push_back(op_Node);
    }
  }

  return Node;
}

// 创建映射表:将每一行解析成DAG_Node
std::map<std::string, std::shared_ptr<DAG_Node>> Trans_To_File(const std::string &FileName)
{
  std::ifstream File(FileName);

  if (!File.is_open())
  {
    throw std::runtime_error("faile to open");
  }

  std::map<std::string, std::shared_ptr<DAG_Node>> Nodes;
  std::string line;

  while (std::getline(File, line))
  {
    // std::shared_ptr<DAG_Node>
    auto node = Trans_To_Node(line);
    if (node)
    {
      Nodes[node->value] = node;
    }
  }

  return Nodes;
}

// 创建selectionDAG图
std::map<std::string, std::shared_ptr<DAG_Node>> Create_SelectionDAG(const std::map<std::string, std::shared_ptr<DAG_Node>> &Nodes)
{
  std::map<std::string, std::shared_ptr<DAG_Node>> DAG_Nodes;

  for (const auto &[value, Node] : Nodes)
  {
    // std::shared_ptr<DAG_Node> node = std::make_shared<DAG_Node>(Node->op,Node->value);
    auto node = std::make_shared<DAG_Node>(Node->op, Node->value);
    DAG_Nodes[value] = node;
  }

  for (const auto &[value, Node] : Nodes)               // 遍历映射表，找到对应value的node
  {                                                     // 形成：
    auto node = DAG_Nodes[value];                       // node: ops：op(value) op() op() op()
    for (const auto &op : node->ops)                    //       op
    {                                                   //       value
      if (DAG_Nodes.find(op->value) != DAG_Nodes.end()) // node：...
      {
        node->ops.push_back(DAG_Nodes[op->value]);
      }
      else
      {
        //...
      }
    }
  }
  // do something
}