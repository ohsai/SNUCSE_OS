#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/gps.h>
#include <linux/slab.h>

// initializing

struct gps_location global = {
	.lat_integer = 0,
	.lat_fractional = 0,
	.lng_integer = 0,
	.lng_fractional = 0,
	.accuracy = 0,
};

static DEFINE_RWLOCK(lock);

static int valid_check(struct gps_location * loc) {
	int lat_integer = loc->lat_integer;
	int lat_fractional = loc->lat_fractional;
	int lng_integer = loc->lng_integer;
	int lng_fractional = loc->lng_fractional;

	if (lat_fractional < 0 || lat_fractional > 999999)
		return 0;
	if (lng_fractional < 0 || lng_fractional > 999999)
		return 0;

	if (lat_integer < -90 || lat_integer > 90)
		return 0;
	if (lng_integer < -180 || lng_integer > 180)
		return 0;

	if (lat_integer == 90 && lat_fractional > 0)
		return 0;
	if (lng_integer == 180 && lng_fractional > 0)
		return 0;

	if (loc->accuracy < 0)
		return 0;

	return 1;
}

SYSCALL_DEFINE1(set_gps_location, struct gps_location __user*, loc) {
	struct gps_location * k_loc;
	int latitude;
	int longitude;

	if (loc == NULL)
		return 0;

	k_loc = kmalloc(sizeof(struct gps_location), GFP_KERNEL);
	if(copy_from_user(k_loc, loc, sizeof(struct gps_location)))
		return 0;

	if(!valid_check(k_loc))
		return 0;

	write_lock(&lock);
	global.lat_integer = k_loc->lat_integer;
	global.lat_fractional = k_loc->lat_fractional;
	global.lng_integer = k_loc->lng_integer;
	global.lng_fractional = k_loc->lng_fractional;
	write_unlock(&lock);

	kfree(k_loc);
	return 1;
}

SYSCALL_DEFINE2(get_gps_location, const char __user*, pathname, 
				struct gps_locatiob __user*, location) {
}










