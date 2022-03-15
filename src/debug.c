
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"

char DEBUG_MEMfiles[1024][1024];
int DEBUG_MEMlines[1024];
void* DEBUG_MEMptrs[1024];
int DEBUG_MEMused = 0;

#ifdef malloc
#undef malloc
#endif

#ifdef free
#undef free
#endif

void* DEBUG_MEMmalloc(size_t size, char* file, int line)
{
	strcpy(DEBUG_MEMfiles[DEBUG_MEMused],file);
	DEBUG_MEMlines[DEBUG_MEMused] = line;
	DEBUG_MEMptrs[DEBUG_MEMused] = malloc(size);
	DEBUG_MEMused++;

	return DEBUG_MEMptrs[DEBUG_MEMused - 1];
}
void DEBUG_MEMfree(void* ptr)
{
	int i;
	for (i = 0; i < DEBUG_MEMused; i++)
		if (ptr == DEBUG_MEMptrs[i])
			break;

	free(ptr);

	int j;
	for (j = i; j < DEBUG_MEMused - 1; j++)
	{
		DEBUG_MEMptrs[j] = DEBUG_MEMptrs[j + 1];
		DEBUG_MEMlines[j] = DEBUG_MEMlines[j + 1];
		strcpy(DEBUG_MEMfiles[j],DEBUG_MEMfiles[j + 1]);
	}
	DEBUG_MEMused--;
}
void DEBUG_MEMeval()
{
	printf("###DEBUG_MEM/###\n");
	if (DEBUG_MEMused == 0)
		printf("Alles super, keine ungefreeten Pointer!\n");
	else
	{
		printf("Folgende Pointer wurden nicht gefreet!:\n");
		int i;
		for (i = 0; i < DEBUG_MEMused; i++)
			printf("%s: %d\n",DEBUG_MEMfiles[i],DEBUG_MEMlines[i]);
	}
	printf("###/DEBUG_MEM###\n");
}
