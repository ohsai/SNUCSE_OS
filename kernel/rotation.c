#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/rotation.h>
#include <linux/slab.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/list.h>

#define TRUE_ROTATION 1
#define FALSE_ROTATION 0

// Concurrent Linked List Implementation
typedef struct __range_descriptor {
        int degree;
        int range;
        int type; //0 read 1 write
        int pid;
        struct list_head list;
} range_descriptor;

LIST_HEAD(range_descriptor_list);
DEFINE_MUTEX(rdlist_mutex);


// Helper function Implementation
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
int rdlist_lookup(int degree){
        range_descriptor * cur_rd;
        int reader_awake = 0;
        int writer_awake = 0;
        int start;
        int end;
        mutex_lock(&rdlist_mutex);
        list_for_each_entry(cur_rd, &range_descriptor_list,list){
                start = start_range(cur_rd->degree, cur_rd->range);
                end = end_range(cur_rd->degree, cur_rd->range);
                if(rotation_degree_valid(degree, start, end)){
                        if(cur_rd->type == 0){ 
                                // reader
                                reader_awake++;
                        }
                        else{
                                writer_awake++;
                        }
                }
                else{
                }
        }
        mutex_unlock(&rdlist_mutex);
        if(writer_awake > 0){
                return 1;
        }
        else if(reader_awake > 0){
                return reader_awake;
        }
        else{
                return 0;
        }
}
int rdlist_add(int pid, int degree, int range, int type){
        range_descriptor * cur_rd = kmalloc(sizeof(range_descriptor), GFP_KERNEL);
        if(!cur_rd) return -ENOMEM;
        cur_rd->degree = degree;
        cur_rd->range = range;
        cur_rd->type = type;
        cur_rd->pid = pid;
        mutex_lock(&rdlist_mutex);
        list_add(&cur_rd->list,&range_descriptor_list);
        mutex_unlock(&rdlist_mutex);
        return 0;
}

SYSCALL_DEFINE1(set_rotation, int, degree) {
        int task_awake;
	if (!arg_ok(degree,90))
		return ERR_FAILURE;
        task_awake =  rdlist_lookup(degree);
        mutex_lock(&proc_mutex);
        cur_rotation_degree = degree;
        cond_broadcast(&cv_onrange);
        cond_broadcast(&cv_outrange);
        mutex_unlock(&proc_mutex);
        return task_awake;
}

/*
 * 381
 */

SYSCALL_DEFINE2(rotlock_read, int, degree, int, range) {
        int start;
        int end;
        if(!arg_ok(degree, range))
                return ERR_FAILURE;
        start = start_range(degree,range);
        end = end_range(degree,range);

        // range descriptor adding
        if(rdlist_add(current->pid, degree, range, 0) != 0){
        }
        
        //Manage concurrency 
        mutex_lock(&proc_mutex);
        while(TRUE_ROTATION){
                if(rotation_degree_valid(cur_rotation_degree,start,end)){
                        if(!mutex_is_locked(&lock)){ //w condition check
                                break;
                        }
                        else{
                                cond_wait(&cv_onrange,&proc_mutex);
                        }
                }
                else{
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
        if(!arg_ok(degree, range))
                return ERR_FAILURE;
        start = start_range(degree,range);
        end = end_range(degree,range);
        
        // range descriptor adding
        if(rdlist_add(current->pid, degree, range, 1) != 0){
        }

        //Manage concurrency 
        mutex_lock(&proc_mutex);
        while(TRUE_ROTATION){
                if(rotation_degree_valid(cur_rotation_degree,start,end)){
                        if(mutex_trylock(&lock)){ //atomic lock check and acquire 
                                // w condition check and set w to true
                                break;
                        }
                        else{
                                cond_wait(&cv_onrange,&proc_mutex);
                        }
                }
                else{
                        cond_wait(&cv_outrange,&proc_mutex);
                }
        }
        while(TRUE_ROTATION){
                if(rotation_degree_valid(cur_rotation_degree,start,end)){
                        if(atomic_read(&read_count) <= 0){ //while r > 0 => break if r <= 0
                                break;
                        }
                        else{
                                cond_wait(&cv_onrange,&proc_mutex);
                        }
                }
                else{
                        cond_wait(&cv_outrange,&proc_mutex);
                }
        }
        mutex_unlock(&proc_mutex);
        return 0;
}

/*
 * 383
 */

int rdlist_del(int pid, int degree, int range, int type){
        range_descriptor * cur_rd;
        mutex_lock(&rdlist_mutex);
        list_for_each_entry(cur_rd, &range_descriptor_list,list){
                if(cur_rd->pid == pid &&
                                cur_rd->degree == degree &&
                                cur_rd->range == range &&
                                cur_rd->type == type){
                        list_del(&cur_rd->list);
                        kfree(cur_rd);
                        mutex_unlock(&rdlist_mutex);
                        return 0;
                }
        }
        mutex_unlock(&rdlist_mutex);
        return -1;
}


SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range) {
        if(rdlist_del(current->pid,degree,range,0) != 0){
                return -1;
        }
        mutex_lock(&proc_mutex);
        //decrement r
        atomic_sub(1,&read_count);
        cond_signal(&cv_onrange);
        mutex_unlock(&proc_mutex);
        return 0;
}

/*
 * 385
 */

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range) {
        if(rdlist_del(current->pid,degree,range,1) != 0){
                return -1;
        }
        mutex_lock(&proc_mutex);
        mutex_unlock(&lock);
        cond_broadcast(&cv_onrange);
        mutex_unlock(&proc_mutex);
        return 0;

}












