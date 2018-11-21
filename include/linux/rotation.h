#ifndef _LINUX_ROTATION_H
#define _LINUX_ROTATION_H
//#include <linux/list.h>

/*
 * pseudo device rotation degree
 */
//extern int cur_rotation_degree;

/*
 * data structure for blocked read requests
 */
/*
struct descriptor_struct {
	int start;
	int end;
        struct task_struct * task;
	struct list_head next;
};
*/
/*
 * data structure for blocked write requests
 */
/*
struct writer_struct {
	int start;
	int end;
	struct list_head next;
}
*/

//extern int lock;



//struct list_head descriptor_list;
//struct list_head wait_writer_list;
//struct list_head runnable_list;
//struct list_head run_writer_list;

#endif /* _LINUX_ROTATION_H */
