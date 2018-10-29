#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sched.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/syscall.h>

#define SYSCALL_SET_SCHEDULER 156
struct node {
        long long int value;
        struct node * next;
};
struct llist {
        struct node * head;
        struct node * curr;
};
void llist_insert(struct list * curlist){
}

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
#define WEIGHT_MAX 20
pthread_t factor_thread[WEIGHT_MAX];
long long number;

void *thread_procedure(void * arg){
	struct timespec before, after;
	long result;
	char buf[100];
	FILE *out_file;
        int weight = ((int)arg) + 1;
	if (syscall(380, syscall(__NR_gettid), weight) < 0) {
		printf("Weight setting failed.\nCheck your permission and try again.\n");
                pthread_exit((void *)-1);
	}
	printf("Start thread %ld with weight %d\n", syscall(__NR_gettid), weight);
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
                if(out_file != NULL){
	                fprintf(out_file, "weight: %d, time: %ld s\n", weight, result);
        	        fclose(out_file);
                }
        }
	syscall(381, syscall(__NR_gettid));
        pthread_exit((void *) 0);
}

int main (int argc, char* argv[]) {
        long int value1;
	struct sched_param param;
        int i;
        int status;
        int end_status;
	param.sched_priority = 0;


	// weight setting
	if (argc != 2) {
		printf("Usage: ./mt_trial  [number]\n");
		return 0;
	}
	value1 =  syscall(SYSCALL_SET_SCHEDULER, getpid(), 6,&param);
        number = atoll(argv[1]);

        for(i = 0; i< WEIGHT_MAX; i++){
                pthread_create(&factor_thread[i], NULL, &thread_procedure, (void *) i);
        }
        for(i = WEIGHT_MAX -1; i >= 0; i--){
                end_status = pthread_join(factor_thread[i], (void **)&status);
                if(end_status == 0){
                        printf("succeeded thread %d status %d\n",i, status);
                }
                else{
                        printf("error on thread %d ret code %d \n",i, end_status);
                        return -1;
                }
        }

	//int weight = syscall(381, getpid());

	return 0;
}
