#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>

#define SYSCALL_SET_GPS_LOCATION 380

int notFinished = 1;

struct gps_location {
	int lat_integer;
	int lat_fractional;
	int lng_integer;
	int lng_fractional;
	int accuracy;
};

void term(int signum){
	notFinished = 0;
}

void gps_update(struct gps_location * loc, int * lat, int * lng) {
	loc = malloc(sizeof(struct gps_location));

	*lat = (*lat + 1)%180;
	*lng = (*lng + 2)%360;

	int lat_fric = rand() % 1000000;
	int lng_fric = rand() % 1000000;

	loc->lat_integer = (*lat) - 90;
	loc->lat_fractional = lat_fric;
	loc->lng_integer = (*lng) - 180;
	loc->lng_fractional = lng_fric;
	loc->accuracy = 0;

	return;
}

void daemon_gps() {
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = term;

	struct gps_location * device;

	if (sigaction(SIGTERM, &action, NULL) == -1)
		exit(-1);

	int lat = 0;
	int lng = 0;
	srand(time(NULL));
	while(notFinished) {
		gps_update(device, &lat, &lng);
		syscall(SYSCALL_SET_GPS_LOCATION, device);
		free(device);
		sleep(1);
	}
}

int main () {
	pid_t pid, sid;

	pid = fork();
	if(pid == -1)
		exit(-1);

	if(pid > 0)
		exit(0);

	sid = setsid();
	if (sid < 0)
		exit(-1);

	pid = fork();
	if(pid == -1)
		exit(-1);

	if(pid > 0)
		exit(0);

	umask(0);

	if (chdir("/") < 0)
		exit(-1);

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	daemon_gps();

	return 0;
}

