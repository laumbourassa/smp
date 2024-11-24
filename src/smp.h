/*
 * smp.h - Static Memory Pool interface
 * 
 * Copyright (c) 2024 Laurent Mailloux-Bourassa
 * 
 * This file is part of the Static Memory Pool (SMP) library.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SMP_H
#define SMP_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define SMP_POOL(pool_name, pool_size)                                      \
    union                                                                   \
    {                                                                       \
        smp_byte_t raw[pool_size];                                          \
        struct                                                              \
        {                                                                   \
            smp_header_t header;                                            \
            smp_byte_t padding[pool_size - sizeof(smp_header_t)];           \
        };                                                                  \
    } pool_name##_memory =                                                  \
    {                                                                       \
        .raw = {0},                                                         \
        .header =                                                           \
        {                                                                   \
            .size = pool_size - sizeof(smp_header_t),                       \
            .available = 1,                                                 \
            .next_offset = 0                                                \
        }                                                                   \
    };                                                                      \
    smp_pool_t pool_name =                                                  \
    {                                                                       \
        .memory = pool_name##_memory.raw,                                   \
        .size = pool_size,                                                  \
        .head = &pool_name##_memory.header                                  \
    };

#define SMP_API(pool_name)                                                  \
    void* pool_name##_alloc(size_t size)                                    \
    {                                                                       \
        return smp_alloc(&pool_name, size);                                 \
    }                                                                       \
    void* pool_name##_calloc(size_t nitems, size_t size)                    \
    {                                                                       \
        return smp_calloc(&pool_name, nitems, size);                        \
    }                                                                       \
    void pool_name##_dealloc(void* ptr)                                     \
    {                                                                       \
        return smp_dealloc(&pool_name, ptr);                                \
    }

#define SMP_POOL_WITH_API(pool_name, pool_size)                             \
    SMP_POOL(pool_name, pool_size)                                          \
    SMP_API(pool_name)

typedef uint8_t smp_byte_t;
typedef void* smp_ptr_t;
typedef size_t smp_size_t;

typedef struct smp_header smp_header_t;
typedef struct smp_header
{
    uint32_t size : 31;
    uint32_t available : 1;
    uint32_t next_offset;
} smp_header_t;

typedef struct smp_pool
{
    smp_byte_t* memory;
    smp_size_t size;
    smp_header_t* head;
} smp_pool_t;

smp_ptr_t smp_alloc(smp_pool_t* pool, smp_size_t size);
smp_ptr_t smp_calloc(smp_pool_t* pool, smp_size_t nitems, smp_size_t size);
void smp_dealloc(smp_pool_t* pool, smp_ptr_t ptr);

smp_size_t smp_size(smp_pool_t* pool, smp_ptr_t ptr);

#endif /* SMP_H */