#pragma once
#include <list>
#include <memory>
#include <vector>
#include <algorithm>
#include <cassert>
// 统一实现双向链表，可以减少很多块内冗余代码

// BaseList封装list
template <typename T>
class BaseList : public std::list<std::unique_ptr<T>>
{
  using Base = std::list<std::unique_ptr<T>>;
  using DataType = std::unique_ptr<T>;

public:
  void push_front(T *_data)
  {
    Base::push_front(DataType(_data));
  }
  void push_back(T *_data)
  {
    Base::push_back(DataType(_data));
  }
};

template <typename Manager, typename Staff>
class Node;
template <typename Manager, typename Staff>
class List;

// 对于节点，关心前后节点prev&next
template <typename Manager, typename Staff>
class Node
{
  friend class List<Manager, Staff>;

  Staff *prev = nullptr;
  Staff *next = nullptr;
  Manager *manager = nullptr;

public:
  Node() = default;

  virtual ~Node()
  {
    if (manager)
      EraseFromManager();
  }

  void SetManager(Manager *_manager) { manager = _manager; }
  Manager *GetParent() const { return manager; }//命名逻辑不一致，后面统一改一下
  Staff *GetNextNode() const { return next; }
  Staff *GetPrevNode() const { return prev; }

  virtual void EraseFromManager()
  {
    if (prev)
      prev->next = next;
    if (next)
      next->prev = prev;
    if (!prev)
      manager->front = next;
    if (!next)
      manager->back = prev;

    manager->size--;
    manager = nullptr;
    prev = nullptr;
    next = nullptr;
  }

  void ReplaceNode(Staff *other)
  {
    if (!prev)
      manager->front = other;
    if (!next)
      manager->back = other;
    if (prev)
      prev->next = other;
    if (next)
      next->prev = other;

    other->prev = prev;
    other->next = next;
    other->manager = manager;

    prev = nullptr;
    next = nullptr;
    manager = nullptr;
  }

  //// dh
  void InsertBefore(Staff *other)
  {
    assert(manager && "Node must belong to a List");
    auto it = typename List<Manager, Staff>::iterator(static_cast<Staff *>(this));
    it.InsertBefore(other);
  }

  void InsertAfter(Staff *other)
  {
    assert(manager && "Node must belong to a List");
    auto it = typename List<Manager, Staff>::iterator(static_cast<Staff *>(this));
    it.InsertAfter(other);
  }
};

// 对于列表整体，关心首尾节点head&back
template <typename Manager, typename Staff>
class List
{
  friend class Node<Manager, Staff>;
  int size = 0;

public:
  Staff *front = nullptr;
  Staff *back = nullptr;
  virtual ~List() { clear(); }

  Staff *GetFront() const
  {
    return front;
  }
  Staff *GetBack() const { return back; }

  class iterator
  {
    Staff *ptr = nullptr;
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = Staff *;
    using difference_type = std::ptrdiff_t;
    using pointer = Staff **;
    using reference = Staff *&;

  public:
    iterator(Staff *node) : ptr(node) {}

    iterator &operator++()
    {
      if (ptr)
        ptr = ptr->next;
      return *this;
    }

    iterator &operator--()
    {
      if (ptr)
        ptr = ptr->prev;
      return *this;
    }

    value_type operator*() { return ptr; }
    bool operator==(const iterator &other) const { return ptr == other.ptr; }
    bool operator!=(const iterator &other) const { return ptr != other.ptr; }

    iterator InsertBefore(Staff *_node)
    {
      assert(ptr && ptr->manager && _node && "Invalid iterator");
      if (ptr == ptr->manager->front)
      {
        ptr->manager->push_front(_node);
      }
      else
      {
        _node->SetManager(ptr->manager);
        _node->prev = ptr->prev;
        _node->next = ptr;
        ptr->prev->next = _node;
        ptr->prev = _node;
        ptr->manager->size++;
      }
      return iterator(_node);
    }

