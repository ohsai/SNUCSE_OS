
#include "sched.h"
#include <linux/slab.h>
struct wrr_rq * 
wrr_e_to_task (struct sched_wrr_entity * wrr_e){
        return container_of(wrr_e, struct task_struct, wrr)
}

static inline int on_wrr_rq(struct sched_wrr_entity *wrr_e){
        return !list_empty(&wrr_e->run_list);
}

static inline 
void inc_wrr_tasks(struct sched_wrr_entity *wrr_e, struct wrr_rq *wrr_rq)
{
        rq->wrr.number_of_task++;
        rq->wrr.weight_sum += wrr_e.weight;
}
static inline 
void dec_wrr_tasks(struct sched_wrr_entity *wrr_e, struct wrr_rq *wrr_rq)
{
        rq->wrr.number_of_task--;
        rq->wrr.weight_sum -= wrr_e.weight;
}

void init_wrr_rq(struct wrr_rq *wrr_rq, struct rq * rq)
{
        wrr_rq->weight_sum = 0;
        wrr_rq->number_of_task = 0;
        raw_spin_lock_init(&wrr_rq->wrr_rq_lock);
        cur_task = NULL;       
        INIT_LIST_HEAD(&wrr_rq->run_list);

}

static void
enqueue_task_wrr(struct rq * rq, struct task_struct *p, int flags)
{
        struct sched_wrr_entity * wrr_e = &p->wrr;
        struct wrr_rq * wrr_rq = &rq->wrr;
        //flags
        int head = flags & ENQUEUE_HEAD;
        //enqueue_wrr_e
        if(on_wrr_rq(wrr_e)){ //dequeue_rt_stack
                list_del_init(&wrr_e->run_list);
                dec_wrr_tasks(wrr_e,wrr_rq);
        }
        //push to migration stack 
        if(head)
                list_add(&wrr_e->run_list, wrr_rq);
        else
                list_add_tail(&wrr_e->run_list, wrr_rq);
        inc_wrr_tasks(wrr_e, wrr_rq);       
        inc_nr_running(rq);

}
static void
dequeue_task_wrr(struct rq * rq, struct task_struct *p, int flags)
{
        struct sched_wrr_e *wrr_e = &p->wrr;
        struct wrr_rq * wrr_rq = &rq->wrr;
        //dequeue_wrr_e
        //dequeue migration stack
        if(on_wrr_rq(wrr_e)){       
                list_del_init(&wrr_e->run_list);
                dec_wrr_tasks(wrr_e,wrr_rq);
        }
        dec_nr_running(rq)
}
static void
requeue_wrr_entity(struct wrr_rq *wrr_rq, struct sched_wrr_entity * wrr_e, int head)
{
        if(on_wrr_rq(wrr_e)){
                struct list_head * queue = &wrr_rq->run_list;
                if(head)
                        list_move(&wrr_e->run_list, queue);
                else
                        list_move_tail(&wrr_e->run_list, queue);
        }
}
static void
requeue_task_wrr(struct rq * rq, struct task_struct *p, int head)
{
        struct sched_wrr_entity *wrr_e = &p->wrr; 
        requeue_wrr_entity(&rq->wrr,wrr_e,head);
}

#define WRR_TIMESLICE (HZ / 100)
#define DEFAULT_WEIGHT 10
static void
task_tick_wrr(struct rq *rq, struct task_struct *p, int queued){
        struct sched_wrr_e * wrr_e = &p->wrr;
        if(p->policy != SCHED_WRR)
                return;
        if(--wrr_e->time_slice)
                return;
        wrr_e->time_slice = wrr_e->weight * WRR_TIMESLICE;
        if(wrr_e->runlist.prev != wrr_e->run_list.next){
                requeue_task_wrr(rq, p, 0);
                set_tsk_need_resched(p);
                return;
        }
}

