#include <linux/list.h>

#ifndef _LINUX_ROTATION_H
#define _LINUX_ROTATION_H

/*
 * pseudo device rotation degree
 */
int ROTATION;

/*
 * data structure for blocked read requests
 */
struct reader_struct {
	int start;
	int end;
	struct list_head next;
};

/*
 * data structure for blocked write requests
 */
struct writer_struct {
	int start;
	int end;
	struct list_head next;
}

struct list_head wait_reader_list;
struct list_head wait_writer_list;
struct list_head run_reader_list;
struct list_head run_writer_list;

#endif /* _LINUX_ROTATION_H */
