#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/gps.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/namei.h>
//#include <linux/math64.h>
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
int nearby_created_area(struct inode * cur_inode){
	struct gps_location * k_loc;
        //get gps_location of inode
	k_loc = (struct gps_location *)kmalloc(sizeof(struct gps_location), GFP_KERNEL);
        if (k_loc == NULL){
                printk(KERN_DEBUG"1_nearby\n");
                return -ENOMEM;
        }
        if(cur_inode->i_op->get_gps_location){
                if(cur_inode->i_op->get_gps_location(cur_inode,k_loc) != 0){
                printk(KERN_DEBUG"2_nearby\n");
                        //bad argument
                        kfree(k_loc);
                        return -EINVAL; 
                } 
        }                                
        else{
                printk(KERN_DEBUG"3_nearby\n");
                kfree(k_loc);
                return -ENODEV;
        }


        return 0; //if nearby
}
#define FRAC_MAX 1000000L
#define FRAC_MIN 0
typedef struct pseudo_float {
        int int_part;
        int frac_part;
} gps_float;
gps_float gps_float_add(gps_float a, gps_float b);
gps_float gps_float_sub(gps_float a, gps_float b);
gps_float gps_float_mul(gps_float a, gps_float b);
gps_float gps_float_div(gps_float a, gps_float b);
gps_float gps_float_sin(gps_float a);
gps_float gps_float_cos(gps_float a);

gps_float gps_float_init(int int_part, int frac_part){
        gps_float out;
        out.int_part = int_part;
        out.frac_part = frac_part;
        return out;
}

