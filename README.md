# Static Memory Pool (SMP) Library

## Overview

The **Static Memory Pool (SMP)** library provides a memory allocation system for embedded systems or applications with constrained environments.

## Features
- **Pre-Allocated Pools:** Pools are fixed in size and allocated at compilation, making its usage safe for applications where memory is constrained.
- **Chunk Coalescing:** Attempts to coalesce neighboring deallocated chunks for efficient memory reusage.
- **Common Operations:** Supports allocation, contiguous allocation and deallocation.

## Getting Started

### Prerequisites
To use the SMP library, ensure you have a C compiler installed and the necessary tools to compile and link C programs.

### Installation

1. Clone the SMP library source files into your project directory.
2. Include the **smp.h** header file in your project:
```c
#include "smp.h"
```

### Compilation
To compile your program with the SMP library, ensure you link both the **smp.c** and **smp.h** files with your program:

```bash
gcc -o your_program your_program.c smp.c
```

### Basic Usage Example

```c
#include "smp.h"

// Define a pool and API
SMP_POOL_WITH_API(my_pool, 512);

int main()
{
  // Allocate memory
  void* data = my_pool_alloc(128);
  if (data == NULL)
  {
    // Allocation failed
    return -1;
  }

  // Use memory
  for (int i = 0; i < 128; i++)
  {
    ((char*) data)[i] = i;
  }

  // Deallocate memory
  my_pool_dealloc(data);
  
  return 0;
}
```

### API Documentation

#### Macros
- `SMP_POOL(pool_name, pool_size)`  
  Creates a static memory pool.

- `SMP_API(pool_name)`  
  Generates pool-specific allocation and deallocation functions.

- `SMP_POOL_WITH_API(pool_name, pool_size)`
  Combines `SMP_POOL` and `SMP_API`.

#### Functions
- `smp_ptr_t smp_alloc(smp_pool_t* pool, smp_size_t size)`  
  Allocates memory from the pool.

- `smp_ptr_t smp_calloc(smp_pool_t* pool, smp_size_t nitems, smp_size_t size)`  
  Allocates contiguous memory from the pool.

- `void smp_dealloc(smp_pool_t* pool, smp_ptr_t ptr)`  
  Deallocates memory from the pool.

- `smp_size_t smp_size(smp_pool_t* pool, smp_ptr_t ptr)`  
  Gets the size of the allocated memory.

## License

The SMP library is released under the **MIT License**. You are free to use, modify, and distribute it under the terms of the license. See the [MIT License](https://opensource.org/licenses/MIT) for more details.

## Author

This library was developed by **Laurent Mailloux-Bourassa**.
