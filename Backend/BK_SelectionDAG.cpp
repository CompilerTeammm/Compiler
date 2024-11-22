#include "../include/Backend/BK_DAG_Node.h"

// ��.ll�ļ���һ��ת����DAG_Node
std::shared_ptr<DAG_Node> Trans_To_Node(const std::string &line)
{
  // ��ȡ.ll�ļ��е�ÿһ���ַ�����������뵽tokens������
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

  // ��ȡ���飬����Ϣ���뵽DAG_Node��(op�е�value)
  std::string op = tokens[0];
  std::string value = tokens[1];

  // std::shared_ptr<DAG_Node> Node = std::make_shared<DAG_Node>(op,value);
  auto Node = std::make_shared<DAG_Node>(op, value);

  for (size_t i = 2; i < tokens.size(); i++)
  {
    // �����������洢
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

// ����ӳ���:��ÿһ�н�����DAG_Node
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

// ����selectionDAGͼ
std::map<std::string, std::shared_ptr<DAG_Node>> Create_SelectionDAG(const std::map<std::string, std::shared_ptr<DAG_Node>> &Nodes)
{
  std::map<std::string, std::shared_ptr<DAG_Node>> DAG_Nodes;

  for (const auto &[value, Node] : Nodes)
  {
    // std::shared_ptr<DAG_Node> node = std::make_shared<DAG_Node>(Node->op,Node->value);
    auto node = std::make_shared<DAG_Node>(Node->op, Node->value);
    DAG_Nodes[value] = node;
  }

  for (const auto &[value, Node] : Nodes)               // ����ӳ����ҵ���Ӧvalue��node
  {                                                     // �γɣ�
    auto node = DAG_Nodes[value];                       // node: ops��op(value) op() op() op()
    for (const auto &op : node->ops)                    //       op
    {                                                   //       value
      if (DAG_Nodes.find(op->value) != DAG_Nodes.end()) // node��...
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