/**
 * Not any company's property but Public-Domain
 * Do with source-code as you will. No requirement to keep this
 * header if need to use it/change it/ or do whatever with it
 *
 * Note that there is No guarantee that this code will work
 * and I take no responsibility for this code and any problems you
 * might get if using it.
 *
 * Code & platform dependent issues with it was originally
 * published at http://www.kjellkod.cc/threadsafecircularqueue
 * 2012-16-19  @author Kjell Hedstr�m, hedstrom@kjellkod.cc
 *
 * Coding standard compliance changes made.   No copyright applied
 * to this source as required by the original Public-Domain license.
 */

/* cinek changes 10-19-2014 */

// should be mentioned the thinking of what goes where
// it is a "controversy" whether what is tail and what is head
// http://en.wikipedia.org/wiki/FIFO#Head_or_tail_first

#ifndef CINEK_CIRCULAR_QUEUE_HPP
#define CINEK_CIRCULAR_QUEUE_HPP

/* 10-19-2014 - added to cinek namespace.
 */
#include <cstddef>
#include <cstdint>
#include <atomic>

namespace cinek {

template<typename Element, size_t Size>
class CircularBuffer
{
public:
  static constexpr size_t kCapacity = Size+1;
  static constexpr size_t kLimit = Size;

  using ValueType = Element;

  CircularBuffer() : _tail(0), _head(0){}

  bool push(const Element& item);
  bool push(Element&& item);
  Element* acquireTail();
  bool push();

  bool pop(Element& item);
  bool pop();

  bool isEmpty() const;
  bool isFull() const;
  bool isLockFree() const;
  size_t size() const;

  Element& at(size_t index);
  const Element& at(size_t index) const;

private:
  size_t increment(size_t idx) const;

  std::atomic<size_t>  _tail;  // tail(input) index
  Element    _array[kCapacity];
  std::atomic<size_t>   _head; // head(output) index
};


// Here with memory_order_seq_cst for every operation. This is overkill but easy to reason about
//
// Push on tail. TailHead is only changed by producer and can be safely loaded using memory_order_relexed
//         head is updated by consumer and must be loaded using at least memory_order_acquire
template<typename Element, size_t Size>
bool CircularBuffer<Element, Size>::push(const Element& item)
{
  const auto current_tail = _tail.load();
  const auto next_tail = increment(current_tail);
  if(next_tail != _head.load())
  {
    _array[current_tail] = item;
    _tail.store(next_tail);
    return true;
  }

  return false;  // full queue
}

template<typename Element, size_t Size>
bool CircularBuffer<Element, Size>::push(Element&& item)
{
  const auto current_tail = _tail.load();
  const auto next_tail = increment(current_tail);
  if(next_tail != _head.load())
  {
    _array[current_tail] = std::move(item);
    _tail.store(next_tail);
    return true;
  }

  return false;  // full queue
}

template<typename Element, size_t Size>
Element* CircularBuffer<Element, Size>::acquireTail()
{
  const auto current_tail = _tail.load();
  const auto next_tail = increment(current_tail);
  if (next_tail != _head.load())
  {
    return &_array[current_tail];
  }
  return nullptr;
}

template<typename Element, size_t Size>
bool CircularBuffer<Element, Size>::push()
{
  const auto current_tail = _tail.load();
  const auto next_tail = increment(current_tail);
  if(next_tail != _head.load())
  {
    _tail.store(next_tail);
    return true;
  }
  return false;  // full queue
}


// Pop by Consumer can only update the head
template<typename Element, size_t Size>
bool CircularBuffer<Element, Size>::pop(Element& item)
{
  const auto current_head = _head.load();
  if(current_head == _tail.load())
    return false;   // empty queue

  item = _array[current_head];
  _head.store(increment(current_head));
  return true;
}

template<typename Element, size_t Size>
bool CircularBuffer<Element, Size>::pop()
{
  const auto current_head = _head.load();
  if(current_head == _tail.load())
    return false;   // empty queue

  _head.store(increment(current_head));
  return true;
}


// snapshot with acceptance of that this comparison function is not atomic
// (*) Used by clients or test, since pop() avoid double load overhead by not
// using isEmpty()
template<typename Element, size_t Size>
bool CircularBuffer<Element, Size>::isEmpty() const
{
  return (_head.load() == _tail.load());
}

// snapshot with acceptance that this comparison is not atomic
// (*) Used by clients or test, since push() avoid double load overhead by not
// using isFull()
template<typename Element, size_t Size>
bool CircularBuffer<Element, Size>::isFull() const
{
  const auto next_tail = increment(_tail.load());
  return (next_tail == _head.load());
}


template<typename Element, size_t Size>
bool CircularBuffer<Element, Size>::isLockFree() const
{
  return (_tail.is_lock_free() && _head.is_lock_free());
}

template<typename Element, size_t Size>
size_t CircularBuffer<Element, Size>::increment(size_t idx) const
{
  return (idx + 1) % kCapacity;
}

template<typename Element, size_t Size>
size_t CircularBuffer<Element, Size>::size() const
{
  const auto current_head = _head.load();
  const auto current_tail = _tail.load();
  if (current_tail >= current_head) {
    return current_tail - current_head;
  } else {
    return (kCapacity - current_head) + current_tail;
  }
}

template<typename Element, size_t Size>
Element& CircularBuffer<Element, Size>::at(size_t index)
{
  const auto current_head = _head.load();
  return _array[(current_head + index) % kCapacity];
}

template<typename Element, size_t Size>
const Element& CircularBuffer<Element, Size>::at(size_t index) const
{
  const auto current_head = _head.load();
  return _array[(current_head + index) % kCapacity];
}


} // sequential_consistent
#endif /* CIRCULARFIFO_SEQUENTIAL_H_ */
