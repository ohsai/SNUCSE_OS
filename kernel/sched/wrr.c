
#include "sched.h"
#include <linux/slab.h>

static unsigned long next_lb_time;
static spinlock_t lb_lock;
#define LB_INTERVAL 2           //in Seconds
#define WRR_TIMESLICE           (10 * HZ / 1000) 
//default base timeslice is 10 msecs. multiplying task wrr weight makes the task's timeslice
#define WRR_DEFAULT_WEIGHT 10
//also defined in /include/linux/init_task.h

struct task_struct *
wrr_e_to_task (struct sched_wrr_entity * wrr_e){
        return container_of(wrr_e, struct task_struct, wrr);
}
static inline int on_wrr_rq(struct sched_wrr_entity *wrr_e){
        return !list_empty(&wrr_e->run_list);
}
static inline void
wrr_e_time_slice_reset(struct sched_wrr_entity * wrr_e){
        wrr_e->time_slice = wrr_e->weight * WRR_TIMESLICE;
}


static int find_highest_lowest_cpus(int * max_cpu_out, int * min_cpu_out){
        struct rq * cur_rq;
        int max_weight, min_weight, max_cpu, min_cpu;
        int cpu;
        //Initialize
        int init_cpu = smp_processor_id(); //this_cpu
        if (!cpumask_test_cpu(init_cpu, cpu_active_mask)){
                init_cpu = cpumask_any(cpu_active_mask);
        }
        cur_rq = cpu_rq(init_cpu);
        max_weight = cur_rq->wrr.weight_sum;
        min_weight = max_weight;
        min_cpu = cur_rq->cpu; //To avoid null 
        max_cpu = cur_rq->cpu;
        *max_cpu_out = max_cpu;
        *min_cpu_out = min_cpu;
        
        if(unlikely(!cpu_active_mask) && num_active_cpus() < 1)
                return 0;

        //Finding highest & lowest cpu ~ find_lock_lowest_rq()
        rcu_read_lock();
        for_each_cpu(cpu, cpu_active_mask){ //cpu_active_mask better than cpu_online_mask
                cur_rq = cpu_rq(cpu);
                if(max_weight < cur_rq->wrr.weight_sum){
                        max_weight = cur_rq->wrr.weight_sum;
                        max_cpu = cur_rq->cpu; // cpu? or cur_rq->cpu?
                }
                if(min_weight > cur_rq->wrr.weight_sum){
                        min_weight = cur_rq->wrr.weight_sum;
                        min_cpu = cur_rq->cpu; // cpu? or cur_rq->cpu?
                }
        }
        if(max_weight <= min_weight || max_cpu == min_cpu){ 
                //duplicate equal-weight rq or only one rq
                rcu_read_unlock();
                return 0;
        } 
        *max_cpu_out = max_cpu;
        *min_cpu_out = min_cpu;
        rcu_read_unlock();
        return 1;
}
static int task_migratable(struct rq * rq, struct task_struct *p , int dest_cpu){ 
        //idea from enqueue_pushable_task condition
        if(!task_current(rq,p) && p->nr_cpus_allowed > 1 && cpumask_test_cpu(dest_cpu, tsk_cpus_allowed(p))){
                return 1;
                }
        return 0;
}
static int migrate_task_wrr(int src_cpu, int dest_cpu){
        struct sched_wrr_entity * wrr;
        struct sched_wrr_entity * temp;
        struct rq * rq_dest;
        struct rq * rq_src;
        struct task_struct * cur_task;
        struct task_struct *  migrating_task;
        int highest_eligible_weight, min_weight, max_weight;
        migrating_task = NULL;
        rcu_read_lock();
        rq_src = cpu_rq(src_cpu);
        rq_dest = cpu_rq(dest_cpu);
        highest_eligible_weight = 0;

        min_weight = rq_dest->wrr.weight_sum;
        max_weight = rq_src->wrr.weight_sum;
        list_for_each_entry_safe(wrr, temp,&rq_src->wrr.run_list, run_list){
                cur_task = wrr_e_to_task(wrr);
                if(task_migratable(rq_src,cur_task,dest_cpu) && // eligibility
                   wrr->weight > highest_eligible_weight && // highest
                   min_weight + wrr->weight < max_weight - wrr->weight){ // irreversible
                        highest_eligible_weight = wrr->weight;
                        migrating_task = cur_task;
                }
        }
        rcu_read_unlock();
        
        if(migrating_task !=NULL){
                raw_spin_lock(&migrating_task->pi_lock);
                double_rq_lock(rq_src, rq_dest);
                if(task_cpu(migrating_task) != src_cpu){ 
                        //already migrated
                double_rq_unlock(rq_src,rq_dest); 
                raw_spin_unlock(&migrating_task->pi_lock);
                        return 1;
                }
                if(!cpumask_test_cpu(dest_cpu, tsk_cpus_allowed(migrating_task))){ 
                        // affinity changed
                double_rq_unlock(rq_src,rq_dest); 
                raw_spin_unlock(&migrating_task->pi_lock);
                        return 0;
                }
                if(migrating_task->on_rq){
                        deactivate_task(rq_src, migrating_task, 0);
                        set_task_cpu(migrating_task, rq_dest->cpu);
                        activate_task(rq_dest, migrating_task, 0);
                        check_preempt_curr(rq_dest, migrating_task, 0);
                }
                double_rq_unlock(rq_src,rq_dest); 
                raw_spin_unlock(&migrating_task->pi_lock);
                return 1;                
        }
        else{
                return 0;
        }
        
        return 0;
}
static void __wrr_load_balance(void){
        int src_cpu, dest_cpu, max_cpu, min_cpu;
        if(!find_highest_lowest_cpus(&max_cpu, &min_cpu)){ // if(need for migration) then 1 else 0
                return;
        }
        //task migration
        src_cpu = max_cpu;
        dest_cpu = min_cpu;
        
        if(!migrate_task_wrr(src_cpu, dest_cpu)){ // if(migrated) then 1 else 0
                return;
        }
        
        return;
}
void wrr_load_balance(struct rq * rq, int cpu){
        spin_lock(&lb_lock);
        if(time_after_eq(jiffies, next_lb_time)){
                next_lb_time = jiffies + LB_INTERVAL * HZ; //next_lb_time_refresh
                preempt_disable(); // This thread does not go down until enabled
                __wrr_load_balance();
                preempt_enable();
        }
        spin_unlock(&lb_lock);        
}


