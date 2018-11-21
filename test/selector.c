#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYSCALL_ROTLOCK_WRITE 382
#define SYSCALL_ROTUNLOCK_WRITE 385

static volatile int run = 1;

void sigint_handler(int tmp) {
	run = 0;
	syscall(SYSCALL_ROTUNLOCK_WRITE, 90, 90);
}

int main(int argc, char* argv[]){

	int input = atoi(argv[1]);
    FILE *f;
	
	signal(SIGINT, sigint_handler);

	while(run){
		if(syscall(SYSCALL_ROTLOCK_WRITE, 90, 90) == 0){

			f = fopen("integer.txt", "w");
			fprintf(f, "%d", input);
			fclose(f);
            
			printf("selector: %d\n", input);
			input++;
			syscall(SYSCALL_ROTUNLOCK_WRITE, 90, 90);

		}else{
          printf("error!\n");
        }
        sleep(1);
	}

	return 0;
}
