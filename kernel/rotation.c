#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/rotation.h>
//#include <linux/slab.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/types.h>

#define TRUE_ROTATION 1
#define FALSE_ROTATION 0

int start_range (int degree, int range) {
	return (degree >= range) ? degree-range : 360 + degree - range;
}

int end_range (int degree, int range) {
	return (degree + range) % 360;
}

int arg_ok (int degree, int range) {
	if (degree < 0 || degree >= 360 || range  < 0 || range  >= 180)
		return FALSE_ROTATION;
	else
		return TRUE_ROTATION;
}

/*
 * int rotation_degree_valid(int degree, int start, int end);
 * check this struct's start~end range contains device degree.
 */

int rotation_degree_valid(int degree, int start, int end) {
	if (end >= start) {
		if (degree >= start && degree <= end)
			return TRUE_ROTATION;
		else
			return FALSE_ROTATION;
	}
	else {
		if (degree >= start || degree <= end)
			return TRUE_ROTATION;
		else
			return FALSE_ROTATION;
	}
}
/*
 * 380
 */

int cur_rotation_degree = 0;
DECLARE_WAIT_QUEUE_HEAD(cv_onrange);
DECLARE_WAIT_QUEUE_HEAD(cv_outrange);
DEFINE_MUTEX(lock);
DEFINE_MUTEX(proc_mutex);
#define ERR_FAILURE -1
atomic_t read_count = ATOMIC_INIT(0);

void cond_wait(wait_queue_head_t * wq, struct mutex * mutex){
        DEFINE_WAIT(__wait);
        //prepare_to_wait(wq,&__wait,TASK_UNINTERRUPTIBLE); 
        prepare_to_wait(wq,&__wait,TASK_INTERRUPTIBLE); 
        // TASK_INTERRUPTIBLE then signal_pending needed
        mutex_unlock(mutex);
        schedule();
        mutex_lock(mutex);
        if(signal_pending(current)){
                remove_wait_queue(wq,&__wait);
        }
        else{
                finish_wait(wq, &__wait);
        }
}
void cond_broadcast(wait_queue_head_t * wq){
        wake_up_all(wq);
}
void cond_signal(wait_queue_head_t * wq){
        wake_up(wq);
}

SYSCALL_DEFINE1(set_rotation, int, degree) {
	if (!arg_ok(degree,90))
		return ERR_FAILURE;
        mutex_lock(&proc_mutex);
        cur_rotation_degree = degree;
        printk(KERN_DEBUG"set_rotation called with degree %d current lock %d read_count %d \n",cur_rotation_degree,mutex_is_locked(&lock),atomic_read(&read_count));
        cond_broadcast(&cv_onrange);
        cond_broadcast(&cv_outrange);
        mutex_unlock(&proc_mutex);
        return 0;
}

/*
 * 381
 */

SYSCALL_DEFINE2(rotlock_read, int, degree, int, range) {
        int start;
        int end;
        printk(KERN_DEBUG"Rotlock_read called\n");
        if(!arg_ok(degree, range))
                return ERR_FAILURE;
        start = start_range(degree,range);
        end = end_range(degree,range);
        mutex_lock(&proc_mutex);
        while(TRUE_ROTATION){
                if(rotation_degree_valid(cur_rotation_degree,start,end)){
                        printk(KERN_DEBUG"readlock|valid|cur : %d, start : %d, end : %d, lock: %d read_count %d\n",cur_rotation_degree, start,end,mutex_is_locked(&lock),atomic_read(&read_count));
                        if(!mutex_is_locked(&lock)){ //w condition check
                                break;
                        }
                        else{
                                cond_wait(&cv_onrange,&proc_mutex);
                        }
                }
                else{
                printk(KERN_DEBUG"readlock|invalid|cur : %d, start : %d, end : %d, lock : %d, read_count %d\n",cur_rotation_degree, start,end,mutex_is_locked(&lock),atomic_read(&read_count));
                        cond_wait(&cv_outrange,&proc_mutex);
                }
        }
        //increment r
        atomic_add(1,&read_count);
        mutex_unlock(&proc_mutex);
        return 0;
}

/*
 * 382
 */

SYSCALL_DEFINE2(rotlock_write, int, degree, int, range) {
        int start;
        int end;
        printk(KERN_DEBUG"Rotlock_write called\n");
        if(!arg_ok(degree, range))
                return ERR_FAILURE;
        start = start_range(degree,range);
        end = end_range(degree,range);
        mutex_lock(&proc_mutex);
        while(TRUE_ROTATION){
                if(rotation_degree_valid(cur_rotation_degree,start,end)){
                        printk(KERN_DEBUG"writelock|valid|cur : %d, start : %d, end : %d, lock: %d, read_count %d\n",cur_rotation_degree, start,end,mutex_is_locked(&lock),atomic_read(&read_count));
                        if(mutex_trylock(&lock)){ //atomic lock check and acquire 
                                // w condition check and set w to true
                                break;
                        }
                        else{
                                cond_wait(&cv_onrange,&proc_mutex);
                        }
                }
                else{
                printk(KERN_DEBUG"writelock|invalid|cur : %d, start : %d, end : %d, lock : %d, read_count %d\n",cur_rotation_degree, start,end,mutex_is_locked(&lock),atomic_read(&read_count));
                        cond_wait(&cv_outrange,&proc_mutex);
                }
        }
        while(TRUE_ROTATION){
                if(rotation_degree_valid(cur_rotation_degree,start,end)){
                        printk(KERN_DEBUG"writelock|valid|cur : %d, start : %d, end : %d, lock: %d, read_count %d\n",cur_rotation_degree, start,end,mutex_is_locked(&lock), atomic_read(&read_count));
                        if(atomic_read(&read_count) <= 0){ //while r > 0 => break if r <= 0
                                break;
                        }
                        else{
                                cond_wait(&cv_onrange,&proc_mutex);
                        }
                }
                else{
                printk(KERN_DEBUG"writelock|invalid|cur : %d, start : %d, end : %d, lock : %d, read_count %d\n",cur_rotation_degree, start,end,mutex_is_locked(&lock), atomic_read(&read_count));
                        cond_wait(&cv_outrange,&proc_mutex);
                }
        }
        //lock = 1;
        mutex_unlock(&proc_mutex);
        return 0;
}

/*
 * 383
 */

SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range) {
        printk(KERN_DEBUG"read_unlock called, read_count %d\n", atomic_read(&read_count));
        mutex_lock(&proc_mutex);
        //decrement r
        atomic_sub(1,&read_count);
        //mutex_unlock(&lock);
        //lock = 0;
        //cond_broadcast(&cv_onrange);
        cond_signal(&cv_onrange);
        mutex_unlock(&proc_mutex);
        return 0;
}

/*
 * 384
 */

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range) {
        printk(KERN_DEBUG"Rotunlock_write called, read_count %d\n",atomic_read(&read_count));
        mutex_lock(&proc_mutex);
        mutex_unlock(&lock);
        //lock = 0;
        cond_broadcast(&cv_onrange);
        mutex_unlock(&proc_mutex);
        return 0;

}












