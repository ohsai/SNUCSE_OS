#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
//#include <linux/sched.h>
#include <sched.h>
#include <time.h>

#define SYSCALL_GET_SCHEDULER 157
#define SYSCALL_SET_SCHEDULER 156
#define SYSCALL_SCHED_SETWEIGHT 380
#define SYSCALL_SCHED_GETWEIGHT 381

void foo(int weight);

int main(int argc, char* argv[]) {
	if(fork()==0) {
		if(fork()==0)
			if(fork()==0) foo(1);
			else foo(4);
		else
			if(fork()==0) foo(7);
			else foo(10);
	}else {
		if(fork()==0)
			if(fork()==0) foo(13);
			else foo(15);
		else
			if(fork()==0) foo(17);
			else foo(20);
	}
	return 0;
}

void foo(int weight){
	struct sched_param param;
	param.sched_priority = 0;
	printf("process %d with weight %d\n", getpid(), weight);
		long int value1 =  syscall(SYSCALL_SET_SCHEDULER, getpid(), 6,&param);
		long int value2 = syscall(SYSCALL_SCHED_SETWEIGHT, getpid(), weight);
		int i;
		clock_t start = clock();
		for(i=1; i<1000000000 ; i++ ) {
		}
		clock_t end = clock();
		float seconds = (float)(end - start) /CLOCKS_PER_SEC;
		printf("process with weight %d ended time: %f\n", weight, seconds);
		syscall(SYSCALL_SCHED_GETWEIGHT, getpid());
}
