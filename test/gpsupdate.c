#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include "gps.h"
#define SYSCALL_SET_GPS_LOCATION 380

/*struct gps_location {
	int lat_integer;
	int lat_fractional;
	int lng_integer;
	int lng_fractional;
	int accuracy;
};

*/
void gps_input(struct gps_location * loc, int lat_i, int lat_f,
				int lng_i, int lng_f, int acc) {
	loc->lat_integer = lat_i;
	loc->lat_fractional = lat_f;
	loc->lng_integer = lng_i;
	loc->lng_fractional = lng_f;
	loc->accuracy = acc;

	return;
}

int main(int argc, char* argv[]) {
	int lat_integer;
	int lat_fractional;
	int lng_integer;
	int lng_fractional;
	int accuracy;
        int err = 0;
	struct gps_location device;
        if(argc != 6){
                printf("Invalid number of Arguments\n");
                return -EINVAL;
        }
        lat_integer = atoi(argv[1]);
        lat_fractional = atoi(argv[2]);
        lng_integer = atoi(argv[3]);
        lng_fractional = atoi(argv[4]);
        accuracy = atoi(argv[5]);
        gps_input(&device, lat_integer, lat_fractional, 
			   lng_integer, lng_fractional, accuracy);
        syscall(SYSCALL_SET_GPS_LOCATION, &device); //Golang-style programming
        err = -errno;
        if(err == -EINVAL){
                printf("Invalid Argument \n");
        }
        else if(err == -EFAULT){
                printf("Segmenation Fault \n");
        }
        else if(err == -ENOMEM){
                printf("Not Enough Kernel Memory \n");
        }
        else if(err == 0){
                printf("New GPS location set \n");
        }
	return err;
}