    iterator InsertAfter(Staff *_node)
    {
      assert(ptr && ptr->manager && _node && "Invalid iterator");
      if (ptr == ptr->manager->back)
      {
        ptr->manager->push_back(_node);
      }
      else
      {
        _node->SetManager(ptr->manager);
        _node->next = ptr->next;
        _node->prev = ptr;
        ptr->next->prev = _node;
        ptr->next = _node;
        ptr->manager->size++;
      }
      return iterator(_node);
    }
  };
  List()
  {
    // this->head = nullptr;
    // this->tail = nullptr;
  }
  iterator begin() { return iterator(front); }
  iterator end() { return iterator(nullptr); }
  iterator rbegin() { return iterator(back); }
  iterator rend() { return iterator(nullptr); }

  void CollectList(Staff *begin, Staff *end)
  {
    assert(!front && !back && "List must be empty for collection");
    front = begin;
    back = end;
    size = 0;
    for (auto node = front; node; node = node->next)
    {
      node->SetManager(static_cast<Manager *>(this));
      size++;
    }
  }

  std::pair<Staff *, Staff *> SplitList(Staff *begin, Staff *end)
  {
    assert(begin && end && "Invalid split range");
    assert(begin->manager == static_cast<Manager *>(this) &&
           end->manager == static_cast<Manager *>(this) && "Nodes not in this list");

    if (begin == front)
      front = end->next;
    if (end == back)
      back = begin->prev;

    if (begin->prev)
      begin->prev->next = end->next;
    if (end->next)
      end->next->prev = begin->prev;

    begin->prev = nullptr;
    end->next = nullptr;

    size = 0;
    for (auto node = front; node; node = node->next)
    {
      size++;
    }

    return {begin, end};
  }

  int Size() const { return size; }

  void push_back(Staff *_node)
  {
    _node->SetManager(static_cast<Manager *>(this));
    if (!front)
    {
      front = back = _node;
    }
    else
    {
      back->next = _node;
      _node->prev = back;
      back = _node;
    }
    size++;
  }

  void push_front(Staff *_node)
  {
    _node->SetManager(static_cast<Manager *>(this));
    if (!front)
    {
      front = back = _node;
    }
    else
    {
      front->prev = _node;
      _node->next = front;
      front = _node;
    }
    size++;
  }

  // 前弹出并返回
  Staff *pop_front()
  {
    if (!front)
      return nullptr;

    Staff *node = front;
    front = front->next;
    if (front)
    {
      front->prev = nullptr;
    }
    else
    {
      back = nullptr;
    }

    node->next = nullptr;
    node->prev = nullptr;
    node->SetManager(nullptr);

    size--;
    return node;
  }

  // 后弹出并返回
  Staff *pop_back()
  {
    if (!back)
      return nullptr;

    Staff *node = back;
    back = back->prev;
    if (back)
    {
      back->next = nullptr;
    }
    else
    {
      front = nullptr;
    }

    node->next = nullptr;
    node->prev = nullptr;
    node->SetManager(nullptr);

    size--;
    return node;
  }

  void ReplaceList(Staff *begin, Staff *end, std::list<Staff *> &sequence)
  {
    assert(!sequence.empty() && "Sequence can't be empty");

    auto prev = begin->prev;
    auto next = end->next;

    if (!prev)
      front = sequence.front();
    for (auto node : sequence)
    {
      assert(node->GetParent() == this && "Nodes in sequence must belong to this list");
      node->prev = prev;
      if (prev)
        prev->next = node;
      prev = node;
    }
    sequence.back()->next = next;
    if (next)
      next->prev = sequence.back();
  }

  void clear()
  {
    while (front)
    {
      auto temp = front;
      front = front->next;
      delete temp;
    }
    back = nullptr;
    size = 0;
  }

  // 寻找符合cond的节点
  template <typename Condition>
  Staff *find(Condition cond)
  {
    for (auto it = begin(); it != end(); ++it)
    {
      if (cond(*it))
      {
        return *it;
      }
    }
    return nullptr;
  }

  // 删除_node节点
  void erase(Staff *_node)
  {
    if (!_node || _node->GetParent() != this)
      return;
    if (_node->prev)
      _node->prev->next = _node->next;
    if (_node->next)
      _node->next->prev = _node->prev;
    if (_node == front)
      front = _node->next;
    if (_node == back)
      back = _node->prev;
    size--;
    delete _node;
  }
};