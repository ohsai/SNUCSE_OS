# osfall2018-team3 - Project 2.

## Implementation

### Weighted Round-Robin

* `sched_class` : WRR scheduler module. Defined at `/kernel/sched/wrr.c`.

	```c
	const struct sched_class wrr_sched_class = {
	    .next        = &fair_sched_class,
	    .enqueue_task    = enqueue_task_wrr,
	    .dequeue_task    = dequeue_task_wrr,
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

	```

	* `enqueue_task_wrr` : enqueue this task to the tail of runqueue.
		```c
		static void
		enqueue_task_wrr(struct rq * rq, struct task_struct *p, int flags);
		```

	* `dequeue_task_wrr` : dequeue this task from runqueue and put it sleep.
		```c
		static void
		dequeue_task_wrr(struct rq * rq, struct task_struct *p, int flags);
		```

	* `pick_next_task_wrr` : pick next task which will hold the cpu from the head of runqueue.
		```c
		static struct task_struct *pick_next_task_wrr(struct rq *rq);
		```

	* `select_task_rq_wrr` : select cpu with the lowest weight sum of tasks in its runqueue.
		```c
		static int
		select_task_rq_wrr(struct task_struct *p, int sd_flag, int flags);
		```

	* `task_tick` : whenever task's time slice is all spent, reduce its time slice or requeue it after time slice reset.
		```c
		static void
		task_tick_wrr(struct rq *rq, struct task_struct *p, int queued);
		```

* `sched_wrr_entity` : Scheduler entity, representing tasks and be actually used for scheduling. Defined at `/include/linux/sched.h`.

	```c
	struct sched_wrr_entity {
		struct list_head run_list;
		unsigned int time_slice;
		unsigned int weight;
	};

	```

* `wrr_rq` : WRR runqueue assigned for each cpu. Directly linked with sched_entity list. Defined at `/kernel/sched/sched.h`.

	```c
	struct wrr_rq{
		unsigned long weight_sum;
		unsigned long number_of_task;
		struct list_head run_list;
		raw_spinlock_t wrr_rq_lock;
	};

	```

* Other minor changes
	* Register WRR scheduling policy as 6. Defined at `/include/uapi/linux/sched.h`.
		```c
		#define SCHED_WRR		6
		```

	* WRR is next of RT scheduler. Defined at `/kernel/sched/rt.c`.
		```c
		const struct sched_class rt_sched_class = {
			.next		= &wrr_sched_class,

			.
			.
			.

		};
		```
	* Change default scheduler of `swapper` & `kthread` to WRR. Defined at `/include/linux/init_task.h` & `/kernel/kthread.c`.

		```c
		#define INIT_TASK(tsk) \
		{
			.
			.
			.

			.policy		= SCHED_WRR,		\

			.
			.
			.
		}
		```

		```c
		sched_setscheduler_nocheck(create.result, SCHED_WRR, &param);
		```

### Load Balancing

* Load balancing for WRR migrates a single task in load-excessive runqueue to load-less runqueue in every 2 seconds. Defined at `/kernel/sched/wrr.c`

* `find_highest_lowest_cpus` : find cpus with the lowest & the highest weight sum. Return 0(failure) when there are is no possible cpu, all weight sums are same, or there is only one possible cpu.

	```c
	static int find_highest_lowest_cpus(int * max_cpu_out, int * min_cpu_out);

	```
	    
* `migrate_task_wrr` : From the runqueues selected at `find_highest_lowest_cpus`, the heavier queue is a source and the other is a destination. Select the task which fits most to the balanced weight from source, and migrate it to the destination queue.

	```c
	static int migrate_task_wrr(int src_cpu, int dest_cpu);
	```

* The above functions are wrapped in `wrr_load_balance`.

### System Calls

* System Calls are defined at `/kenel/sched/core.c`.

* `Set Weight System Call`: set selected process' weight between 1 to 20.
		```
			SYSCALL_DEFINE2(sched_setweight, pid_t, pid, int, weight);
		```
* `Get Weight System Call`: get selected process' weight.
		```
			SYSCALL_DEFINE1(sched_getweight, pid_t, pid);
		```

### Trial Division Test 
* `mt_trial.c` : In main routine, 1 to 20 threads each representing weights are created. Each sub-routine of threads does trial division test to yield its factors of a given number in WRR scheduler.

	```c
	if (syscall(380, syscall(__NR_gettid), weight) < 0) {
		printf("Weight setting failed.\nCheck your permission and try a    gain.\n");
		pthread_exit((void *)-1);
	}

		.
		.

	trial_division(number); // number = atoll(argv[1]);
	```

* We tested the efficiency of each weighted threads by number `4611686014132420609`, which is the multiple of `2147483647`.

* How to compile & execute the test code.

	```
	~/linux-3.10-artik$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/mt_trial.c -o ./test/mt_trial -lpthread

	~/linux-3.10-artik$ sdb push ./test/mt_trial /root/

	~/root$ ./mt_trial 4611686014132420609
	```

* The result : The spent time by `trial_division` in each threads was **inverse proportional** with their related weights. More specific results are plotted in [plot.pdf](plot.pdf) as **Original WRR**.

### Improvement
* The original WRR scheduler doesn't specify how to pick task from a runqueue. If it picks a task in FIFO order, it satisfies the WRR spec. However, this method is definitely bad for turnaround metric due to possible convoy effects.
* The original implementation picks task in FIFO, so if we change **to pick the task with the smallest weight in runqueue first**, we can improve average turnaround metric.
* Improved WRR is implemented in `/test/wrr_improved.c`. If you want to build woth improved wrr, rename this file to `wrr.c` and replace `/kernel/sched/wrr.c`.
* In improved WRR, `lowest_weight_in_rq` finds the lowest weight task in runqueue.

	```c
	static struct sched_wrr_entity *
	lowest_weight_in_rq(struct wrr_rq *wrr_rq)
	{   
	    int lowest;
	    int curr_weight;
	    struct sched_wrr_entity *wrr_se;
	    struct sched_wrr_entity *p;
	    struct list_head * probe;

		lowest = 21; // This works as INFINITE initialization for looping.
		list_for_each(probe, &wrr_rq->run_list) {
			wrr_se = list_entry(probe, struct sched_wrr_entity, run_list);
			curr_weight = wrr_se->weight;

			if(curr_weight < lowest) {
				lowest = curr_weight;
				p = wrr_se;
			}
		}

		return p;
	}

	```

	```c
	static struct task_struct *pick_next_task_wrr(struct rq *rq)
	{
		struct shced_wrr_entity *wrr_se;
		struct task_struct *p;
		struct wrr_rq *wrr_rq = &rq->wrr;

		.
		.

		wrr_se = lowest_weight_in_rq(wrr_rq);

		.
		.
	}

	```
* When test the trial division with same multiple of the prime number, it improved average turnaround time, but the intention of **weight** changed. Because lighter weight tasks will be more interactive than heavier weight tasks, the lighter tasks tend to be completed faster. You can see the result in [plot.pdf](plot.pdf) as **Improved WRR**.



