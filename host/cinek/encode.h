/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Cinekine Media
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
 * @file    cinek/encode.h
 * @author  Samir Sinha
 * @date    6/13/2019
 * @brief   BaseN encoding and decoding of buffers
 * @copyright Cinekine
 */

#ifndef CINEK_ENCODE_H
#define CINEK_ENCODE_H


#ifdef __cplusplus
extern "C" {
#endif

unsigned int base64_encode_len(unsigned int src_len);

unsigned int base64_encode(
  char* out,
  const unsigned char* src,
  unsigned int len
);

unsigned int base64_decode_len(const char* src, unsigned int len);

unsigned int base64_decode(
  unsigned char* out,
  const char *src,
  unsigned int len
);

unsigned int cinek_encode_hex(
  char* result,
  unsigned int size,
  const unsigned char* source,
  unsigned int sourceSize
);

unsigned int cinek_decode_hex(
  unsigned char* result,
  unsigned int size,
  const char* source
);

#ifdef __cplusplus
}
#endif

#endif