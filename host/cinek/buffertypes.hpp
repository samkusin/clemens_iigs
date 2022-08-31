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
 * @date    7/7/2020
 * @brief   essential types for buffer
 * @copyright Cinekine
 */

#ifndef CINEK_V2_BUFFER_TYPES_HPP
#define CINEK_V2_BUFFER_TYPES_HPP

#include <cstdint>
#include <utility>

namespace cinek {

  struct BufferString
  {
    BufferString() = default;
    BufferString(int32_t i, int32_t c): index(i), count(c) {}
    explicit operator bool() const { return index >= 0 && count != 0; }
    int32_t index = 0;
    int32_t count = 0;
  };

  template<typename T> using Range = std::pair<T*,T*>;
  template<typename T> using ConstRange = std::pair<const T*,const T*>;
  template<typename T> ConstRange<T> ConstCastRange(Range<T>& range) {
    return ConstRange<T> { range.first, range.second };
  }

  template<typename T> int32_t length(Range<T> range) {
    return static_cast<int32_t>(range.second - range.first);
  }

  template<typename T> int32_t length(ConstRange<T> range) {
    return static_cast<int32_t>(range.second - range.first);
  }

  template<typename T> class BufferBase;
  template<typename T, typename Enable> class Buffer;

} // namespace cinek

#endif
