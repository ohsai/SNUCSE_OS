#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/gps.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/namei.h>
// initializing

struct gps_location GLOBAL_GPS = {
	.lat_integer = 0,
	.lat_fractional = 0,
	.lng_integer = 0,
	.lng_fractional = 0,
	.accuracy = 0,
};

DEFINE_RWLOCK(gps_lock); 
//we cannot use this in inode operation if we declare it as static
int nearby_created_area(struct inode * inode_in){
        return 0; //if nearby
}

static int valid_check(struct gps_location * loc) {
	int lat_integer = loc->lat_integer;
	int lat_fractional = loc->lat_fractional;
	int lng_integer = loc->lng_integer;
	int lng_fractional = loc->lng_fractional;
#define is_not_fractional( X )  (X < 0 || X > 999999)
	if (is_not_fractional(lat_fractional)){
		return 0;
        }
        if (is_not_fractional(lng_fractional)){
		return 0;
        }
#undef is_not_fractional
#define is_not_angle(X_INT, X_FRAC, MAX_ANGLE) ((X_INT < - MAX_ANGLE) || (X_INT > MAX_ANGLE) || (X_INT == MAX_ANGLE && X_FRAC > 0))
        if(is_not_angle(lat_integer, lat_fractional, 90)){
		return 0;
        }
        if(is_not_angle(lng_integer, lng_fractional, 180)){
		return 0;
        }
#undef is_not_angle
	if (loc->accuracy < 0)
		return 0;

	return 1;
}

SYSCALL_DEFINE1(set_gps_location, struct gps_location __user*, loc) {
	struct gps_location * k_loc;

        int err;
        //loc arg check
	if (loc == NULL)
		return -EINVAL;
        if((err = access_ok(VERIFY_READ,loc,sizeof(struct gps_location))) ==0)
                return -EFAULT;
        //copy from user
	k_loc = (struct gps_location *)kmalloc(sizeof(struct gps_location), GFP_KERNEL);
        if (k_loc == NULL)
                return -ENOMEM;
	if(copy_from_user(k_loc, loc, sizeof(struct gps_location))){ //nonzero if something is not copied
                kfree(k_loc);
		return -EFAULT;
        }
        //input validity check
	if(!valid_check(k_loc)){
                kfree(k_loc);
		return -EINVAL;
        }
        
        //set global gps
	write_lock(&gps_lock);//should we use write_lock_rqsave?
	GLOBAL_GPS.lat_integer = k_loc->lat_integer;
	GLOBAL_GPS.lat_fractional = k_loc->lat_fractional;
	GLOBAL_GPS.lng_integer = k_loc->lng_integer;
	GLOBAL_GPS.lng_fractional = k_loc->lng_fractional;
	GLOBAL_GPS.accuracy = k_loc->accuracy;
	write_unlock(&gps_lock);

	kfree(k_loc);
	return 0;
}

#define MAX_PATHLENGTH 100000L
SYSCALL_DEFINE2(get_gps_location, const char __user*, pathname, 
				struct gps_location __user*, loc) {
	struct gps_location * k_loc;

        int err;
        long path_strlen;
        char * k_pathname;
        struct inode * cur_inode;
        struct path cur_path;

        //loc arg check
	if (loc == NULL){
                printk(KERN_DEBUG"1\n");
		return -EINVAL;
        }
        if((err = access_ok(VERIFY_WRITE,loc,sizeof(struct gps_location))) ==0){
                printk(KERN_DEBUG"2\n");
                return -EFAULT;
        }
        //path arg check
        if (pathname == NULL){
                printk(KERN_DEBUG"3\n");
                return -EINVAL;
        }
        if((path_strlen = strnlen_user(pathname,MAX_PATHLENGTH)) == 0){
                printk(KERN_DEBUG"4\n");
                return -EINVAL;
        }
        if((err = access_ok(VERIFY_READ,pathname,path_strlen)) == 0){
                printk(KERN_DEBUG"5\n");
                return -EFAULT;
        }

        //copy path name
        k_pathname = (char*) kmalloc(sizeof(char) * path_strlen, GFP_KERNEL);
        if(k_pathname == NULL){
                printk(KERN_DEBUG"6\n");
                return -ENOMEM;
        }
        if(strncpy_from_user(k_pathname,pathname,MAX_PATHLENGTH) < 0){
                printk(KERN_DEBUG"7\n");
                kfree(k_pathname);
                return -EFAULT;
        }
        if(kern_path(k_pathname,LOOKUP_FOLLOW,&cur_path)){ 
                printk(KERN_DEBUG"8\n");
                // LOOKUP_FOLLOW also considers symlinks.
                //stackoverflow "Retrieving inode struct given the path to a file"
                kfree(k_pathname);
                return -EINVAL;
        }
        kfree(k_pathname);
        
        //find inode for it
        cur_inode = cur_path.dentry->d_inode;
        //setup space for copy_to_user
	k_loc = (struct gps_location *)kmalloc(sizeof(struct gps_location), GFP_KERNEL);
        if (k_loc == NULL){
                printk(KERN_DEBUG"9\n");
                return -ENOMEM;
        }
        //find gps info from it
        if(cur_inode->i_op->get_gps_location){
                if(cur_inode->i_op->get_gps_location(cur_inode,k_loc) != 0){
                printk(KERN_DEBUG"10\n");
                        //bad argument
                        kfree(k_loc);
                        return -EINVAL; 
                } 
        }                                
        else{
                printk(KERN_DEBUG"11\n");
                kfree(k_loc);
                return -ENODEV;
        }
        //copy_to_user
	if(copy_to_user(loc, k_loc, sizeof(struct gps_location))){ //nonzero if something is not copied
                printk(KERN_DEBUG"12\n");
                kfree(k_loc);
		return -EFAULT;
        }

	kfree(k_loc);
        return 0;
}







