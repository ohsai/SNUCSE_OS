#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYSCALL_ROTLOCK_WRITE 382
#define SYSCALL_ROTUNLOCK_WRITE 385

static volatile int keepRunning = 1;

void intHandler(int dummy) {
	keepRunning = 0;
}

int main(int argc, char* argv[]){
	FILE *fp;
	int inputInt = atoi(argv[1]);
	int unlock;
        //printf("selector \n");
	
	signal(SIGINT, intHandler);
        printf("selector \n");
	while(keepRunning){
		if(syscall(SYSCALL_ROTLOCK_WRITE, 90, 90) == 0){
                        printf("succeeded\n");
			fp = fopen("integer.txt", "w");
			fprintf(fp, "%d", inputInt);
			printf("selector: %d\n", inputInt);
			inputInt++;
			fclose(fp);
			unlock = syscall(SYSCALL_ROTUNLOCK_WRITE, 90, 90);
			printf("write unlock: %d\n", unlock);
		}
		else printf("failed\n");
		sleep(1);
	}
        

	return 0;
}
