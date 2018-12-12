#ifndef _LINUX_GPS_H
#define _LINUX_GPS_H

struct gps_location {
	int lat_integer;
	int lat_fractional;
	int lng_integer;
	int lng_fractional;
	int accuracy;
};
int nearby_created_area(struct inode * cur_inode);
extern struct gps_location GLOBAL_GPS;// global gps from /kernel/gps.c
extern rwlock_t gps_lock;
#endif
