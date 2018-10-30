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
        int size;
        struct node * head;
        struct node * curr;
};
void llist_append(struct llist * curlist, long long int value){
        struct node * newnode = (struct node *) malloc(sizeof(struct node));
        newnode->value = value;
        newnode->next = NULL;
        curlist->curr->next = newnode;
        curlist->curr = newnode;
        curlist->size += 1;
}
void llist_init(struct llist * curlist){
        struct node * newnode = (struct node *) malloc(sizeof(struct node));
        curlist->head = newnode;
        curlist->curr = curlist->head;
        curlist->size = 0;
}
#define iterate_list(nodep, curlistp)    \
        for (nodep = curlistp->head->next ; nodep != NULL; nodep = nodep->next)

int trial_division(long long n){
        long long f = 2;
        long long original = n;
        struct llist * curlist = (struct llist*) malloc(sizeof(struct llist));
        struct node * node_e;
        char * output;
        llist_init(curlist);
        while(n > 1){
                if(n % f == 0){
                        n = n / f;
                        //printf("%lld,",f);
                        llist_append(curlist,f);
                }
                else{
                        f += 1;
                }
        }
        int j;
        char* out = (char*) malloc(sizeof(long long int) * curlist->size + 400);
        j = sprintf(out,"threadid %ld factor of %lld : ",syscall(__NR_gettid),original);
        iterate_list(node_e, curlist){
                        j += sprintf(out + j,"%lld,",node_e->value);
        }
        j += sprintf(out+j,"\n");
        printf("%s",out);
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
                        printf("succeeded thread of weight %d status %d\n",i, status);
                }
                else{
                        printf("error on thread of weight  %d ret code %d \n",i, end_status);
                        return -1;
                }
        }

	//int weight = syscall(381, getpid());

	return 0;
}
