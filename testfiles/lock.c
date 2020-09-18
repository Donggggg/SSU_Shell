#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#define DATA_SIZE 2048

int lock_memory(char *addr, size_t size)
{
	unsigned long page_offset, page_size;
	page_size = sysconf(_SC_PAGE_SIZE);
	page_offset = (unsigned long) addr % page_size;

	addr -= page_offset;
	size += page_offset;
	return (mlock(addr, size));
}

int main(){
	char data[DATA_SIZE];

	if(lock_memory(data, DATA_SIZE) == -1)
		perror("lock_memory");

	while(1);

}
