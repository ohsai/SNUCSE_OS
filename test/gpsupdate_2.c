#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYSCALL_SET_GPS_LOCATION 380

struct gps_location {
	int lat_integer;
	int lat_fractional;
	int lng_integer;
	int lng_fractional;
	int accuracy;
};

void gps_update(struct gps_location * loc, int lat_i, int lat_f,
				int lng_i, int lng_f, int acc) {
	loc->lat_integer = lat_i;
	loc->lat_fractional = lat_f;
	loc->lng_integer = lng_i;
	loc->lng_fractional = lng_f;
	loc->acuuracy = acc;

	return;
}

int main() {
	int lat_integer;
	int lat_fractional;
	int lng_integer;
	int lng_fractional;
	int accuracy;
	struct gps_location * device;
	device = malloc(sizeof(struct gps_location));

	printf("GPS Location Update\n");

	printf("GPS latitude integer : ");
	scanf("%d", &lat_integer);

	printf("GPS latitude fractional : ");
	scanf("%d", &lat_fractional);

	printf("GPS longitude integer : ");
	scanf("%d", &lng_integer);

	printf("GPS longitude fractional : ");
	scanf("%d", &lng_fractional);

	printf("GPS accuracy : ");
	scanf("%d", &accuracy);

	gps_update(device, lat_integer, lat_fractional, 
			   lng_integer, lng_fractional, accuracy);

	syscall(SYSCALL_SET_GPS_LOCATION, device);
	free(device);
	return 0;
}




