# include <linux/kernel.h>
# include <linux/unistd.h>
# include <linux/syscalls.h>
# include <linux/rotation.h>
# include <linux/slab.h>
# include <linux/mutex.h>
# include <linux/wait.h>


int ROTATION = 0;
LIST_HEAD(wait_reader_list);
LIST_HEAD(wait_writer_list);
LIST_HEAD(run_reader_list);
LIST_HEAD(run_writer_list);

DECLARE_WAIT_QUEUE_HEAD(read_cond);
DECLARE_WAIT_QUEUE_HEAD(write_cond);
DEFINE_MUTEX(proc_mutex);
DEFINE_MUTEX(lock);


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

void cond_wait(wait_queue_head_t * wq, struct mutex * mutex) {
	DEFINE_WAIT(__wait);
	prepare_to_wait(wq, &__wait, TASK_INTERRUPTIBLE);
	mutex_unlock(mutex);
	schedule();
	mutex_lock(mutex);

	if(signal_pending(current))
		remove_wait_queue(wq, &__wait);
	else
		finish_wait(wq, &__wait);
}

/*
void cond_signal(wait_queue_head_t * wq) {
	wake_up(wq);
}
*/

void cond_broadcast(wait_queue_head_t * wq) {
	wake_up_all(wq);
}

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

struct list_head * reader_wait_add (int _start, int _end) {
	struct reader_struct * new_reader 
		= kmalloc(sizeof(struct reader_struct), GFP_KERNEL);
	new_reader->start = _start;
	new_reader->end = _end;
	INIT_LIST_HEAD(&new_reader->next);

	list_add_tail(&new_reader->next, &wait_reader_list);
	return &new_reader->next;
}

struct list_head * writer_wait_add (int _start, int _end) {
	struct writer_struct * new_writer 
		= kmalloc(sizeof(struct writer_struct), GFP_KERNEL);
	new_writer->start = _start;
	new_writer->end = _end;
	INIT_LIST_HEAD(&new_writer->next);

	list_add_tail(&new_writer->next, &wait_writer_list);
	return &new_writer->next;
}

void reader_run_move(struct list_head * p) {
	list_move_tail(p, &run_reader_list);
}

void writer_run_move(struct list_head * p) {
	list_move_tail(p, &run_reader_list);
}

int read_overlap(int start, int end) {
	int r_s;
	int r_e;
	struct list_head * p;
	struct reader_struct *r;
	list_for_each(p, &run_reader_list) {
		r = list_entry(p, struct reader_struct, next);
		r_s = r->start;
		r_e = r->end;

		if (start  <= end) {
			if ((r_s > end && r_e < start) ||
				(r_s <= r_e && r_s > end && r_e > end) ||
				(r_s <= r_e && r_s < start && r_e < start)) {
				continue;
			}
			else
				return 0;
		}
		else {
			if (r_s <= r_e && r_s > end && r_e < start) {
				continue;
			}
			else
				return 0;
		}
	}
	return 1;
}

int write_overlap(int start, int end) {
	int w_s;
	int w_e;
	struct list_head * p;
	struct writer_struct *w;
	list_for_each(p, &run_writer_list) {
		w = list_entry(p, struct writer_struct, next);
		w_s = w->start;
		w_e = w->end;

		if (start  <= end) {
			if ((w_s > end && w_e < start) ||
				(w_s <= w_e && w_s > end && w_e > end) ||
				(w_s <= w_e && w_s < start && w_e < start)) {
				continue;
			}
			else
				return 0;
		}
		else {
			if (w_s <= w_e && w_s > end && w_e < start) {
				continue;
			}
			else
				return 0;
		}
	}
	return 1;
}

int unlock_valid(int degree, int range, int type) {
	int start; int end;
	int s_s; int s_e;
	struct list_head * p;
	struct list_head * q;
	struct reader_struct *r;
	struct writer_struct *w;

	start = start_range(degree, range);
	end = end_range(degree, range);

	if (type == 0) {
		list_for_each_safe(p, q, &run_reader_list) {
			r = list_entry(p, struct reader_struct, next);
			s_s = r->start;
			s_e = r->end;

			if(s_s == start && s_e == end) {
				list_del(&r->next);
				kfree(r);
				return 1;
			}
		}
	}
	else {
		list_for_each_safe(p, q, &run_writer_list) {
			w = list_entry(p, struct writer_struct, next);
			s_s = w->start;
			s_e = w->end;

			if(s_s == start && s_e == end) {
				list_del(&w->next);
				kfree(w);
				return 1;
			}
		}
	}

	return 0;
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

	if (degree < 0 || degree >= 360)
		return -1;

	/*
	 * Must unblock lock requests in waiting data structure
	 * and count unblocked requests
	 */

	mutex_lock(&proc_mutex);
	ROTATION = degree;

	cond_broadcast(&write_cond);
	list_for_each(p, &wait_writer_list) {
		w = list_entry(p, struct writer_struct, next);
		start = w->start;
		end = w->end;

		if (degree_validity(ROTATION, start, end)) {
			unblocked++;
			break;
		}
	}

	// writer unblocked
	if (unblocked == 0) {
		mutex_unlock(&proc_mutex);
		return unblocked;
	}

	cond_broadcast(&read_cond);
	list_for_each(p, &wait_reader_list) {
		r = list_entry(p, struct reader_struct, next);
		start = r->start;
		end = r->end;

		if (degree_validity(ROTATION, start, end)) {
			unblocked++;
		}
	}

	mutex_unlock(&proc_mutex);
	return unblocked;
}

/*
 * 381
 */

SYSCALL_DEFINE2(rotlock_read, int, degree, int, range) {
	int start;
	int end;
	struct list_head * p;

	if (!arg_ok(degree, range))
		return -1;

	start = start_range(degree, range);
	end = end_range(degree, range);

	mutex_lock(&proc_mutex);
	p = reader_wait_add(start, end);
	while(true) {
		if (degree_validity(ROTATION, start, end) &&
			read_overlap(start, end)) {
			reader_run_move(p);
			break;
		}
		else {
			cond_wait(&read_cond, &proc_mutex);
		}
	}
	mutex_unlock(&proc_mutex);
	return 0;
}

/*
 * 382
 */

SYSCALL_DEFINE2(rotlock_write, int, degree, int, range) {
	int start;
	int end;
	struct list_head * p;

	if (!arg_ok(degree, range))
		return -1;

	start = start_range(degree, range);
	end = end_range(degree, range);

	mutex_lock(&proc_mutex);
	p = writer_wait_add(start, end);
	while(true) {
		if (degree_validity(ROTATION, start, end) &&
			write_overlap(start, end)) {
			mutex_lock(&lock);
			writer_run_move(p);
			break;
		}
		else {
			cond_wait(&write_cond, &proc_mutex);
		}
	}
	mutex_unlock(&proc_mutex);
	return 0;
}

/*
 * 383
 */

SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range) {
	mutex_lock(&proc_mutex);
	if (unlock_valid(degree, range, 0) == 0) {
		mutex_unlock(&proc_mutex);
		return -1;
	}

	mutex_unlock(&proc_mutex);
	return 0;
}

/*
 * 385
 */

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range) {
	mutex_lock(&proc_mutex);
	mutex_unlock(&lock);
	if (unlock_valid(degree, range, 1) == 0) {
		mutex_unlock(&proc_mutex);
		return -1;
	}
	mutex_unlock(&proc_mutex);
	return 0;
}











