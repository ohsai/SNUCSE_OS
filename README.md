# osfall2018-team3 Project 3. Rotation Read-Write Lock 

### Rotation Lock Policies
 1. Read/Write requests can hold locks when rotation device degree is located between requests' range.
 2. **No starvation for write requests.** Write requests have higher priority to hold lock than read requests.
 3. Multiple readers can access the critical section unless there is a write request in the critical section.
 4. If there is one write request in the critical section, then no other requests are allowed to enter the critical section.
 5. When users try to unlock requests, they should input **exact same degree and range value** used in previous lock.
 6. Rotation lock/unlock should work well even when the rotation device is turned off.

### Implementation
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
      DEFINE_MUTEX(rdlist_mutex); // lock for mutually exclusive access in range descriptor list
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
       * is any possible write request.
       */
       
       int rdlist_lookup(int degree);
    ```
    
  * Sync Primitives
    ```c
      DECLARE_WAIT_QUEUE_HEAD(cv_onrange); // wait_queue for requests on degree range
      DECLARE_WAIT_QUEUE_HEAD(cv_outrange); // wait_queue for requests out of degree range
      DEFINE_MUTEX(lock); // Block read requests while write request is being processed.
      DEFINE_MUTEX(proc_mutex); // Assure the mutual exclusion among rotation lock/unlock.
      atomic_t read_count = ATOMIC_INIT(0);
    ```
  * Condition Variable Functions : Newly implemented condition variable using `wait_queue`.
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
      
* `set_rotation` : Set the rotation value from daemon device. System Call 380.

```c
  SYSCALL_DEFINE1(set_rotation, int, degree) {
    int task_awake;
    if (arg_ok(degree, 90)) return ERR_FAILURE;
    
    task_awake = rdlist_lookup(degree);
    mutex_lock(&proc_mutex);
    cur_rotation_degree = degree; // Global Variable tracking current degree
    printk(KERN_DEBUG"set_rotation|degree %d lock %d read_count %d task_awake %d\n", cur_rotation_degree, mutex_is_locked(&lock), atomic_read(&read_count), task_awake);
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
        if(!mutex_is_locked(&lock)) { // Wait until no write request is being processed
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
    
    atomic_add(1, &read_count); // read_count++
    mutex_unlock(&proc_mutex);
    return 0;
  }
```

* `rotlock_write` : Hold lock for write request. System Call 382.

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
    atomic_sub(1,&read_count); // read_count--
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
    mutex_unlock(&lock); // Release lock for write request
    cond_broadcast(&cv_onrange);
    mutex_unlock(&proc_mutex);
    return 0;
  }
```

### Test

* `rotd` daemon : This daemon updates degree value in every 2 seconds and calls the `set_rotation` system call.

* `selector` : While running, it writes incrementing number from given argument on `integer.txt` in every 1 second.
               It uses `rotlock_write` and `rotunlock_write` system calls.
```c
  int main(int argc, char* argv[]){

    ...

    while(run){
      if(syscall(SYSCALL_ROTLOCK_WRITE, 90, 90) == 0){
        f = fopen("integer.txt", "w");
        fprintf(f, "%d", input);
        fclose(f);

        printf("selector: %d\n", input);
        input++;
        syscall(SYSCALL_ROTUNLOCK_WRITE, 90, 90);
      }
      else
        printf("error!\n");
      sleep(1);
    }

    return 0;
  }              
```
               
* `trial` : While running, it reads the integer value from `integer.txt` and does *trial divison* on that number in every 1 second.
            It uses `rotlock_read`, `rotunlock_read`, and receives an integer argument as id.
            
```c
  int main(int argc, char* argv[]){

    ...

    int value;
    while(run){
      if(syscall(SYSCALL_ROTLOCK_READ, 90, 90) == 0){
        if(NULL != (f = fopen("integer.txt", "r"))){
          fscanf(f, "%d", &value);
          fclose(f);

          printf("trial-%d: ", input);
          trial_division(value);
        }
        
        syscall(SYSCALL_ROTUNLOCK_READ, 90, 90);
      }	
      sleep(1);
    }
    
    return 0;
  }
```

* How to do the test.

  * Compile test files
  
  ```console
  foo@bar:~/osfall2018-team3$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/rotd.c -o ./test/rotd  
  foo@bar:~/osfall2018-team3$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/selector.c -o ./test/selector
  foo@bar:~/osfall2018-team3$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/trial.c -o ./test/trial
  ```
  
  * Test on Tizen board
  ```console
  sh4.2$ ./rotd
  sh4.2$ ./selector [number] & (...) & ./trial 1 & (...) &
  sh4.2$ dmesg | tail -n 20
  sh4.2$ dmesg | grep "lookup\|set_rotation"
  sh4.2$ killall trial && killall selector

  ```  

