/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2012 Cinekine Media
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
 * @file    cinek/ckdefs.h
 * @author  Samir Sinha
 * @date    11/1/2013
 * @brief   C and C++ common macros and definitions
 * @copyright Cinekine
 */

#ifndef CINEK_DEFS_H
#define CINEK_DEFS_H

/* #ifdef CINEK_HAS_CKOPTS_H */
#include "ckopts.h"
/* #endif */

/**
 *  \name Compiler Defines
 *  Defines properties related to the build compiler system.
 */
/**@{*/

#ifdef _MSC_VER
#define CK_COMPILER_ 1
#else
#define CK_COMPILER_MSVC 0
#endif

#if __clang__
#define CK_COMPILER_CLANG 1
#else
#define CK_COMPILER_CLANG 0
#endif

#if defined(__GNUC__)
#define CK_COMPILER_GNUC 1
#else
#define CK_COMPILER_GNUC 0
#endif

/**
 * \def CK_COMPILER_HAS_STDINT
 * Set to 1 if available, else set to 0 if not available.
 */
#ifndef CK_COMPILER_HAS_STDINT
#if CK_COMPILER_MSVC
#if _MSC_VER < 1600
#define CK_COMPILER_HAS_STDINT 0
#else
#define CK_COMPILER_HAS_STDINT 1
#endif
#else
#define CK_COMPILER_HAS_STDINT 1
#endif
/* CK_COMPILER_HAS_STDINT */
#endif

#ifdef __cplusplus
/**
 * \def CK_CPP_EXCEPTIONS
 * Define if C++ exception handling is enabled.
 */
#if !defined(CK_CPP_EXCEPTIONS) || !CK_CPP_EXCEPTIONS
#if CK_COMPILER_CLANG
#if __has_feature(cxx_exceptions)
#define CK_CPP_EXCEPTIONS 1
#else
#define CK_CPP_EXCEPTIONS 0
#endif
#elif CK_COMPILER_MSVC
#ifdef _CPPUNWIND
#define CK_CPP_EXCEPTIONS 1
#else
#undef _HAS_EXCEPTIONS
#define _HAS_EXCEPTIONS 0
#endif
#elif CK_COMPILER_GNUC
#if defined(__EXCEPTIONS) || defined(__cpp_exceptions)
#define CK_CPP_EXCEPTIONS 1
#else
#define CK_CPP_EXCEPTIONS 0
#endif
#endif
#define CK_NOEXCEPT
#else
#define CK_NOEXCEPT noexcept
#endif

#endif /* __cplusplus */

/**@}*/

/**
 *  \name Platform Defines
 *  Defines the target development platform
 */
/**@{*/
#ifdef CK_DOXYGEN_RUNNING_PASS
/**
 * \def CK_TARGET_OSX
 * Compiling for OS X
 */
#define CK_TARGET_OSX
/**
 * \def CK_TARGET_LINUX
 * Compiling for Linux
 */
#define CK_TARGET_LINUX
/**
 * \def CK_TARGET_WINDOWS
 * Compiling for a Windows desktop OS.
 */
#define CK_TARGET_WINDOWS
/**
 * \def CK_TARGET_IOS
 * Compiling for the iOS Platform.
 */
#define CK_TARGET_IOS
/**
 * \def CK_TARGET_IOS_SIMULATOR
 * Compiling specifically for the iPhone Simulator.
 */
#define CK_TARGET_IOS_SIMULATOR

#endif /* CK_DOXYGEN_RUNNING_PASS */
/**@}*/

#ifdef __APPLE__
#include "TargetConditionals.h"
#if TARGET_OS_IPHONE
#define CK_TARGET_IOS
#elif TARGET_IPHONE_SIMULATOR
#define CK_TARGET_IOS_SIMULATOR
#elif TARGET_OS_MAC
#define CK_TARGET_OSX
#endif
#elif __linux__
#define CK_TARGET_LINUX
#elif _WIN32
#define CK_TARGET_WINDOWS
#else
#error "Detected an unsupported platform."
#endif

