/*
 * smp.c - Static Memory Pool implementation
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

#include <string.h>
#include "smp.h"

#define SMP_COALESCE_CHUNKS(a, b)                                   \
    {                                                               \
        a->size = a->size + b->size + sizeof(smp_chunk_t);          \
        a->offset = SMP_GET_RELATIVE_OFFSET(                        \
                SMP_GET_CHUNK_FROM_OFFSET(b->offset, b), a)         \
                ?: a->offset;                                       \
            memset(b, 0, sizeof(smp_chunk_t));                      \
    }

#define SMP_GET_CHUNK_FROM_OFFSET(offset, relative_to)  \
    ((smp_chunk_t*) ((smp_byte_t*) relative_to + offset)) ?: NULL

#define SMP_GET_RELATIVE_OFFSET(chunk, relative_to)                     \
    ((uint32_t)(((smp_byte_t*)(chunk) > (smp_byte_t*)(relative_to)) ?   \
    ((smp_byte_t*)(chunk) - (smp_byte_t*)(relative_to)) : 0))

#define SMP_GET_PTR_FROM_CHUNK(chunk)    \
    ((smp_byte_t*) chunk + sizeof(smp_chunk_t))

#define SMP_GET_CHUNK_FROM_PTR(ptr) \
    ((smp_chunk_t*) ((smp_byte_t*) ptr - sizeof(smp_chunk_t)))

#define SMP_VALIDATE_CHUNK(chunk)   \
    (chunk->magic == SMP_MAGIC)

smp_ptr_t smp_alloc(smp_pool_t* pool, smp_size_t size)
{
    if (!pool) return NULL;
    
    smp_chunk_t* chunk = pool->head;
    smp_chunk_t* prev = NULL;
    
    while (chunk)
    {
        if (chunk->size < size)
        {
            prev = chunk;
            chunk = SMP_GET_CHUNK_FROM_OFFSET(chunk->offset, chunk);
            continue;
        }
        
        smp_size_t remaining_size = chunk->size - size;
        smp_chunk_t* new_chunk = NULL;
        
        if (remaining_size > sizeof(smp_chunk_t))
        {
            new_chunk = SMP_GET_CHUNK_FROM_OFFSET(size + sizeof(smp_chunk_t), chunk);
            new_chunk->size = remaining_size - sizeof(smp_chunk_t);
            new_chunk->free = 1;
            new_chunk->offset = SMP_GET_RELATIVE_OFFSET(SMP_GET_CHUNK_FROM_OFFSET(chunk->offset, chunk), new_chunk);
            new_chunk->magic = SMP_MAGIC;
            chunk->size = size;
        }
        
        if (prev)
        {
            prev->offset = SMP_GET_RELATIVE_OFFSET(new_chunk, prev);
        }
        else if (new_chunk)
        {
            pool->head = new_chunk;
        }
        else
        {
            pool->head = SMP_GET_CHUNK_FROM_OFFSET(chunk->offset, chunk);
        }
        
        chunk->free = 0;
        chunk->offset = 0;
        
        return SMP_GET_PTR_FROM_CHUNK(chunk);
    }
    
    return NULL;
}

smp_ptr_t smp_calloc(smp_pool_t* pool, smp_size_t nitems, smp_size_t size)
{
    if (!pool) return NULL;
    if (nitems && size > (SIZE_MAX / nitems)) return NULL;
    
    return smp_alloc(pool, nitems * size);
}

void smp_dealloc(smp_pool_t* pool, smp_ptr_t ptr)
{
    if (!pool || !ptr) return;
    if (ptr < (smp_ptr_t) pool->memory || ptr >= (smp_ptr_t) (pool->memory + pool->size)) return;
    
    smp_chunk_t* chunk = SMP_GET_CHUNK_FROM_PTR(ptr);
    
    if (!SMP_VALIDATE_CHUNK(chunk)) return;
    
    chunk->free = 1;
    memset(ptr, 0, chunk->size);
    
    if (!pool->head)
    {
        pool->head = chunk;
        return;
    }
    
    if (chunk < pool->head)
    {
        // Check if we can coalesce the current and next chunk
        if ((smp_chunk_t*) ((smp_byte_t*) &chunk[1] + chunk->size) == pool->head)
        {
            SMP_COALESCE_CHUNKS(chunk, pool->head);

            if (chunk->size == pool->size - sizeof(smp_chunk_t))
            {
                chunk->offset = 0;
            }
        }
        else
        {
            chunk->offset = SMP_GET_RELATIVE_OFFSET(pool->head, chunk);
        }
        
        pool->head = chunk;
        return;
    }

    // Find the previous free chunk and make it point to this chunk
    smp_chunk_t* prev = pool->head;
    smp_chunk_t* next = NULL;
    
    while (prev->offset)
    {
        next = SMP_GET_CHUNK_FROM_OFFSET(prev->offset, prev);
        
        if (next > chunk) break;
        
        prev = next;
    }
    
    // Check if we can coalesce the previous and current chunk
    if ((smp_chunk_t*) ((smp_byte_t*) &prev[1] + prev->size) == chunk)
    {
        SMP_COALESCE_CHUNKS(prev, chunk);
        chunk = prev;
    }
    
    if (!next) return;
    
    // Check if we can coalesce the current and next chunk
    if ((smp_chunk_t*) ((smp_byte_t*) &chunk[1] + chunk->size) == next)
    {
        SMP_COALESCE_CHUNKS(chunk, next);

        if (chunk->size == pool->size - sizeof(smp_chunk_t))
        {
            chunk->offset = 0;
        }
        else if (chunk != prev)
        {
            prev->offset = SMP_GET_RELATIVE_OFFSET(chunk, prev);
        }
    }
    else
    {
        chunk->offset = SMP_GET_RELATIVE_OFFSET(next, chunk);
    }
}

smp_size_t smp_size(smp_pool_t* pool, smp_ptr_t ptr)
{
    if (!pool || !ptr) return 0;
    if (ptr < (smp_ptr_t) pool->memory || ptr >= (smp_ptr_t) (pool->memory + pool->size)) return 0;
    
    smp_chunk_t* chunk = SMP_GET_CHUNK_FROM_PTR(ptr);

    if (!SMP_VALIDATE_CHUNK(chunk)) return 0;
    
    return chunk->size;
}
