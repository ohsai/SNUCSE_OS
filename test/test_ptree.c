#include <stdio.h>
//#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/prinfo.h>

#define __PROJ1_SYSCALL__ 380
#define __PRINFO_ARRAY_SIZE__ 300

int main(){
    struct prinfo * p;
    int i,j,k;
    int nr = __PRINFO_ARRAY_SIZE__;
    struct prinfo * buf = (struct prinfo *) malloc(sizeof(struct prinfo) * nr);
    int * tier;
    int syscall_status = syscall(__PROJ1_SYSCALL__,buf,&nr);
    //printf("System call sys_ptree returned status %d and  %d prinfo entries\n",syscall_status, nr);
    tier = (int *) malloc(sizeof(int) * nr);
    for (i = 0; i<nr; i++){
        p = &(buf[i]);
        tier[i] = 0;
        if(p->parent_pid > 0){ // if parent exists
                for(j = 0; j<i; j++){
                        if(buf[j].pid == p->parent_pid){
                                tier[i] = tier[j] + 1; //Tier up one from its parents
                                break;
                        }
                }
        }
        for(k = 0; k < tier[i]; k++){
                printf("  ");
        }
        printf("%s,%d,%ld,%d,%d,%d,%ld\n", p->comm, p->pid, p->state, p->parent_pid, p->first_child_pid, p->next_sibling_pid, p->uid);
    }
    free(buf);
    free(tier);


    return 0;
}

