#include <linux/kernel.h>
#include <linux/prinfo.h>
#include <linux/unistd.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <uapi/asm-generic/errno-base.h>
#include <asm-generic/uaccess.h>
#include <linux/slab.h>

#define NONEXISTENT 0

void depth_first_search (struct task_struct *cur_task, struct prinfo *buf,const int *max_index, int * cur_index){
    struct task_struct *child;
    struct task_struct *first_child;
    struct task_struct *next_sibling;
	struct task_struct *back_to_parent; // newly added
    struct list_head * probe;
    struct prinfo * p;
    
    if(*cur_index < *max_index){
            p = &buf[*cur_index];
            p->state = cur_task->state;
            p->pid = cur_task->pid;
            p->parent_pid = cur_task->parent->pid;
            first_child = list_first_entry_or_null(&(cur_task->children),struct task_struct, sibling);
            p->first_child_pid = first_child == NULL ? NONEXISTENT : first_child->pid;
            next_sibling = list_first_entry_or_null(&(cur_task->sibling),struct task_struct, sibling);
			back_to_parent = list_first_entry_or_null(&(cur_task->sibling),struct task_struct, children); // newly added
            p->next_sibling_pid = next_sibling == NULL ? NONEXISTENT : (back_to_parent->pid == p->parent_pid ? NONEXISTENT : next_sibling->pid); 
			// 8388608
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

asmlinkage int sys_ptree(struct prinfo *buf, int *nr){
    int cur_index = 0;
    int max_index;
    int temp;
    int err;
    struct prinfo * kernel_buf;
    if(buf == NULL || nr == NULL ){
            return EINVAL;
    }
    
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

    read_lock(&tasklist_lock);
    depth_first_search(&init_task, kernel_buf,&max_index,&cur_index);
    read_unlock(&tasklist_lock);

    temp = copy_to_user(buf, kernel_buf, sizeof(struct prinfo) * cur_index);
    if((err = put_user(cur_index, nr))!= 0 ){
        return err;
    }
    kfree(kernel_buf);
    return 0;
}
