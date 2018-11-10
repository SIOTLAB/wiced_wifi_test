#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct myStruct
{
	char * str;
};
int main(void)
{
	struct myStruct m1;
	struct myStruct m2;

	m1.str = malloc(sizeof(char) * 10);
	m2.str = malloc(sizeof(char) * 10);
	m1.str[0] = 'a';
	strcpy(m2.str, m1.str);
	
	printf("Hello: %s\n", m2.str);
}
