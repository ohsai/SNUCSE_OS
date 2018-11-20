# osfall2018-team3 Project 3. Rotation Read-Write Lock 

## Policy

## Implementation
* Data Structures & Sync Primitives for Policy

  * `range_descriptor` : It stores read/write requests info, and is put on the list.
    ```c
      typedef struct __range_descriptor {
        int degree;
        int range;
        int type; // 0 read 1 write
        int pid;
        struct list_head list;
      } range_descriptor;
    ```
    
    ```c
      LIST_HEAD(range_descriptor_list);
      DEFINE_MUTEX(rdlist_mutex);
    ```
    
    ```c
      /*
       * rdlist_add allocates range_descriptor for current process' request
       * and puts it on the range_descriptor_list. Return 0 when it successes.
       */
       
      int rdlist_add(int pid, int degreem int range, int type);
      
      /*
       * rdlist_del deletes range descriptor which has same pid, degree,
       * range, and request type with requesting process. Return 0 when it successes.
       */
       
       int rdlist_del(int pid, int degree, int range, int type);
      
      /*
       * rdlist_lookup traverses range_descriptor_list and counts requests
       * which can hold lock in given degree. If write can awake, it returns
       * only 1. It returns the number of awakable read requests unless there
       * is possible write request.
       */
       
       int rdlist_lookup(int degree);
    ```
    
  * Sync Primitives
    ```
      DECLARE_WAIT_QUEUE_HEAD(cv_onrange); // wait_queue for requests on degree range
      DECLARE_WAIT_QUEUE_HEAD(cv_outrange); // wait_queue for requests out of degree range
      DEFINE_MUTEX(lock); // Block read requests while write request is being processed.
      DEFINE_MUTEX(proc_mutex); // Assure the mutual exclusion among rotation lock/unlock.
      atomic_t read_count = ATOMIC_INIT(0);
    ```
  * Condition Variable Functions : Implement condition variable using `wait_queue`.
  ```c
    void cond_wait(wait_queue_head_t * wq, struct mutex * mutex) {
      DEFINE_WAIT(__wait);
      prepare_to_Wait(wq, &__wait, TASK_INTERRUPTIBLE);
      mutex_unlock(mutex);
      schedule();
      mutex_lock(mutex);
      if(signal_pending(current))
        remove_wait_queue(wq, &__wait);
      else
        finish_wait(wq, &__wait);
    }
    
    void cond_broadcast(wait_queue_head_t * wq) {
      wake_up_all(wq);
    }
    
    void cond_signal(wait_queue_head_t * wq) {
      wake_up(wq);
    }
  ```
      
* `set_rotation` : Set rotation value for daemon device. System Call 380.

```c
  SYSCALL_DEFINE1(set_rotation, int, degree) {
    int task_awake;
    if (arg_ok(degree, 90)) return ERR_FAILURE;
    
    task_awake = rdlist_lookup(degree);
    mutex_lock(&proc_mutex);
    cur_rotation_degree = degree; // Global Variable tracking current degree
    cond_broadcast(&cv_onrange);
    cond_broadcast(&cv_outrange);
    mutex_unlock(&proc_mutex);
    
    return 0;
  }
```

* `rotlock_read` : Hold lock for read request. System Call 381.

```c
  SYSCALL_DEFINE2(rotlock_read, int, degree, int, range) {
  
    ...
   
    if(rdlist_add(current->pid, degree, range, 0) != 0) // add range descriptor
    
    ...
   
    mutex_lock(&proc_mutex);
    while(TRUE_ROTATION) {
      if(rotation_degree_valid(cur_rotation_degree, start, end)) {
        if(!mutex_is_locked(&lock)) { // No write request is being processed
          break;
        }
        else {
          cond_wait(&cv_onrange, &proc_mutex);
        }
      }
      else {
        cond_wait(&cv_outrange, &proc_mutex);
      }
    }
    
    atomic_add(1, &read_count);
    mutex_unlock(&proc_mutex);
    return 0;
  }
```

* `rotlock_Write` : Hold lock for write request. System Call 382.

```c
  SYSCALL_DEFINE2(rotlock_write, int, degree, int, range) {
  
    ...
   
    if(rdlist_add(current->pid, degree, range, 1) != 0) // add range descriptor
    
    ...
   
    mutex_lock(&proc_mutex);
    while(TRUE_ROTATION) {
      if(rotation_degree_valid(cur_rotation_degree, start, end)) {
        if(mutex_trylock(&lock)) { // Write request should be processed one at a time
          break;
        }
        else {
          cond_wait(&cv_onrange, &proc_mutex);
        }
      }
      else {
        cond_wait(&cv_outrange, &proc_mutex);
      }
    }
    
    while(TRUE_ROTATION) {
      if(rotation_degree_valid(cur_rotation_degree, start, end)) {
        if(atomic_read(&read_count) <= 0) { // Wait if there are any processing read requests
          break;
        }
        else {
          cond_wait(&cv_onrange, &proc_mutex);
        }
      }
      else {
        cond_wait(&cv_outrange, &proc_mutex);
      }
    }
    
    mutex_unlock(&proc_mutex);
    return 0;
  }
```

* `rotunlock_read` : Release lock on read request. System Call 383.

```c
  SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range) {
    if(rdlist_del(current->pid,degree,range,0) != 0) // delete this range descriptor from list
    
    ...
    
    mutex_lock(&proc_mutex);
    atomic_sub(1,&read_count);
    cond_signal(&cv_onrange);
    mutex_unlock(&proc_mutex);
    return 0;
  }
```

* `rotunlock_write` : Release lock on write request. System Call 385. System Call 384 is owned by the other one.

```c
  SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range) {
    if(rdlist_del(current->pid,degree,range,1) != 0) // delete this range descriptor from list
    
    ...
    
    mutex_lock(&proc_mutex);
    mutex_unlock(&lock);
    cond_broadcast(&cv_onrange);
    mutex_unlock(&proc_mutex);
    return 0;
  }
```

## Test

rotd daemon, selector and trial  
```console
foo@bar:~/osfall2018-team3$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/rotd.c -o ./test/rotd  
foo@bar:~/osfall2018-team3$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/selector.c -o ./test/selector
foo@bar:~/osfall2018-team3$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/trial.c -o ./test/trial

sh4.2$ ./rotd
sh4.2$ ./selector [number] & ./trial 1 & ./trial 2 & ./trial 3 & 
sh4.2$ dmesg | tail -n 20
sh4.2$ killall trial && killall selector

selector 28374 & ./trial 1 & ./trial 2 & ./trial 3 &
dmesg | grep "lookup\|set_rotation"

```  

selector / trial example works well currently.  

