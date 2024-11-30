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
#include <stdbool.h>
#include "smp.h"

#define SMP_FORCE_INLINE    inline __attribute__((always_inline))

static SMP_FORCE_INLINE void _smp_coalesce_blocks(smp_block_t* a, smp_block_t* b);
static SMP_FORCE_INLINE smp_block_t* _smp_get_block_from_offset(uint32_t offset, smp_block_t* relative_to);
static SMP_FORCE_INLINE uint32_t _smp_get_relative_offset(smp_block_t* block, smp_block_t* relative_to);
static SMP_FORCE_INLINE smp_byte_t* _smp_get_ptr_from_block(smp_block_t* block);
static SMP_FORCE_INLINE smp_block_t* _smp_get_block_from_ptr(smp_byte_t* ptr);
static SMP_FORCE_INLINE bool _smp_validate_block(smp_block_t* block);

smp_ptr_t smp_alloc(smp_pool_t* pool, smp_size_t size)
{
    if (!pool) return NULL;
    
    smp_block_t* block = pool->head;
    smp_block_t* prev = NULL;
    
    while (block)
    {
        if (block->size < size)
        {
            prev = block;
            block = _smp_get_block_from_offset(block->offset, block);
            continue;
        }
        
        smp_size_t remaining_size = block->size - size;
        smp_block_t* new = NULL;
        
        if (remaining_size > sizeof(smp_block_t))
        {
            new = _smp_get_block_from_offset(size + sizeof(smp_block_t), block);
            new->magic = SMP_MAGIC;
            new->size = remaining_size - sizeof(smp_block_t);
            new->free = 1;
            new->offset = _smp_get_relative_offset(_smp_get_block_from_offset(block->offset, block), new);
            block->size = size;
        }
        
        if (prev)
        {
            prev->offset = _smp_get_relative_offset(new, prev);
        }
        else if (new)
        {
            pool->head = new;
        }
        else
        {
            pool->head = _smp_get_block_from_offset(block->offset, block);
        }
        
        block->free = 0;
        block->offset = 0;
        
        return _smp_get_ptr_from_block(block);
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
    
    smp_block_t* block = _smp_get_block_from_ptr(ptr);
    
    if (!_smp_validate_block(block)) return;
    
    block->free = 1;
    memset(ptr, 0, block->size);
    
    if (!pool->head)
    {
        pool->head = block;
        return;
    }
    
    if (block < pool->head)
    {
        // Check if we can coalesce the current and next block
        if ((smp_block_t*) ((smp_byte_t*) &block[1] + block->size) == pool->head)
        {
            _smp_coalesce_blocks(block, pool->head);

            if (block->size == pool->size - sizeof(smp_block_t))
            {
                block->offset = 0;
            }
        }
        else
        {
            block->offset = _smp_get_relative_offset(pool->head, block);
        }
        
        pool->head = block;
        return;
    }

    // Find the previous free block and make it point to this block
    smp_block_t* prev = pool->head;
    smp_block_t* next = NULL;
    
    while (prev->offset)
    {
        next = _smp_get_block_from_offset(prev->offset, prev);
        
        if (next > block) break;
        
        prev = next;
    }
    
    // Check if we can coalesce the previous and current block
    if ((smp_block_t*) ((smp_byte_t*) &prev[1] + prev->size) == block)
    {
        _smp_coalesce_blocks(prev, block);
        block = prev;
    }
    
    if (!next) return;
    
    // Check if we can coalesce the current and next block
    if ((smp_block_t*) ((smp_byte_t*) &block[1] + block->size) == next)
    {
        _smp_coalesce_blocks(block, next);

        if (block->size == pool->size - sizeof(smp_block_t))
        {
            block->offset = 0;
        }
        else if (block != prev)
        {
            prev->offset = _smp_get_relative_offset(block, prev);
        }
    }
    else
    {
        block->offset = _smp_get_relative_offset(next, block);
    }
}

smp_size_t smp_size(smp_pool_t* pool, smp_ptr_t ptr)
{
    if (!pool || !ptr) return 0;
    if (ptr < (smp_ptr_t) pool->memory || ptr >= (smp_ptr_t) (pool->memory + pool->size)) return 0;
    
    smp_block_t* block = _smp_get_block_from_ptr(ptr);

    if (!_smp_validate_block(block)) return 0;
    
    return block->size;
}

static SMP_FORCE_INLINE void _smp_coalesce_blocks(smp_block_t* a, smp_block_t* b)
{
    a->size = a->size + b->size + sizeof(smp_block_t);
    a->offset = _smp_get_relative_offset(_smp_get_block_from_offset(b->offset, b), a) ?: a->offset;
    memset(b, 0, sizeof(smp_block_t));  
}

static SMP_FORCE_INLINE smp_block_t* _smp_get_block_from_offset(uint32_t offset, smp_block_t* relative_to)
{
    return (smp_block_t*) ((smp_byte_t*) relative_to + offset ?: NULL);
}

static SMP_FORCE_INLINE uint32_t _smp_get_relative_offset(smp_block_t* block, smp_block_t* relative_to)
{
    smp_byte_t* a = (smp_byte_t*) block;
    smp_byte_t* b = (smp_byte_t*) relative_to;

    return a > b ? a - b : 0;
}

static SMP_FORCE_INLINE smp_byte_t* _smp_get_ptr_from_block(smp_block_t* block)
{
    return (smp_byte_t*) block + sizeof(smp_block_t);
}

static SMP_FORCE_INLINE smp_block_t* _smp_get_block_from_ptr(smp_byte_t* ptr)
{
    return (smp_block_t*) ((smp_byte_t*) ptr - sizeof(smp_block_t));
}

static SMP_FORCE_INLINE bool _smp_validate_block(smp_block_t* block)
{
    return block->magic == SMP_MAGIC;
}