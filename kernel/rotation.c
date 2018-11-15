# include <linux/syscalls.h>
# include <linux/rotation.h>
# include <uapi/asm-generic/errno-base.h>

int start_range (int degree, int range) {
	return (degree >= range) ? degree-range : 360 + degree - range;
}

int end_range (int degree, int range) {
	return (degree + range) % 360;
}

int arg_ok (int degree, int range) {
	if (degree < 0 || degree >= 360 || range  < 0 || range  >= 180)
		return -1;
	else
		return 0;
}

/*
 * 380
 */

SYSCALL_DEFINE1(set_rotation, int, degree) {
	int unblocked = 0;

	if (degree < 0 || degree >= 360)
		return -1;

	/*
	 * Must unblock lock requests in waiting data structure
	 * and count unblocked requests
	 */

	ROTATION = degree;

	return unblocked;
}

/*
 * 381
 */

SYSCALL_DEFINE2(rotlock_read, int, degree, int, range) {
}

/*
 * 382
 */

SYSCALL_DEFINE2(rotlock_write, int, degree, int, range) {
}

/*
 * 383
 */

SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range) {
}

/*
 * 384
 */

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range) {
}

/*
 * 385? I'm not sure whether we need this.
 */

SYSCALL_DEFINE(lock_init) {

}











