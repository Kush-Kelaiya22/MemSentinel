#include <stdio.h>
#include <malloc.h>

int main()
{
	char* a;
	{
		char* b = malloc(256);
		a = b;
		b += 5;
		//some operations using b that may or may not modify b
		// a is not modified after setting the variable
	}
	free(a);

	return 0;
}