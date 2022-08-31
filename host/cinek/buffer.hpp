/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Cinekine Media
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @file    cinek/buffer.hpp
 * @author  Samir Sinha
 * @date    12/3/2018
 * @brief   buffer classes ported from ckbuilder
 * @copyright Cinekine
 */

#ifndef CINEK_V2_BUFFER_HPP
#define CINEK_V2_BUFFER_HPP

#include "ckdebug.h"
#include "ckdefs.h"

#include "buffertypes.hpp"

#include <type_traits>
#include <new>

namespace cinek {
  /**
   * Storage for a contiguous area of object memory created from a
   * ResourceHeap.   This buffer does not perform any allocations from a
   * ResourceHeap directly - it is the application's responsibility to supply
   * a region of memory for the BufferBase to manage.
   */
  template<typename T>
  class BufferBase
  {
  public:
    using Range = Range<T>;
    using ConstRange = ConstRange<T>;
    using DataType = T;
    using ThisType = BufferBase<T>;

    BufferBase() = default;
    BufferBase(T* data, int32_t limit, int32_t size=0);
    BufferBase(BufferBase&& other) { moveFrom(other); }
    BufferBase& operator=(BufferBase&& other) { moveFrom(other); return *this; }
    BufferBase(const BufferBase& other) = delete;
    BufferBase& operator=(const BufferBase& other) = delete;

    const T& operator[](int32_t index) const {
      return *getNodeAtIndex(index);
    }
    T& operator[](int32_t index) {
      return *getNodeAtIndex(index);
    }

    const T* getBack() const {
      CK_ASSERT_RETURN_VALUE(head_ < tail_, head_);
      return tail_-1;
    }
    T* getBack() {
      return const_cast<T*>(static_cast<const BufferBase*>(this)->getBack());
    }
    int32_t calcIndex(const T* ptr) const {
      return static_cast<int32_t>(ptr - head_);
    }
    T* getNodeAtIndex(int32_t index);
    const T* getNodeAtIndex(int32_t index) const;
    Range getRange(BufferString bs = BufferString{0, -1});
    ConstRange getConstRange(BufferString bs = BufferString{0, -1}) const;

    BufferString getStringFromRange(Range range) const;
    BufferString getStringFromRange(ConstRange range) const;

    bool isEmpty() const { return tail_ == head_; }
    bool isFull() const { return tail_ == limit_; }

    ///
    /// @return The current amount of data between head and tail
    int32_t getSize() const { return static_cast<int32_t>(tail_ - head_); }

    /// @return The number of objects remaining in the buffer
    int32_t getRemaining() const { return static_cast<int32_t>(limit_ - tail_); }

    /// @return The capacity of the buffer
    int32_t getCapacity() const { return static_cast<int32_t>(limit_ - head_); }

    /// @param  value Pushed onto tail of buffer
    /// @return Reference to this Buffer for chaining
    void pushBack(T value);

    template<class... Args>
    T* emplaceBack(Args&&... args);

    /// @param  sz Amount to append to tail
    /// @return Returns the old and new tail pointers used for populating the
    ///         newly expanded region
    Range forwardSize(int32_t sz);

    //  DEPRECATED - Long term goal of Buffer() is to eliminate the need
    //               to explicitly access these pointer values so that we're not
    //               relying on std::vector-ish (linear buffer) implementations
    const T* getHead() const { return head_; }
    T* getHead() { return head_; }
    const T* getTail() const { return tail_; }
    T* getTail() { return tail_; }

  protected:
    void resetInternal();
    void setTail(T* tail) { tail_ = tail; }
    T* getLimit() { return limit_; }
  private:
    void moveFrom(BufferBase& other);
    T* head_ = nullptr;
    T* tail_ = nullptr;
    T* limit_ = nullptr;
  };

