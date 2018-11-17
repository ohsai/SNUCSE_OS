#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYSCALL_ROTLOCK_READ 381
#define SYSCALL_ROTUNLOCK_READ 383

static volatile int keepRunning = 1;

void intHandler(int dummy){
	keepRunning = 0;
}

void printFactor(int num){
	int init = num;
	int factor = 2;
	int cnt = 1;
	long A[100] = {1,0,};
	
	while(num != 1){
		if(num % factor == 0)
		{
		
			A[cnt] = factor;
			cnt++;
			num /= factor;
		}
		else
		{
			factor++;
		}	
	}
	
	printf("%d = ", init);
	for(int j=1; j<cnt-1; j++)
		printf("%ld * ", A[j]);
	printf("%ld\n", A[cnt-1]);
	
	return;
}

int main(int argc, char* argv[]){
	FILE *fp;
	int id = atoi(argv[1]);
	int num;

	signal(SIGINT, intHandler);
	
	while(keepRunning){
		if(syscall(SYSCALL_ROTLOCK_READ, 90, 90) == 0){
			// fp = fopen("integer.txt", "r");
			if(NULL != (fp = fopen("integer.txt", "r"))){
			fscanf(fp, "%d", &num);
			printf("trial-%d: ", id);
			printFactor(num);
			fclose(fp);
			}
			syscall(SYSCALL_ROTUNLOCK_READ, 90, 90);
		}
		sleep(1);
	}

	return 0;
}
