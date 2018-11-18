#ifndef _LINUX_ROTATION_H
#define _LINUX_ROTATION_H

#include <linux/list.h>

/*
 * data structure for blocked read requests
 */
struct reader_struct {
	int start;
	int end;
	struct task_struct * task;
	struct list_head next;
};

/*
 * data structure for blocked write requests
 */
struct writer_struct {
	int start;
	int end;
	struct task_struct * task;
	struct list_head next;
};

struct list_head wait_reader_list;
struct list_head wait_writer_list;
struct list_head run_reader_list;
struct list_head run_writer_list;

#endif