  template<typename T>
  BufferBase<T>::BufferBase(T* data, int32_t limit, int32_t size) :
    head_(data),
    tail_(data),
    limit_(data + limit)
  {
    if (size > limit) {
      CK_ASSERT(size <= limit);
      size = limit;
    }
    tail_ = data + size;
  }

  template<typename T>
  void BufferBase<T>::moveFrom(BufferBase& other)
  {
    head_ = other.head_;
    tail_ = other.tail_;
    limit_ = other.limit_;
    other.head_ = nullptr;
    other.tail_ = nullptr;
    other.limit_ = nullptr;
  }

  template<typename T>
  void BufferBase<T>::resetInternal()
  {;
    tail_ = head_;
  }

  template<typename T>
  void BufferBase<T>::pushBack(T value)
  {
    CK_ASSERT_RETURN(tail_ < limit_);
    *tail_ = value;
    ++tail_;
  }

  template<typename T>
  template<class... Args>
  T* BufferBase<T>::emplaceBack(Args&&... args)
  {
    CK_ASSERT_RETURN_VALUE(tail_ < limit_, nullptr);

    T* obj = tail_++;
    return ::new(obj) T(std::forward<Args>(args)...);
  }

  template<typename T>
  auto BufferBase<T>::forwardSize(int32_t sz) -> Range
  {
    Range range;
    range.first = tail_;
    range.second = tail_ + sz;
    if (range.second > limit_ || range.second < head_) {
      CK_ASSERT(false);
      range.second = limit_;
    }
    tail_ = range.second;
    for (auto* t = range.first; t != range.second; ++t) {
      ::new(t) T();
    }
    return range;
  }

  template<typename T>
  T* BufferBase<T>::getNodeAtIndex(int32_t index)
  {
    return const_cast<T*>(static_cast<const ThisType*>(this)->getNodeAtIndex(index));
  }

  template<typename T>
  const T* BufferBase<T>::getNodeAtIndex(int32_t index) const
  {
    return (index < 0) ? getTail() - index : getHead() + index;
  }

  template<typename T>
  auto BufferBase<T>::getRange(BufferString bs) -> Range
  {
    if (bs.count < 0) { bs.count = getSize(); }
    CK_ASSERT(bs.index + bs.count <= getSize());
    auto endIndex = (bs.index + bs.count < getSize()) ? (bs.index + bs.count) : getSize();
    return Range { getNodeAtIndex(bs.index), getNodeAtIndex(endIndex) };
  }

  template<typename T>
  auto BufferBase<T>::getConstRange(BufferString bs) const -> ConstRange
  {
    if (bs.count < 0) { bs.count = getSize(); }
    CK_ASSERT(bs.index + bs.count <= getSize());
    auto endIndex = (bs.index + bs.count < getSize()) ? (bs.index + bs.count) : getSize();
    return ConstRange { getNodeAtIndex(bs.index), getNodeAtIndex(endIndex) };
  }

  template<typename T>
  BufferString BufferBase<T>::getStringFromRange(Range range) const
  {
    return BufferString {
      range.first ? calcIndex(range.first) : 0,
      calcIndex(range.second) - calcIndex(range.first)
    };
  }

  template<typename T>
  BufferString BufferBase<T>::getStringFromRange(ConstRange range) const
  {
    return BufferString {
      range.first ? calcIndex(range.first) : 0,
      calcIndex(range.second) - calcIndex(range.first)
    };
  }


  //////////////////////////////////////////////////////////////////////////////


  template<typename T, typename Enable=void>
  class Buffer;

  // allow for buffers with non trivial types to avoid calling destructor per
  // buffer object
  template<typename T>
  using BufferEnableIfTrivial = typename std::enable_if<
    std::is_trivially_destructible<T>::value>::type;

  template<typename T>
  using BufferEnableIfNonTrivial = typename std::enable_if<!std::is_trivially_destructible<T>::value>::type;

