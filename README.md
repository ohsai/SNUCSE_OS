# osfall2018-team3

## How to Configure, build, install
```console
foo@bar:~$ git clone https://github.com/RockyLim92/osfall2018-team3.git

# compile/building kernel code

foo@bar:~/osfall2018-team3$ ./build

# install/flashing kernel image to device
# on other terminal  

foo@bar:~/osfall2018-team3$ screen -S team3 /dev/ttyUSB0 115200 cs8 ixoff

# press reset button for 1 sec and press power button for reboot
# then press any key at the prompt "PRESS ANY KEY"

ARTIK10 # thordown

# on original terminal

foo@bar:~/osfall2018-team3$ lthor image.tar

Linux Thor downloader ~~~
..
..
request target reboot : success  
# if this prompts, flashing is successful
```

How to execute test code
```console
# on other terminal  
foo@bar:~/osfall2018-team3$ screen -S team3 /dev/ttyUSB0 115200 cs8 ixoff

# press reset button for 1 sec and press power button for reboot
# type as below to log in to device as root
log in : root
password : tizen

# enable connection through sdb
root$ direct_set_debug.sh --sdb-set

# on original terminal
foo@bar:~/osfall2018-team3$ arm-linux-gnueabi-gcc -I$(pwd)/include ./test/test_ptree.c -o ./test/test  
foo@bar:~/osfall2018-team3$ sdb push ./test/test /root/test
foo@bar:~/osfall2018-team3$ sdb shell
$ /root/test

swapper/0...
..
..
kthread..
..

# result printed if successful
```

## How we installed system call in kernel code
We have appended or modified following codes in appropriate positions of listed files.
* arch/arm/include/uapi/asm/unistd.h : register system call
```
#define __NR_ptree                      (__NR_SYSCALL_BASE+380)
```
* arch/arm/include/asm/unistd.h : increased number of total system calls in multiple of 4 
```
#define __NR_syscalls(384)
```
* arch/arm/kernel/calls.S
```
CALL(sys_ptree)
```
* kernel/ptree.c : code of system call routine implemented
* kernel/Makefile : append 'ptree.o' at the end of obj-y definition 
```
obj-y = .... / ... ptree.o
```

## Implementation

### Main routine (methods used)

* `access_ok` : check if user space memory for given pointer is able to read and write

```c
access_ok(VERIFY_WRITE,nr,sizeof(int))
```

* `get_user` : get integer value from user space memory pointer

```c
get_user(max_index, nr)
```

* `kmalloc` : dynamically allocate kernel memory for prinfo array

  * `GFP_KERNEL` : Allocate normal kernel ram

```c
kmalloc(sizeof(struct prinfo) * max_index, GFP_KERNEL)
```

* `depth_first_search` : subroutine for DFS in process tree (tasklist)

  * Should lock before accessing tasklist
  
  * Explained more in **Depth_first_search subroutine**

```c
depth_first_search(child, buf, max_index, cur_index)
```

* `copy_to_user` : copy kernel side prinfo array data to user side prinfo array

```c
copy_to_user(buf, kernel_buf, sizeof(struct prinfo) * cur_index)
```

* `put_user` : copy integer value from kernel side to user side

```c
put_user(cur_index, nr)
```

* `kfree` : free dynamically allocated kernel memory

```c
kfree(kernel_buf)
```

* `EINVAL`, `EFAULT`, `ENOMEM` : check error for every kernel builtin function

```c
if((err = access_ok(VERIFY_WRITE,nr,sizeof(int))) == 0){
        return EFAULT;
}
if((err = get_user(max_index, nr)) != 0){
    return err;
}
if(max_index < 1){
    return EINVAL;
}
if((err = access_ok(VERIFY_WRITE,buf,sizeof(struct prinfo) * max_index)) == 0){
    return EFAULT;
}
kernel_buf = (struct prinfo *) kmalloc(sizeof(struct prinfo) * max_index, GFP_KERNEL);
if (kernel_buf == NULL){
    return ENOMEM;
}
```

### Depth_first_search subroutine

* Get data from `task_struct` according to `prinfo` spec

  * `state`, `pid`, `parent_pid`, `uid` : just copy
  
  * `comm` : use `get_task_comm()`
  
  * `first_child` : first entry of children list of current `task_struct`
  
    * use `list_first_entry_or_null`
    
  * `next_sibling` : next entry of sibling list of current task_struct
    
    * use `list_first_entry_or_null`
    
      * List defined in kernel source is doubly linked circular list, and sibling field of current task_struct points at current position, not the head of true sibling list.

      * Why `8388608`?

* For every `task_struct` entry of children, recursively apply subroutine

  * use `list_for_each` and `list_entry`
  
  * Refers to pseudocode of recursive implementation of dfs, but do not need `discovered` flag since it is a tree, not graph

```c
void depth_first_search (struct task_struct *cur_task, struct prinfo *buf,const int *max_index, int * cur_index){
    struct task_struct *child;
    struct task_struct *first_child;
    struct task_struct *next_sibling;
    struct list_head * probe;
    struct prinfo * p;
    
    if(*cur_index < *max_index){
            p = &buf[*cur_index];
            p->state = cur_task->state;
            p->pid = cur_task->pid;
            p->parent_pid = cur_task->real_parent->pid;
            first_child = list_first_entry_or_null(&(cur_task->children),struct task_struct, sibling);
            p->first_child_pid = first_child == NULL ? NONEXISTENT : first_child->pid;
            next_sibling = list_first_entry_or_null(&(cur_task->sibling),struct task_struct, sibling);
            p->next_sibling_pid = next_sibling == NULL ? NONEXISTENT : (next_sibling->pid == 8388608 ? NONEXISTENT : next_sibling->pid);
            p->uid = cur_task->cred->uid;
            get_task_comm(p->comm,cur_task);
            //printk("%s,%d,%ld,%d,%d,%d,%ld\n", p->comm, p->pid, p->state, p->parent_pid, p->first_child_pid, p->next_sibling_pid,p->uid);
            *cur_index += 1;
    }
    list_for_each(probe,&(cur_task->children)){
        child = list_entry(probe, struct task_struct, sibling);
        depth_first_search(child, buf, max_index, cur_index);
    }
}
```
