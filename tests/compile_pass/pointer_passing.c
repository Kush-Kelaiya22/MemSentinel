#include <stdio.h>
#include <malloc.h>

int main()
{
	char* ptr = malloc(1024);
	char* ptr_2 = ptr;
	ptr += 3;
	free(ptr_2);

	return 0;
}