  template<typename T>
  class Buffer<T, BufferEnableIfTrivial<T> > : public BufferBase<T>
  {
  public:
    using BaseType = BufferBase<T>;
    using Range = typename BaseType::Range;
    using ConstRange = typename BaseType::ConstRange;
    using DataType = T;

    Buffer() = default;
    Buffer(T* data, int32_t limit, int32_t size=0);

    ///
    /// Resets buffer head and tail to start
    void reset();

    /// Rewinds tail to position.
    /// If rewind underflows the head, rewind will enforce setting tail to the
    /// current head
    ///
    /// @param cnt  Count of items to rewind.
    void rewind(int32_t cnt);
  };

  template<typename T>
  Buffer<T, BufferEnableIfTrivial<T> >::Buffer(T* data, int32_t limit, int32_t size) :
    BufferBase<T>(data, limit, size)
  {
  }

  template<typename T>
  void Buffer<T, BufferEnableIfTrivial<T> >::reset()
  {
    BaseType::resetInternal();
  }

  template<typename T>
  void Buffer<T, BufferEnableIfTrivial<T> >::rewind(int32_t cnt)
  {
    T* newTail = BaseType::getTail() - cnt;
    if (newTail < BaseType::getHead() || newTail > BaseType::getTail()) {
      CK_ASSERT(false);
      newTail = BaseType::getHead();
    }
    BaseType::setTail(newTail);
  }

  //////////////////////////////////////////////////////////////////////////////

  template<typename T>
  class Buffer<T, BufferEnableIfNonTrivial<T> > : public BufferBase<T>
  {
  public:
    using BaseType = BufferBase<T>;
    using Range = typename BaseType::Range;
    using ConstRange = typename BaseType::ConstRange;
    using DataType = T;

    Buffer() = default;
    Buffer(T* data, int32_t limit, int32_t size=0);
    ~Buffer();

    ///
    /// Resets buffer head and tail to start
    void reset();

    /// Rewinds tail to position.
    /// If rewind underflows the head, rewind will enforce setting tail to the
    /// current head
    ///
    /// @param cnt  Count of items to rewind.
    void rewind(int32_t cnt);
  };


  template<typename T>
  Buffer<T, BufferEnableIfNonTrivial<T> >::Buffer(T* data, int32_t limit, int32_t size) :
    BufferBase<T>(data, limit, size)
  {
  }

  template<typename T>
  Buffer<T, BufferEnableIfNonTrivial<T> >::~Buffer()
  {
    reset();
  }

  template<typename T>
  void Buffer<T, BufferEnableIfNonTrivial<T> >::reset()
  {
    rewind(BaseType::getSize());
    BaseType::resetInternal();
  }

  template<typename T>
  void Buffer<T, BufferEnableIfNonTrivial<T> >::rewind(int32_t cnt)
  {
    T* tail = BaseType::getTail();
    T* head = BaseType::getHead();
    T* newTail = tail - cnt;
    if (newTail < head || newTail > tail) {
      CK_ASSERT(false);
      newTail = head;
    }

    while (tail > newTail) {
      --tail;
      tail->~T();
    }
    BaseType::setTail(tail);
  }

  //////////////////////////////////////////////////////////////////////////////

  template<typename T> cinek::Range<T> copy(cinek::Range<T> dest,
                                            cinek::ConstRange<T> source) {
    auto sourceIt = source.first;
    auto destIt = dest.first;
    for (; sourceIt < source.second && destIt < dest.second; ++sourceIt, ++destIt) {
      *destIt = *sourceIt;
    }
    dest.second = destIt;
    return dest;
  }

  using ByteBuffer = Buffer<uint8_t>;
  using CharBuffer = Buffer<char>;
  //  specialized version that behaves like malloc()
  template<> auto BufferBase<uint8_t>::forwardSize(int32_t sz) -> Range;
  //  specialized version that allocates aligned strings by architecture
  //  (vs malloc)
  template<> auto BufferBase<char>::forwardSize(int32_t sz) -> Range;
}

#endif
