#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/gps.h>

SYSCALL_DEFINE1(set_gps_location, struct gps_location __user*, loc) {
}

SYSCALL_DEFINE2(get_gps_locayion, const char __user*, pathname, 
				struct gps_locatiob __user*, location) {
}
