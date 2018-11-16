# include <linux/syscalls.h>
# include <linux/rotation.h>
# include <linux/slab.h>
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
 * int degree_validity(int degree, int start, int end);
 * check this struct's start~end range contains device degree.
 */

int degree_validity(int degree, int start, int end) {
	if (end >= start) {
		if (degree >= start && degree <= end)
			return 1;
		else
			return 0;
	}
	else {
		if (degree >= start || degree <= end)
			return 1;
		else
			return 0;
	}
}

void reader_to_wait_list (int _start, int _end) {
	struct reader_struct * new_reader 
		= kmalloc(sizeof(*reader_struct, GFP_KERNEL));
	new_reader->start = _start;
	new_reader->end = _end;
	INIT_LIST_HEAD(&new_reader->next);

	list_add_tail(&new_reader->next, &wait_reader_list);
	return;
}

void writer_to_wait_list (int start, int end) {
	struct writer_struct * new_writer 
		= kmalloc(sizeof(*writer_struct, GFP_KERNEL));
	new_writer->start = _start;
	new_writer->end = _end;
	INIT_LIST_HEAD(&new_writer->next);

	list_add_tail(&new_writer->next, &wait_writer_list);
	return;
}

/*
 * 380
 */

SYSCALL_DEFINE1(set_rotation, int, degree) {
	int unblocked = 0;
	int start; int end;
	struct writer_struct *w;
	struct reader_struct *r;
	struct list_head *p;
	struct list_head *q;

	if (degree < 0 || degree >= 360)
		return -1;

	/*
	 * Must unblock lock requests in waiting data structure
	 * and count unblocked requests
	 */

	list_for_each_safe(p, wait_writer_list) {
		w = list_entry(p, struct writer_struct, next);
		start = w->start;
		end = w->end;

		if (degree_validity(degree, start, end)) {
			list_move_tail(p, run_writer_list);
			unblocked++;
			break;
		}
	}

	// writer unblocked
	if (unblocked)
		return unblocked;

	list_for_each_safe(q, wait_reader_list) {
		r = list_entry(q, struct reader_struct, next);
		start = r->start;
		end = r->end;

		if (degree_validity(degree, start, end)) {
			list_move_tail(q, run_reader_list);
			unblocked++;
		}
	}

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
	LIST_HEAD(wait_reader_list);
	LIST_HEAD(wait_writer_list);
	LIST_HEAD(run_reader_list);
	LIST_HEAD(run_writer_list);
}











