#include "../include/lib/MyList.hpp"

template <typename T>
void BaseList<T>::push_front(T *_data)
{
  Base::push_front(DataType(_data));
}

template <typename T>
void BaseList<T>::push_back(T *_data)
{
  Base::push_back(DataType(_data));
}

template <typename Manager, typename Staff>
Node<Manager, Staff>::~Node()
{
  if (manager)
    EraseFromManager();
}

template <typename Manager, typename Staff>
void Node<Manager, Staff>::SetManager(Manager *_manager)
{
  manager = _manager;
}

template <typename Manager, typename Staff>
Manager *Node<Manager, Staff>::GetParent() const
{
  return manager;
}

template <typename Manager, typename Staff>
Staff *Node<Manager, Staff>::GetNextNode() const
{
  return next;
}

template <typename Manager, typename Staff>
Staff *Node<Manager, Staff>::GetPrevNode() const
{
  return prev;
}

template <typename Manager, typename Staff>
void Node<Manager, Staff>::EraseFromManager()
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

template <typename Manager, typename Staff>
void Node<Manager, Staff>::ReplaceNode(Staff *other)
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

template <typename Manager, typename Staff>
List<Manager, Staff>::~List()
{
  clear();
}

template <typename Manager, typename Staff>
Staff *List<Manager, Staff>::GetFront() const
{
  return front;
}

template <typename Manager, typename Staff>
Staff *List<Manager, Staff>::GetBack() const
{
  return back;
}

template <typename Manager, typename Staff>
List<Manager, Staff>::iterator::iterator(Staff *node) : ptr(node) {}

template <typename Manager, typename Staff>
typename List<Manager, Staff>::iterator &List<Manager, Staff>::iterator::operator++()
{
  if (ptr)
    ptr = ptr->next;
  return *this;
}

template <typename Manager, typename Staff>
typename List<Manager, Staff>::iterator &List<Manager, Staff>::iterator::operator--()
{
  if (ptr)
    ptr = ptr->prev;
  return *this;
}

template <typename Manager, typename Staff>
typename List<Manager, Staff>::iterator::value_type List<Manager, Staff>::iterator::operator*() const
{
  return ptr;
}

template <typename Manager, typename Staff>
bool List<Manager, Staff>::iterator::operator==(const iterator &other) const
{
  return ptr == other.ptr;
}

template <typename Manager, typename Staff>
bool List<Manager, Staff>::iterator::operator!=(const iterator &other) const
{
  return ptr != other.ptr;
}

template <typename Manager, typename Staff>
typename List<Manager, Staff>::iterator List<Manager, Staff>::iterator::InsertBefore(Staff *_node)
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

template <typename Manager, typename Staff>
typename List<Manager, Staff>::iterator List<Manager, Staff>::iterator::InsertAfter(Staff *_node)
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

template <typename Manager, typename Staff>
typename List<Manager, Staff>::iterator List<Manager, Staff>::begin()
{
  return iterator(front);
}

template <typename Manager, typename Staff>
typename List<Manager, Staff>::iterator List<Manager, Staff>::end()
{
  return iterator(nullptr);
}

template <typename Manager, typename Staff>
typename List<Manager, Staff>::iterator List<Manager, Staff>::rbegin()
{
  return iterator(back);
}

template <typename Manager, typename Staff>
typename List<Manager, Staff>::iterator List<Manager, Staff>::rend()
{
  return iterator(nullptr);
}

template <typename Manager, typename Staff>
void List<Manager, Staff>::CollectList(Staff *begin, Staff *end)
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

template <typename Manager, typename Staff>
std::pair<Staff *, Staff *> List<Manager, Staff>::SplitList(Staff *begin, Staff *end)
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

template <typename Manager, typename Staff>
int List<Manager, Staff>::Size() const
{
  return size;
}

template <typename Manager, typename Staff>
void List<Manager, Staff>::push_back(Staff *_node)
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

template <typename Manager, typename Staff>
void List<Manager, Staff>::push_front(Staff *_node)
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

template <typename Manager, typename Staff>
void List<Manager, Staff>::ReplaceList(Staff *begin, Staff *end, std::list<Staff *> &sequence)
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

template <typename Manager, typename Staff>
void List<Manager, Staff>::clear()
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

template <typename Manager, typename Staff>
template <typename Condition>
Staff *List<Manager, Staff>::find(Condition cond)
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

template <typename Manager, typename Staff>
void List<Manager, Staff>::erase(Staff *_node)
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