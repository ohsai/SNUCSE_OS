#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYSCALL_ROTLOCK_READ 381
#define SYSCALL_ROTUNLOCK_READ 383

struct node {
        int value;
        struct node * next;
};
struct ilist {
        int size;
        struct node * head;
        struct node * curr;
};
void ilist_append(struct ilist * curlist, int value){
        struct node * newnode = (struct node *) malloc(sizeof(struct node));
        newnode->value = value;
        newnode->next = NULL;
        curlist->curr->next = newnode;
        curlist->curr = newnode;
        curlist->size += 1;
}
void ilist_init(struct ilist * curlist){
        struct node * newnode = (struct node *) malloc(sizeof(struct node));
        curlist->head = newnode;
        curlist->curr = curlist->head;
        curlist->size = 0;
}
#define iterate_list(nodep, curlistp)    \
        for (nodep = curlistp->head->next ; nodep != NULL; nodep = nodep->next)

int trial_division(int n){
        int f = 2;
        int original = n;
        struct ilist * curlist = (struct ilist*) malloc(sizeof(struct ilist));
        struct node * node_e;
        ilist_init(curlist);
        while(n > 1){
                if(n % f == 0){
                        n = n / f;
                        ilist_append(curlist,f);
                }
                else{
                        f += 1;
                }
        }
        int j;
        char* out = (char*) malloc(sizeof(int) * curlist->size + 400);
        j = sprintf(out,"%d = ", original);
        iterate_list(node_e, curlist){
            if(node_e->next == NULL){
                j += sprintf(out + j,"%d\n",node_e->value);
            }else{
                j += sprintf(out + j,"%d * ",node_e->value);
            }
        }
        printf("%s",out);
        return 1;
}

static volatile int run = 1;

void sigint_handler(int tmp){
	run = 0;
	syscall(SYSCALL_ROTUNLOCK_READ, 90, 90);
}

int main(int argc, char* argv[]){

	int input = atoi(argv[1]);

	FILE *f;

	signal(SIGINT, sigint_handler);
	
	int value;
	while(run){
		if(syscall(SYSCALL_ROTLOCK_READ, 90, 90) == 0){

			if(NULL != (f = fopen("integer.txt", "r"))){
		    	fscanf(f, "%d", &value);
                fclose(f);

	            printf("trial-%d: ", input);
		    	trial_division(value);
			}

			syscall(SYSCALL_ROTUNLOCK_READ, 90, 90);
           
	    }	
          sleep(1);
	}

	return 0;
}
