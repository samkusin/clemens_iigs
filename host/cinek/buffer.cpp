/**
 * \file    buffer.cpp
 *
 *
 * \note    Created by Samir Sinha on 1/19/20.
 *          Copyright (c) 2020 Cinekine. All rights reserved.
 */

#include "buffer.hpp"

namespace cinek {

  //  specialization to ensure alignment based on malloc style behavior
  template<>
  auto BufferBase<uint8_t>::forwardSize(int32_t sz) -> Range
  {
    Range range;
    constexpr uintptr_t kAlign = CK_ARCH_MALLOC_ALIGN_BYTES;
    size_t offset = (uint8_t*)CK_ALIGN_PTR(tail_, kAlign) - tail_;
    // TODO: Reevaluate this equation - seems we'll always allocate another
    // pool block if our remaining bytes == memSize, since offset in that
    // case will return 'align'.
    if (offset == kAlign) offset = 0;

    range.first = tail_ + offset;
    range.second = range.first + sz;
    if (range.second > limit_ || range.second < head_) {
      CK_ASSERT(false);
      range.second = limit_;
    }
    tail_ = range.second;
    /* - DO NOT ZERO OUT memory
    for (auto* t = range.first; t != range.second; ++t) {
      ::new(t) uint8_t();
    }
    */
    return range;
  }

  template<>
  auto BufferBase<char>::forwardSize(int32_t sz) -> Range
  {
    Range range;
    constexpr uintptr_t kAlign = CK_ARCH_ALIGN_BYTES;
    size_t offset = (char*)CK_ALIGN_PTR(tail_, kAlign) - tail_;
    // TODO: Reevaluate this equation - seems we'll always allocate another
    // pool block if our remaining bytes == memSize, since offset in that
    // case will return 'align'.
    if (offset == kAlign) offset = 0;

    range.first = tail_ + offset;
    range.second = range.first + sz;
    if (range.second > limit_ || range.second < head_) {
      CK_ASSERT(false);
      range.second = limit_;
    }
    tail_ = range.second;
    /* - DO NOT ZERO OUT memory
    for (auto* t = range.first; t != range.second; ++t) {
      ::new(t) uint8_t();
    }
    */
    return range;
  }

}
