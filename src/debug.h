#ifdef DEBUG_ALL
	#ifndef DEBUG_MEM
		#define DEBUG_MEM
	#endif
#endif

#ifdef DEBUG_MEM
void* DEBUG_MEMmalloc(size_t size, char* file, int line);
void DEBUG_MEMfree(void* ptr);
void DEBUG_MEMeval();

#define malloc(X)	DEBUG_MEMmalloc(X,__FILE__,__LINE__)
#define free(X)		DEBUG_MEMfree(X)
#endif