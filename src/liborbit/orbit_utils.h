//
//  orbit_utils.h
//  OrbitVM
//
//  Created by Cesar Parent on 06/12/2016.
//  Copyright © 2016 cesarparent. All rights reserved.
//
#ifndef orbit_utils_h
#define orbit_utils_h

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

typedef struct _String {
    const char* data;
    size_t      length;
    uint32_t    hash;
} String;

#define ALLOC(type) \
    (type*)calloc(sizeof(type), 1)
#define ALLOC_ARRAY(type, count) \
    (type*)calloc(sizeof(type), (count))
#define ALLOC_FLEX(type, arrayType, count) \
    (type*)calloc(sizeof(type) + (sizeof(arrayType) * (count)), 1)
#define DEALLOC(ptr) \
    free(ptr)

//
void* orbit_realloc(void* ptr, size_t newSize);

// Hash functions
    
// Computes the FNV-1a hash of [string].
// This is O(n) complexity and should be used lightly. 
uint32_t orbit_hashString(const char* string, size_t length);

// Computes the hash code of [number].
uint32_t orbit_hashDouble(double number);
        
        
// Debugging facilities. When compiling a release build, OASSERT is a no-op to
// speed up the interpreter.

#ifdef NDEBUG
#define OASSERT(expr, message)
#else
#define OASSERT(expr, message)                                      \
do {                                                                \
    if(!(expr)) {                                                   \
        fprintf(stderr, "[%s:%d] Assert failed in %s(): %s\n",      \
                __FILE__, __LINE__, __func__, message);             \
        abort();                                                    \
    }                                                               \
} while(0)
#endif
    
// DBG puts out console messages, with file, line number and function name when
// running on non-release builds

#ifdef NDEBUG
#define DBG(fmt, ...)
#else
#define DBG(fmt, ...) fprintf(stderr, "[%s:%d] %s(): " fmt "\n",    \
    __FILE__, __LINE__, __func__ , ##__VA_ARGS__)
#endif

#endif /* orbit_utils_h */
