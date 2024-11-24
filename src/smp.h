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

/**
 * @brief Creates and initializes a static pool and its memory.
 * 
 * @param pool_name The name of the pool.
 * @param pool_size The size of the pool.
 */
#define SMP_POOL(pool_name, pool_size)                                      \
    static union                                                            \
    {                                                                       \
        smp_byte_t raw[pool_size];                                          \
        struct                                                              \
        {                                                                   \
            smp_chunk_t header;                                             \
            smp_byte_t padding[pool_size - sizeof(smp_chunk_t)];            \
        };                                                                  \
    } pool_name##_memory =                                                  \
    {                                                                       \
        .raw = {0},                                                         \
        .header =                                                           \
        {                                                                   \
            .size = pool_size - sizeof(smp_chunk_t),                        \
            .available = 1,                                                 \
            .next_offset = 0                                                \
        }                                                                   \
    };                                                                      \
    static smp_pool_t pool_name =                                           \
    {                                                                       \
        .memory = pool_name##_memory.raw,                                   \
        .size = pool_size,                                                  \
        .head = &pool_name##_memory.header                                  \
    };

/**
 * @brief Generates an API for the pool.
 * Generated functions include alloc, calloc and dealloc.
 * SMP_POOL() should be called before.
 * 
 * @param pool_name The name of the pool.
 */
#define SMP_API(pool_name)                                                  \
    static void* pool_name##_alloc(size_t size)                             \
    {                                                                       \
        return smp_alloc(&pool_name, size);                                 \
    }                                                                       \
    static void* pool_name##_calloc(size_t nitems, size_t size)             \
    {                                                                       \
        return smp_calloc(&pool_name, nitems, size);                        \
    }                                                                       \
    static void pool_name##_dealloc(void* ptr)                              \
    {                                                                       \
        return smp_dealloc(&pool_name, ptr);                                \
    }

/**
 * @brief Creates and initializes a static pool and its memory and generates
 * an API.
 * 
 * @param pool_name The name of the pool.
 * @param pool_size The size of the pool.
 */
#define SMP_POOL_WITH_API(pool_name, pool_size)                             \
    SMP_POOL(pool_name, pool_size)                                          \
    SMP_API(pool_name)

typedef uint8_t smp_byte_t;
typedef void* smp_ptr_t;
typedef size_t smp_size_t;

// Structure holding the chunk metadata
// This structure is inside the memory pool for every individual chunk
typedef struct smp_chunk smp_chunk_t;
typedef struct smp_chunk
{
    uint32_t size : 31;
    uint32_t available : 1;
    uint32_t next_offset;
} smp_chunk_t;

// Structure holding the pool metadata
typedef struct smp_pool
{
    smp_byte_t* memory;
    smp_size_t size;
    smp_chunk_t* head; // Pointer to the first free chunk
} smp_pool_t;

/**
 * @brief Allocates memory from the pool.
 * 
 * @param pool The pool to allocate memory from.
 * @param size The size of the allocated memory.
 * @return Pointer to the allocated memory or NULL on failure.
 */
smp_ptr_t smp_alloc(smp_pool_t* pool, smp_size_t size);

/**
 * @brief Allocates contiguous memory from the pool.
 * 
 * @param pool The pool to allocate memory from.
 * @param nitems The number of contiguous items.
 * @param size The size of an item.
 * @return Pointer to the allocated memory or NULL on failure.
 */
smp_ptr_t smp_calloc(smp_pool_t* pool, smp_size_t nitems, smp_size_t size);

/**
 * @brief Deallocates memory from the pool.
 * 
 * @param pool The pool to deallocate memory from.
 * @param ptr Pointer to the memory to deallocate.
 */
void smp_dealloc(smp_pool_t* pool, smp_ptr_t ptr);

/**
 * @brief Returns the size of the allocated memory.
 * 
 * @param pool The pool of the allocated memory.
 * @param ptr Pointer to the memory to get the size of.
 * @return The size of the memory, or 0 on failure.
 */
smp_size_t smp_size(smp_pool_t* pool, smp_ptr_t ptr);

#endif /* SMP_H */