#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sched.h>
#include <dirent.h>


#define SYSCALL_SET_SCHEDULER 156
int trial_division(long long n){
        long long f = 2;
        printf("factor of %lld : ",n);
        while(n > 1){
                if(n % f == 0){
                        n = n / f;
                        printf("%lld,",f);
                }
                else{
                        f += 1;
                }
        }
        printf("\n");
        return 1;
}

int main (int argc, char* argv[]) {
	struct timespec before, after;
	long result;
        long int value1;
	char buf[100];
	FILE *out_file;
	struct sched_param param;
	param.sched_priority = 0;

	// weight setting
	if (argc != 3) {
		printf("Usage: ./test_weight.o [weight] [number]\n");
		return 0;
	}
	value1 =  syscall(SYSCALL_SET_SCHEDULER, getpid(), 6,&param);
        long long number = atoll(argv[2]);

	if (syscall(380, getpid(), atoi(argv[1])) < 0) {
		printf("Weight setting failed.\nCheck your permission and try again.\n");
		return -1;
	}

	int weight = syscall(381, getpid());

	printf("Start process %d with weight %d\n", getpid(), weight);
	sleep(1);

	// start division
        clock_gettime(CLOCK_MONOTONIC, &before);
        trial_division(number);
	clock_gettime(CLOCK_MONOTONIC, &after);
	result = after.tv_sec - before.tv_sec;

	printf("weight: %d, time: %ld s\n", weight, result);
	sprintf(buf, "/root/result/weight%d_number%lld.result", weight,number);
        DIR* dir = opendir("/root/result");
        if(dir){
	out_file = fopen(buf, "w");
	fprintf(out_file, "weight: %d, time: %ld s\n", weight, result);
	fclose(out_file);
        }
	return 0;
}
