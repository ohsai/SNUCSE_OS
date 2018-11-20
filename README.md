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
      DEFINE_MUTEX(proc_mutex); // Guarantee 
      atomic_t read_count = ATOMIC_INIT(0);
    ```
  * Condition Variable Functions
  ```c
    void cond_wait(wait_queue_head_t * wq, struct mutex * mutex) {
      DEFINE_WAIT(__wait);
      prepare_to_Wait(wq, &__wait, TASK_INTERRUPTIBLE);
      mutex_unlock(mutex);
      schedule();
      mutex_lock(mutex);
      if(signal_pending(current)) remove_wait_queue(wq, &__wait);
      else                        finish_wait(wq, &__wait);
    }
    
    void cond_broadcast(wait_queue_head_t * wq) {
      wake_up_all(wq);
    }
    
    void cond_signal(wait_queue_head_t * wq) {
      wake_up(wq);
    }
  ```
      
* `set_rotation` : Set rotation value for daemon device. System Call 380.
* `rotlock_read` : Hold lock for read request. System Call 381.
* `rotlock_Write` : Hold lock for write request. System Call 382.
* `rotunlock_read` : Release lock on read request. System Call 383.
* `rotunlock_write` : Release lock on write request. System Call 385. System Call 384 is owned by the other one.

## Test
terminating while holding lock does not release lock, this is current problem.  
Current version [ 11/17 5:00 pm ] is not a rwlock, but few change will make it as rwlock.  

rotd daemon, selector and trial  
```console
foo@bar:~/osfall2018-team3$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/rotd.c -o ./test/rotd  
foo@bar:~/osfall2018-team3$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/selector.c -o ./test/selector
foo@bar:~/osfall2018-team3$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/trial.c -o ./test/trial

sh4.2$ ./rotd
sh4.2$ ./selector [number] & ./trial 1 & ./trial 2 & ./trial 3 & 
sh4.2$ dmesg | tail -n 20
sh4.2$ killall trial && killall selector

WHY IS THIS CRAZY??? -- TRY TO DO

selector 28374 & ./trial 1 & ./trial 2 & ./trial 3 &
dmesg | grep "lookup\|set_rotation"

```  

selector / trial example works well currently.  

