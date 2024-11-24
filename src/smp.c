/*
 * smp.h - Static Memory Pool implementation
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

smp_ptr_t smp_alloc(smp_pool_t* pool, smp_size_t size)
{
    if (!pool) return NULL;
    
    smp_header_t* header = pool->head;
    while (header && (!header->available || (header->size < (size + sizeof(smp_header_t)))))
    {
        header = header->next;
    }
    
    if (!header) return NULL;
    
    smp_ptr_t ptr = &header[1];
    
    smp_size_t s = header->size;
    smp_header_t* next = header->next;
    
    header->size = (s - size) <= sizeof(smp_header_t) ? s : size;
    header->available = false;
    header->next = NULL;
    
    smp_header_t* new_header = (smp_header_t*) ((smp_byte_t*) ptr + header->size);

    // Check if the new header fits in the pool
    if (!new_header->size && ((smp_byte_t*) new_header + sizeof(smp_header_t) <= (pool->memory + pool->size)))
    {
        new_header->size = s - header->size - sizeof(smp_header_t);
        new_header->available = true;
        new_header->next = next;
    }
    else
    {
        new_header = next;
    }
    
    smp_header_t* prev_header = pool->head;
    while (prev_header->available && (prev_header->next < new_header))
    {
        prev_header = prev_header->next;
    }
    
    if (prev_header == pool->head)
    {
        prev_header = new_header;
        pool->head = new_header;
    }
    else
    {
        prev_header = new_header;
    }
    
    return ptr;
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
    
    smp_header_t* header =  (smp_header_t*) (ptr - sizeof(smp_header_t));
    header->available = true;
    memset(ptr, 0, header->size);
    
    smp_byte_t* chunk_end = (smp_byte_t*) ptr + header->size;
    smp_byte_t* pool_end = pool->memory + pool->size;
    
    if (chunk_end >= pool_end)
    {
        // We're the last chunk in the pool
        smp_header_t* tail = pool->head;
        while (tail->next)
        {
            tail = tail->next;
        }
        
        // Coalesce the chunks
        tail->size += header->size + sizeof(smp_header_t);
        memset(header, 0, sizeof(smp_header_t));
    }
    else if (chunk_end == (smp_byte_t*) pool->head)
    {
        // We're the first chunk in the pool and the next chunk is free
        // Coalesce the chunks
        header->size += pool->head->size + sizeof(smp_header_t);
        header->next = pool->head->next;
        memset(pool->head, 0, sizeof(smp_header_t));
        pool->head = header;
    }
    else if ((smp_byte_t*) header < (smp_byte_t*) pool->head)
    {
        // We're the first chunk in the pool and the next chunk isn't free
        header->next = pool->head;
        pool->head = header;
    }
    else
    {
        // Find the previous free chunk
        smp_header_t* prev_header = pool->head;
        while (((smp_byte_t*) prev_header->next + prev_header->size + sizeof(smp_header_t)) < (smp_byte_t*) header)
        {
            prev_header = prev_header->next;
        }
        
        // Coalesce the chunks
        prev_header->size += header->size + sizeof(smp_header_t);
        memset(header, 0, sizeof(smp_header_t));
    }
}

smp_size_t smp_size(smp_pool_t* pool, smp_ptr_t ptr)
{
    if (!pool || !ptr) return 0;
    if (ptr < (smp_ptr_t) pool->memory || ptr >= (smp_ptr_t) (pool->memory + pool->size)) return 0;
    
    smp_header_t* header =  (smp_header_t*) (ptr - sizeof(smp_header_t));
    
    return header->size;
}