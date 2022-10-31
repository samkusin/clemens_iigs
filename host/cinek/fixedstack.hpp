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
 * @file    cinek/memorystack.hpp
 * @author  Samir Sinha
 * @date    1/12/2020
 * @brief   Object allocation within a fixed heap
 * @copyright Cinekine
 */

#ifndef CINEK_FIXED_STACK_HPP
#define CINEK_FIXED_STACK_HPP

#include "ckdefs.h"

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <utility>

namespace cinek {

  /**
   * @class FixedStack
   * @brief Implements a simple stack-based fixed buffer pool.
   *
   * Note, there is no garbage collection if a FixedStack() is released.  It's
   * generally good practice to avoid object allocation within fixed stacks
   * except in very controlled circumstances (i.e. PODs, or controlling code
   * that creates/destroys any complex objects.)
   *
   * NOT THREADSAFE!
   */
  class FixedStack
  {
    CK_CLASS_NON_COPYABLE(FixedStack)

  public:
    FixedStack() = default;
    /**
     * Constructor initializing the fixed pool.
     * The size is adjusted to align with the architecture's byte alignment.
     * @param initSize  The initial memory pool size.
     * @param buffer    The raw memory (should be aligned)
     */
    FixedStack(size_t initSize, void* buffer) :
      first_(buffer),
      last_(buffer),
      limit_(reinterpret_cast<void*>(uintptr_t(buffer) + initSize))
    {
      assert(initSize == CK_ALIGN_SIZE_TO_ARCH(initSize));
    }

    /** @cond */
    FixedStack(FixedStack&& other) :
      first_(other.first_),
      last_(other.last_),
      limit_(other.limit_)
    {
      other.first_ = nullptr;
      other.last_ = nullptr;
      other.limit_ = nullptr;
    }
    FixedStack& operator=(FixedStack&& other)
    {
      first_ = other.first_;
      last_ = other.last_;
      limit_ = other.limit_;
      other.first_ = nullptr;
      other.last_ = nullptr;
      other.limit_ = nullptr;
      return *this;
    }
    /** @endcond */
    /**
     * @return The size of the stack.
     */
    size_t capacity() const {
      return size_t(uintptr_t(limit_) - uintptr_t(first_));
    }
    /**
     * Calculates the bytes allocated from the pool.
     * @return The number of bytes allocated
     */
    size_t size() const {
      return size_t(uintptr_t(last_) - uintptr_t(first_));
    }
    /**
     * Calculates the bytes remaining in the pool.
     * @return The number of unallocated bytes
     */
    size_t remaining() const {
       return capacity() - size();
    }
    /**
     * Allocates a block of memory
     * @param  memSize The number of bytes to allocate
     * @param  align   What byte alignment to use for the returned pointer
     * @return Pointer to the allocated block or nullptr if out of memory.
     */
    void* allocate(size_t memSize, size_t align = CK_ARCH_MALLOC_ALIGN_BYTES) {
      size_t offset = size_t(
      CK_ALIGN_PTR(uintptr_t(last_), align) - uintptr_t(last_));

      if (offset == align) offset = 0;
      if (remaining() < offset + memSize) {
        assert(false);     // out of memory
        return nullptr;
      }

      void* p = reinterpret_cast<void*>(uintptr_t(last_) + offset);
      last_ = reinterpret_cast<void*>(uintptr_t(p) + memSize);
      return p;
    }
    /**
     * Allocates a block of *uninitialized* data objects
     * @param   count   The number of plain objects of type T to allocate
     * @param  align   What byte alignment to use for the returned pointer
     * @return Pointer to the allocated C array or nullptr if out of memory.
     *
     * If the objects of type T are not POD/trivial/etc then it is the callers
     * responsibility to placement new and explicitly destruct any objects it
     * uses from this array.
     */
    template<typename T> T* allocateArray(
      size_t count, size_t align = CK_ARCH_MALLOC_ALIGN_BYTES);
    /**
     * Allocate and construct a block of type T.
     * The returned pointer is aligned
     * @return Pointer to the constructed object or nullptr if out of memory
     */
    template<typename T, typename... Args> T* newItem(Args&&... args);
    template<typename T> void deleteItem(T* item);
    /**
     * Resets the stack to the head
     */
    void reset() {
      last_ = first_;
    }

    /**
     * @brief Get the pointer to the raw data buffer supplied in the constructor
     *
     * @return void* This should be used to retrieve the pointer passed into
     *               the constructor only for deallocation purposes
     */
    void* getHead() { return first_; }


  private:
    void* first_ = nullptr;
    void* last_ = nullptr;
    void* limit_ = nullptr;
  };

  template<typename T, typename... Args>
  T* FixedStack::newItem(Args&&... args)
  {
    void* p = allocate(sizeof(T));
    return ::new(p) T(std::forward<Args>(args)...);
  }

  template<typename T>
  void FixedStack::deleteItem(T* item)
  {
    item->~T();
  }

  template<typename T>
  T* FixedStack::allocateArray(size_t count, size_t align)
  {
    void* data = allocate(sizeof(T) * count, align);
    return reinterpret_cast<T*>(data);
  }

}   // namespace cinek

#endif
