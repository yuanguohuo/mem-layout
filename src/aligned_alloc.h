#ifndef ALIGNED_ALLOC_H_
#define ALIGNED_ALLOC_H_

#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>

static
void* aligned_alloc_posix(size_t alignment, size_t size)
{
	void* ptr = nullptr;
	assert(posix_memalign(&ptr, alignment, size) == 0 && ptr != nullptr);
	return ptr;
}

#endif //ALIGNED_ALLOC_H_
