#include <stdio.h>

int main()
{
	int i = 0;
	while(1){
		if(i > 9)
			i = 0;
		printf("%d\n", i++);
	}

}
