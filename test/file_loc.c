#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "gps.h"
#define SYSCALL_GET_GPS_LOCATION 381

#define MAX_STRING_LENGTH 400
int main(int argc, char* argv[]) {
        int len;
        float latitude;
        float longitude;
	int accuracy;
        int err = 0;
        char * pathname;
	struct gps_location device;
        if(argc != 2){
                return -EINVAL;
        }
        len = strlen(argv[1]);
        if(len > MAX_STRING_LENGTH){
                printf("Path Too Long \n");
                return -EINVAL;
        }
        pathname = (char*) malloc(sizeof(char) * (len+1));
        if(pathname == NULL){
                printf("Not Enough User Memory \n");
                return -ENOMEM;
        }
        strncpy(pathname,argv[1],len+1);
        //printf("Input path : %s \n",pathname);
        
        syscall(SYSCALL_GET_GPS_LOCATION,pathname, &device); //Golang-style programming
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
        else if(err == -ENODEV){
                printf("GPS Ability Not Developed \n");
        }
        else if(err == -EACCES){
                printf("File Permission Denied \n");
        }
        else if(err == 0){
                latitude = (float)device.lat_integer + ((float)device.lat_fractional / 1000000.0);
                longitude = (float)device.lng_integer + ((float)device.lng_fractional / 1000000.0);
                //print gps location
                printf("latitude : %0.6f , longitude : %0.6f, accuracy : %dm\n",latitude, longitude, device.accuracy);
                printf("https://www.google.com/maps/search/?api=1&query=%0.6f,%0.6f\n",latitude,longitude);
        }
	return err;
}




