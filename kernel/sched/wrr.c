#include "sched.h"
#include <linux/slab.h>

unsigned int base_time_slice = 10000000ULL; // 1*10^7 nanoseconds

static struct task_struct *pick_next_task_wrr(struct rq *rq)
{
	struct sched_wrr_entity *wrr_se;
	struct task_struct *p;
	struct wrr_rq *wrr_rq;

	wrr_rq = &rq->wrr;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
	// if still active, enqueue this task_struct.
}

static int
select_task_rq_wrr(struct task_struct *p, int sd_flag, int flags)
{
	struct task_struct *curr;
	struct rq *rq;
	int cpu;

	int lowest;
	int tmp;
	int select_cpu = task_cpu(p);

	if (p->nr_cpus_allowed == 1)
		return select_cpu;

	// Select the lowest total weight cpu.
	rcu_read_lock();

	rq = cpu_rq(select_cpu);
	lowest = &rq->wrr->weight_sum;

	for_each_possible_cpu(cpu) {
		rq = cpu_rq(cpu);
		tmp = &rq->wrr->weight_sum;

		if (tmp < lowest) {
			lowest = tmp;
			select_cpu = cpu;
		}
	}

	rcu_read_unlock();

	return select_cpu;
}

const struct sched_class wrr_sched_class = {
	.next			 = &wrr_sched_class,
	.enqueue_task    = enqueue_task_wrr,
	.dequeue_task	 = dequeue_task_wrr,

	.pick_next_task  = pick_next_task_wrr,
	.put_prev_task   = put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq  = select_task_rq_wrr,
#endif
	.get_rr_interval = get_rr_interval_wrr,
};
