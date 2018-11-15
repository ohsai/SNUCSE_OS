#ifndef _LINUX_ROTATION_H
#define _LINUX_ROTATION_H

/*
 * pseudo device rotation degree
 */
int ROTATION;

/*
 * data structure for blocked lock requests
 */
struct waiting_lock {
	int lock_type; // 0 : read, 1 : write
	int start;
	int end;
};

#endif /* _LINUX_ROTATION_H */
