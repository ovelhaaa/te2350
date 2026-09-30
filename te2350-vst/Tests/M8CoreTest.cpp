#include "M8Behaviour.h"
#ifdef M8_ALLOCATION_AUDIT
bool m8InCallback=false;
unsigned long m8AllocationCount=0;
extern "C" {
void* __real_malloc(size_t);
void* __real_calloc(size_t,size_t);
void* __real_realloc(void*,size_t);
void __real_free(void*);
void* __wrap_malloc(size_t n) {if(m8InCallback) ++m8AllocationCount;return __real_malloc(n);}
void* __wrap_calloc(size_t n,size_t s) {if(m8InCallback) ++m8AllocationCount;return __real_calloc(n,s);}
void* __wrap_realloc(void* p,size_t n) {if(m8InCallback) ++m8AllocationCount;return __real_realloc(p,n);}
void __wrap_free(void* p) {if(m8InCallback) ++m8AllocationCount;__real_free(p);}
}
#endif
int main() { return verifyM8Behaviour() ? 0 : 1; }