void init_sched_wrr_class(void){
        next_lb_time = jiffies + LB_INTERVAL * HZ;
        spin_lock_init(&lb_lock);
        current->wrr.weight = WRR_DEFAULT_WEIGHT;
};

#ifdef CONFIG_SCHED_DEBUG
extern void print_wrr_rq(struct seq_file *m, int cpu, struct wrr_rq * wrr_rq);
void print_wrr_stats(struct seq_file *m, int cpu){
        struct wrr_rq * wrr_rq ;
        rcu_read_lock();
        wrr_rq = &cpu_rq(cpu)->wrr;
        print_wrr_rq(m,cpu,wrr_rq);
        rcu_read_unlock();
}

#endif


static inline 
void inc_wrr_tasks(struct sched_wrr_entity *wrr_e, struct wrr_rq *wrr_rq)
{
        ++wrr_rq->number_of_task;
        wrr_rq->weight_sum += wrr_e->weight;
}
static inline 
void dec_wrr_tasks(struct sched_wrr_entity *wrr_e, struct wrr_rq *wrr_rq)
{
        --wrr_rq->number_of_task;
        wrr_rq->weight_sum -= wrr_e->weight;
}

void init_wrr_rq(struct wrr_rq *wrr_rq)
{
        wrr_rq->weight_sum = 0;
        wrr_rq->number_of_task = 0;
        raw_spin_lock_init(&wrr_rq->wrr_rq_lock);
        INIT_LIST_HEAD(&wrr_rq->run_list);

}

static void
enqueue_task_wrr(struct rq * rq, struct task_struct *p, int flags)
{
        struct sched_wrr_entity * wrr_e = &p->wrr;
        struct wrr_rq * wrr_rq = &rq->wrr;
        //flags
        int head = flags & ENQUEUE_HEAD;
        raw_spin_lock(&wrr_rq->wrr_rq_lock);
                list_add_tail(&wrr_e->run_list, &wrr_rq->run_list);
        wrr_e_time_slice_reset(wrr_e);
        inc_wrr_tasks(wrr_e, wrr_rq);       
        inc_nr_running(rq);
        raw_spin_unlock(&wrr_rq->wrr_rq_lock);
      

}
static void
dequeue_task_wrr(struct rq * rq, struct task_struct *p, int flags)
{
        struct sched_wrr_entity *wrr_e = &p->wrr;
        struct wrr_rq * wrr_rq = &rq->wrr;
        raw_spin_lock(&wrr_rq->wrr_rq_lock);
                list_del_init(&wrr_e->run_list);
                dec_wrr_tasks(wrr_e,wrr_rq);
        dec_nr_running(rq);
        raw_spin_unlock(&wrr_rq->wrr_rq_lock);

}
static void
requeue_wrr_entity(struct wrr_rq *wrr_rq, struct sched_wrr_entity * wrr_e, int head)
{
        raw_spin_lock(&wrr_rq->wrr_rq_lock);
                        list_move_tail(&wrr_e->run_list, &wrr_rq->run_list);
        raw_spin_unlock(&wrr_rq->wrr_rq_lock);
}
static void
requeue_task_wrr(struct rq * rq, struct task_struct *p, int head)
{
        struct sched_wrr_entity *wrr_e = &p->wrr; 
        requeue_wrr_entity(&rq->wrr,wrr_e,head);
        wrr_e_time_slice_reset(wrr_e);
}
static void yield_task_wrr (struct rq *rq){
        requeue_task_wrr(rq, rq->curr, 0);
}
//no prioirty term