gps_float gps_float_add(gps_float a, gps_float b){
       gps_float out;
       out.int_part = a.int_part + b.int_part;
       out.int_part += (a.frac_part + b.frac_part) / FRAC_MAX;
       out.frac_part = (a.frac_part + b.frac_part) % FRAC_MAX;
       //printk(KERN_DEBUG"gpsfloat add a : %d.+%d, b : %d.+%d, out : %d.+%d",a.int_part, a.frac_part, b.int_part, b.frac_part, out.int_part, out.frac_part);
       return out;
}
gps_float gps_float_sub(gps_float a, gps_float b){
       gps_float out;
       int temp;
       out.int_part = a.int_part - b.int_part;
       temp = (a.frac_part - b.frac_part);
       if(temp < FRAC_MIN){
               out.int_part -= 1;
               temp = FRAC_MAX + temp;
       }
       out.frac_part = temp;
       //printk(KERN_DEBUG"gpsfloat sub a : %d.+%d, b : %d.+%d, out : %d.+%d",a.int_part, a.frac_part, b.int_part, b.frac_part, out.int_part, out.frac_part);
       return out;
}
gps_float gps_float_mul(gps_float a, gps_float b){
        gps_float out;
        //int part
        s64 a1b1 = ((s64)a.int_part) * ((s64)b.int_part);
        //frac part
        s32 remainder;
        s64 a1b2 = (s64)a.int_part * (s64)b.frac_part;
        s64 a2b1 = (s64)a.frac_part * (s64)b.int_part;
        s64 a2b2 = (s64)a.frac_part * (s64)b.frac_part;
        s64 temp = a1b2 + a2b1 + (div64_s64(a2b2, FRAC_MAX));
        out.int_part = (s32)a1b1;
        out.int_part += (s32)div_s64_rem(temp, (s32)FRAC_MAX,&remainder);

        //printk(KERN_DEBUG"gpsfloat mul a1b1 %lld a2b1 %lld a1b2 %lld a2b2 %lld temp %lld remainder %d out.int_part %d",a1b1, a2b1, a1b2, a2b2, temp,remainder, out.int_part);
        if(remainder < FRAC_MIN){
                out.int_part -= 1;
                remainder = (s32)FRAC_MAX + remainder;
        }
        out.frac_part = remainder;
       //printk(KERN_DEBUG"gpsfloat mul ( (%d + 0.%06d) * (%d + 0.%06d) ) - (%d + 0.%06d) == 0",a.int_part, a.frac_part, b.int_part,b.frac_part, out.int_part, out.frac_part);
        return out;
}
gps_float gps_float_div(gps_float a, gps_float b){
        gps_float out;
        /*
        long long int_temp;
        long long frac_temp;
        long long tempa = ((long long) a.int_part) * FRAC_MAX + a.frac_part;
        long long tempb = ((long long) b.int_part) * FRAC_MAX + b.frac_part;
        //use do_div for long long division -> complicated
        */
        s64 int_temp;
        s64 frac_temp;
        s64 carry_temp;
        s64 remainder_temp;
        s64 tempa = ((s64)a.int_part) * FRAC_MAX + (s64)a.frac_part;
        s64 tempb = ((s64)b.int_part) * FRAC_MAX + (s64)b.frac_part;
        int_temp = div64_s64(tempa,tempb);
        remainder_temp = (tempa - (int_temp * tempb));
        frac_temp = div64_s64((remainder_temp * FRAC_MAX),tempb);
        carry_temp = div64_s64(frac_temp,FRAC_MAX);
        frac_temp = frac_temp - carry_temp * FRAC_MAX;
        //printk(KERN_DEBUG"gpsfloat div tempa %lld tempb %lld int_temp %lld remainder_temp %lld carry_temp %lld frac_temp %lld",tempa,tempb,int_temp,remainder_temp, carry_temp,frac_temp);
        if(frac_temp < FRAC_MIN){
                int_temp -= 1;
                frac_temp = FRAC_MAX + frac_temp;
        }
        out = gps_float_init((s32)int_temp, (s32)frac_temp);
        //out.int_part = (int)int_temp;
        //out.frac_part = (int)frac_temp;
       //printk(KERN_DEBUG"gpsfloat div ( (%d + 0.%06d) * (%d + 0.%06d) ) - (%d + 0.%06d) == 0",a.int_part, a.frac_part, b.int_part,b.frac_part, out.int_part, out.frac_part);
        return out;
}
gps_float gps_float_factorial(int n){
        gps_float out;
        gps_float temp;
        int i;
        if(n < 0){
                printk(KERN_DEBUG"gps_float_fact negative argument\n");
                return gps_float_init(-123,456);
        }
        else if(n == 0) {
                return gps_float_init(1,0);
        }
        out = gps_float_init(1,0);
        for(i = 1; i <= n; i++){
                temp = gps_float_init(i,0);
                out = gps_float_mul(out,temp);
        }
        //printk(KERN_DEBUG"gpsfloat factorial n : %d, out : %d.+%d",n, out.int_part, out.frac_part);
        return out;
}
gps_float gps_float_power(gps_float a, int n){
        gps_float out;
        int i;
        if(n < 0){
                printk(KERN_DEBUG"gps_float_power negative argument\n");
                return gps_float_init(-123,456);
        }
        else if(n == 0) {
                return gps_float_init(1,0);
        }
        out = gps_float_init(1,0);
        for(i = 1; i <= n; i++){
                out = gps_float_mul(out,a);
        }
        //printk(KERN_DEBUG"gpsfloat power a : %d.+%d,n : %d. out : %d.+%d",a.int_part, a.frac_part,n, out.int_part, out.frac_part);
        return out;
}
gps_float gps_float_sin(gps_float a){
        gps_float out;
        int i;
        int max_depth =11; //positive odd
        out = gps_float_init(0,0);
        for(i = 1; i <= max_depth; i+=2){
                if((i-1)%4 == 0){
                        out = gps_float_add(out,gps_float_div(
                                                gps_float_power(a,i),
                                                gps_float_factorial(i)));
                }
                else{
                        out = gps_float_sub(out,gps_float_div(
                                                gps_float_power(a,i),
                                                gps_float_factorial(i)));
                }
        }
        printk(KERN_DEBUG"gpsfloat calc sin(%d+0.%06d)-(%d+0.%06d) == 0 ",a.int_part, a.frac_part, out.int_part, out.frac_part);
        return out;
}
gps_float gps_float_cos(gps_float a){
        gps_float out;
        int i;
        int max_depth = 10;
        out = gps_float_init(0,0);
        for(i = 0; i <= max_depth; i+=2){
                if((i)%4 == 0){
                        out = gps_float_add(out,gps_float_div(
                                                gps_float_power(a,i),
                                                gps_float_factorial(i)));
                }
                else{
                        out = gps_float_sub(out,gps_float_div(
                                                gps_float_power(a,i),
                                                gps_float_factorial(i)));
                }
        }
        printk(KERN_DEBUG"gpsfloat calc cos(%d+0.%06d)-(%d+0.%06d) == 0 ",a.int_part, a.frac_part, out.int_part, out.frac_part);
        //printk(KERN_DEBUG"gpsfloat cos a : %d.+%d, out : %d.+%d",a.int_part, a.frac_part, out.int_part, out.frac_part);
        return out;
}


static int valid_check(struct gps_location * loc) {
	int lat_integer = loc->lat_integer;
	int lat_fractional = loc->lat_fractional;
	int lng_integer = loc->lng_integer;
	int lng_fractional = loc->lng_fractional;
#define is_not_fractional( X )  (X < FRAC_MIN || X >= FRAC_MAX)
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
        int test;
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

        for(test = 1; test < 5; test++){
                gps_float_sin(gps_float_init(test, (FRAC_MAX / 10)));
                gps_float_sin(gps_float_init(test, (5 * FRAC_MAX / 10)));
                gps_float_cos(gps_float_init(test, (FRAC_MAX / 10)));
                gps_float_cos(gps_float_init(test, (5 * FRAC_MAX / 10)));
        }
        for(test = 0; test > -5; test--){
                gps_float_sin(gps_float_init(test, (FRAC_MAX / 10)));
                gps_float_sin(gps_float_init(test, (5 * FRAC_MAX / 10)));
                gps_float_cos(gps_float_init(test, (FRAC_MAX / 10)));
                gps_float_cos(gps_float_init(test, (5 * FRAC_MAX / 10)));
        }

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







