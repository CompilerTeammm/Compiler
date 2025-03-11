#pragma once
#include <list>
#include <memory>
#include <vector>
#include <algorithm>
#include <cassert>
// 统一实现双向链表，可以减少很多块内冗余代码

// dh
template <typename Manager, typename Staff>
class List;

// BaseList封装list
template <typename T>
class BaseList : public std::list<std::unique_ptr<T>>
{
private:
  using Base = std::list<std::unique_ptr<T>>;
  using DataType = std::unique_ptr<T>;

public:
  void push_front(T *_data);
  void push_back(T *_data);
};

// 对于节点，关心前后节点prev&next
template <typename Manager, typename Staff>
class Node
{
  friend class List<Manager, Staff>;

private:
  Staff *prev = nullptr;
  Staff *next = nullptr;
  Manager *manager = nullptr;

public:
  Node() = default;
  virtual ~Node();

  void SetManager(Manager *_manager);
  Manager *GetParent() const;
  Staff *GetNextNode() const;
  Staff *GetPrevNode() const;

  virtual void EraseFromManager();
  void ReplaceNode(Staff *other);
};

// 对于列表整体，关心首尾节点head&back
template <typename Manager, typename Staff>
class List
{
  friend class Node<Manager, Staff>;

private:
  Staff *front = nullptr;
  Staff *back = nullptr;
  int size = 0;

public:
  virtual ~List();

  Staff *GetFront() const;
  Staff *GetBack() const;

  class iterator
  {
    Staff *ptr = nullptr;

  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = Staff *;
    using difference_type = std::ptrdiff_t;
    using pointer = Staff **;
    using reference = Staff *&;

    iterator() = default;
    iterator(Staff *node);

    iterator &operator++();
    iterator &operator--();
    value_type operator*() const;
    bool operator==(const iterator &other) const;
    bool operator!=(const iterator &other) const;

    iterator InsertBefore(Staff *_node);
    iterator InsertAfter(Staff *_node);
  };

  iterator begin();
  iterator end();
  iterator rbegin();
  iterator rend();

  void CollectList(Staff *begin, Staff *end);
  std::pair<Staff *, Staff *> SplitList(Staff *begin, Staff *end);
  int Size() const;

  void push_back(Staff *_node);
  void push_front(Staff *_node);
  void ReplaceList(Staff *begin, Staff *end, std::list<Staff *> &sequence);
  void clear();

  template <typename Condition>
  Staff *find(Condition cond);

  void erase(Staff *_node);
};