static void
task_tick_wrr(struct rq *rq, struct task_struct *p, int queued){
        struct sched_wrr_entity * wrr_e = &p->wrr;
        if(p->policy != SCHED_WRR)
                return;
        if(--wrr_e->time_slice)
                return;
        wrr_e_time_slice_reset(wrr_e); //also does in requeue  but in case of singular list
                requeue_task_wrr(rq, p, 0);
                set_tsk_need_resched(p);
                return;
}
unsigned int base_time_slice = 10000000ULL; // 1*10^7 nanoseconds
static void check_preempt_curr_wrr (struct rq *rq, struct task_struct *p, int flags){} 

static struct task_struct *pick_next_task_wrr(struct rq *rq)
{
	struct sched_wrr_entity *wrr_se;
	struct task_struct *p;
	struct wrr_rq *wrr_rq = &rq->wrr;

        if(!wrr_rq->number_of_task|| list_empty(&wrr_rq->run_list))
                return NULL;
        wrr_se = list_first_entry_or_null(&wrr_rq->run_list, struct sched_wrr_entity, run_list);
        if(!wrr_se)
                return NULL;
        p = wrr_e_to_task(wrr_se);
        if(p){
                //Needs something like dequeue_pushable_task 
                p->se.exec_start = rq->clock_task; // just as rt.c pick_next_task_rt
                wrr_e_time_slice_reset(wrr_se);
                
        }
        return p;
        
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
	// if still active, enqueue this task_struct.
}

#ifdef CONFIG_SMP
static int
select_task_rq_wrr(struct task_struct *p, int sd_flag, int flags)
{
	struct rq *rq;
	int cpu;

	int lowest;
	int tmp;
	int select_cpu = task_cpu(p);

	if (p->nr_cpus_allowed == 1)
		return select_cpu;
	/* For anything but wake ups, just return the task_cpu */
	if (sd_flag != SD_BALANCE_FORK)
		return select_cpu;

	// Select the lowest total weight cpu.
	rcu_read_lock();

	rq = cpu_rq(select_cpu);
	lowest = (&rq->wrr)->weight_sum;

	for_each_cpu(cpu,cpu_active_mask) { 
                // active cpu =< online cpu =< possible cpu (subset relation)
		rq = cpu_rq(cpu);
		tmp = (&rq->wrr)->weight_sum;

		if (tmp < lowest && cpumask_test_cpu(cpu,tsk_cpus_allowed(p))) { //affinity check
			lowest = tmp;
			select_cpu = cpu;
		}
	}

	rcu_read_unlock();

	return select_cpu;
}
static void rq_online_wrr(struct rq *rq){}
static void rq_offline_wrr(struct rq *rq){}
static void pre_schedule_wrr (struct rq *this_rq, struct task_struct *task){}
static void post_schedule_wrr (struct rq *this_rq){}
static void task_woken_wrr (struct rq *this_rq, struct task_struct *task){} //pushing supervised globally
#endif

static void set_curr_task_wrr(struct rq * rq){}
static void task_fork_wrr (struct task_struct *p){
        //define wrr_e, then do.get weight from parent. this is incomplete function
        struct sched_wrr_entity * wrr_e = &p->wrr;
        wrr_e->weight = p->real_parent->wrr.weight;       
        wrr_e_time_slice_reset(wrr_e);
}
static void switched_to_wrr(struct rq * this_rq, struct task_struct *task){
        //define wrr_e, then do. get weight from default. this is incomplete function
        struct sched_wrr_entity * wrr_e = &task->wrr;
        //wrr_e->weight = WRR_DEFAULT_WEIGHT;
        wrr_e_time_slice_reset(wrr_e);
}
static void switched_from_wrr(struct rq * this_rq, struct task_struct *task){

}
static unsigned int get_rr_interval_wrr(struct rq * rq, struct task_struct *task){
        return task->wrr.weight * WRR_TIMESLICE;
}
static void prio_changed_wrr(struct rq * rq, struct task_struct *p, int oldprio){}


const struct sched_class wrr_sched_class = {
	.next		 = &fair_sched_class,
	.enqueue_task    = enqueue_task_wrr,
	.dequeue_task	 = dequeue_task_wrr,
        .yield_task      = yield_task_wrr,
        .check_preempt_curr     = check_preempt_curr_wrr,
	.pick_next_task  = pick_next_task_wrr,
	.put_prev_task   = put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq  = select_task_rq_wrr,
        .rq_online       = rq_online_wrr,
        .rq_offline      = rq_offline_wrr,
        .pre_schedule    = pre_schedule_wrr,
        .post_schedule   = post_schedule_wrr,
        .task_woken      = task_woken_wrr,
#endif
        .set_curr_task   = set_curr_task_wrr,
        .task_tick       = task_tick_wrr,
        .task_fork       = task_fork_wrr,
	.get_rr_interval = get_rr_interval_wrr,
        .switched_from   = switched_from_wrr,
        .switched_to     = switched_to_wrr,
        .prio_changed    = prio_changed_wrr,
};