/**
 *  \def CK_ALIGN_SIZE(_val, _align_)
 *  Returns a size value based on _val_, and aligned using the specified _align_ value.
 */
#define CK_ALIGN_SIZE(_val_, _align_) (((_val_) + (_align_)-1) & ~((_align_)-1))
/**
 * \def CK_ALIGN_PTR(_ptr_, _align_)
 * Returns an adjusted pointer based on _ptr_, aligned by _align_ bytes.
 */
#define CK_ALIGN_PTR(_ptr_, _align_) (((uintptr_t)(_ptr_) + (_align_)-1) & ~((_align_)-1))

/**
 * \def CK_ARCH_ALIGN_BYTES
 * The architecture specific alignment value (in bytes.)
 */
#define CK_ARCH_ALIGN_BYTES sizeof(void *)
/**
 *  \def CK_ALIGN_SIZE_TO_ARCH(_val_)
 *  Invokes CK_ALIGN_SIZE with the platform's predefined alignment value.
 */
#define CK_ALIGN_SIZE_TO_ARCH(_val_) CK_ALIGN_SIZE((_val_), CK_ARCH_ALIGN_BYTES)
/**
 *  \def CK_ALIGN_PTR_TO_ARCH(_ptr_)
 *  Invokes CK_ALIGN_PTR with the platform's predefined alignment value.
 */
#define CK_ALIGN_PTR_TO_ARCH(_ptr_) CK_ALIGN_PTR((_ptr_), CK_ARCH_ALIGN_BYTES)

/**
 * \def CK_ARCH_MALLOC_ALIGN_BYTES
 * The architecture specific alignment value (in bytes) for malloc style blocks,
 * using the GNU behavior as standard since many libraries seem to assume this
 * type of alignment.
 */
#define CK_ARCH_MALLOC_ALIGN_BYTES (CK_ARCH_ALIGN_BYTES * 2)

/**
 * \def DO_PRAGMA(_x_)
 * Adds a compiler pragma.
 */
#define DO_PRAGMA(_x_) __pragma(_x_)

/**
 * \def CK_C_S_(_plaintext_)
 * Stringification
 */
#define CK_C_S(__ck_s)  #__ck_s
#define CK_C_S_(__ck_s) CK_C_S(__ck_s)

#if CK_COMPILER_CLANG
#define CK_FILENAME __BASE_FILE__
#else
#define CK_FILENAME __FILE__
#endif

#define CK_C_LINE__ CK_C_S_(__LINE__)

/**@{*/
/**
 * \def TODO(_x_)
 * Adds a TODO message to the compile process.
 */
#define CK_TODO(_x_) DO_PRAGMA(message(CK_FILENAME ":" CK_C_LINE__ " TODO: " #_x_))
/**@}*/

#ifdef __cplusplus
/**
 *  \name C++ Macros
 *  Useful macros for C++ development.
 */
/**@{*/
/**
 * \def CK_CLASS_NON_COPYABLE(__class_name_)
 * Prevents copy operations for the specified class.  This must be placed
 * the C++ class definition.
 */
#define CK_CLASS_NON_COPYABLE(__class_name_)                                                       \
    __class_name_(const __class_name_ &) = delete;                                                 \
    __class_name_ &operator=(const __class_name_ &) = delete;

template <typename... Ts> struct ck_sizeof_max;

template <> struct ck_sizeof_max<> {
    enum { size = 0 };
};

template <typename T0, typename... Ts> struct ck_sizeof_max<T0, Ts...> {
    enum {
        size = sizeof(T0) < ck_sizeof_max<Ts...>::size ? ck_sizeof_max<Ts...>::size : sizeof(T0)
    };
};

/**@}*/
#endif

#if defined(_MSC_VER)
#define strncasecmp _strnicmp
#define strcasecmp  _stricmp
#endif

typedef double CKTime;
typedef double CKTimeDelta;

/* CINEK_DEFS_H */
#endif
