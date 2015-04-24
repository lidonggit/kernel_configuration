/*
 * dependency.c
 *
 * It implements a task scheduler with dependencies.
 * Every task can depends only from another one, not multiple dependencies is allowed
 *
 * A task has this members.
 * status - block by dependency, waiting to run
 * depends - the task that depends on
 *
 * When a task is complete all dependencies are unblocked, one of them is execute in the same thread
 * if there is no more dependencies to unblock then a waiting one is pick from the list.
 *
 * every running task is move to the end of the list
 * when list size reach zero then the running thread finish.
 *
 * if there is no any task to do then the thread must wait in a waiting queue until waiting task !=0
 * ones all task are done ( list size ==0) then waiting task get the value -1 to wakeup all waiting threads
 * and release all thread because list is empty.
 * spin lock have to be use to access the list
 *
 * any module which its dependency is not found on the list will take the highest
 *
 * Picking a new task.
 * Wake up all waiting task.
 * If number of waked up task != 1 the decrement and waiting += waked up
 * if (waiting) wake_up queue
 * if list empty return null
 * if waiting == 0 then pick one, waiting = len, signal queue
 *
 *	Main thread will start others threads and pick task from the list.
 *	if all task are waiting for end_idx then wait for all threads
 *
 *	List holding task
 *
 *  |----------------|----------|------------|
 *  begin            waiting    running      done/end
 *
 *  if waiting == running then all task became ready
 *
 *  two tables are in use
 *  1 . task data
 *  2 . task index -
 *
 *  Created on: 16 Apr 2015
 *      Author: lester.crespo
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/debugfs.h>
#include <linux/mm.h>  // mmap related stuff#include <linux/slab.h>#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <asm/atomic.h>
#include <linux/kthread.h>  // for threads#ifdef CONFIG_ASYNCHRO_MODULE_INIT_DEBUG
#define printk_debug(...) printk(__VA_ARGS__)
#else
#define printk_debug(...) do {} while(0)
#endif

/**
 * Static struct holding all data
 */
extern struct init_fn_t __async_initcall_start[], __async_initcall_end[];
extern struct dependency_t __async_modules_depends_start[], __async_modules_depends_end[];

extern initcall_t __initcall_start[];
extern initcall_t __initcall0_start[];
extern initcall_t __initcall1_start[];
extern initcall_t __initcall2_start[];
extern initcall_t __initcall3_start[];
extern initcall_t __initcall4_start[];
extern initcall_t __initcall5_start[];
extern initcall_t __initcall6_start[];
extern initcall_t __initcall7_start[];
extern initcall_t __initcall_end[];

static initcall_t *initcall_levels[] __initdata =
{ //
        __initcall0_start, //
        __initcall1_start, //
        __initcall2_start, //
        __initcall3_start, //
        __initcall4_start, //
        __initcall5_start, //
        __initcall6_start, //
        __initcall7_start, //
        __initcall_end //
        };

#ifdef CONFIG_ASYNCHRO_MODULE_INIT_DEBUG
static const char* const module_name[] =
{   "",INIT_CALLS(macro_str)};
#endif

/**
 * Dependencies list can be declare any time in any c file
 */
ADD_MODULE_DEPENDENCY(rfcomm_init,bt_init);

ADD_MODULE_DEPENDENCY(snd_hrtimer_init,alsa_timer_init);
ADD_MODULE_DEPENDENCY(alsa_mixer_oss_init,alsa_pcm_init);
ADD_MODULE_DEPENDENCY(alsa_pcm_oss_init,alsa_mixer_oss_init);
ADD_MODULE_DEPENDENCY(snd_hda_codec,alsa_hwdep_init);
ADD_MODULE_DEPENDENCY(alsa_hwdep_init,alsa_pcm_init);
ADD_MODULE_DEPENDENCY(alsa_seq_device_init,alsa_timer_init);
ADD_MODULE_DEPENDENCY(alsa_seq_init,alsa_seq_device_init);
ADD_MODULE_DEPENDENCY(alsa_seq_midi_event_init,alsa_seq_init);
ADD_MODULE_DEPENDENCY(alsa_seq_dummy_init,alsa_seq_init);	
ADD_MODULE_DEPENDENCY(alsa_seq_oss_init,alsa_seq_midi_event_init);	
/* HDA snd is exported function plus all patches */		
ADD_MODULE_DEPENDENCY(patch_si3054_init,alsa_hwdep_init);		
ADD_MODULE_DEPENDENCY(patch_ca0132_init,alsa_hwdep_init);		
ADD_MODULE_DEPENDENCY(patch_hdmi_init,alsa_hwdep_init);		
ADD_MODULE_DEPENDENCY(patch_sigmatel_init,alsa_hwdep_init);		
ADD_MODULE_DEPENDENCY(patch_cirrus_init,alsa_hwdep_init);		
ADD_MODULE_DEPENDENCY(patch_ca0110_init,alsa_hwdep_init);		
ADD_MODULE_DEPENDENCY(patch_via_init,alsa_hwdep_init);		
ADD_MODULE_DEPENDENCY(patch_realtek_init,alsa_hwdep_init);		
ADD_MODULE_DEPENDENCY(patch_conexant_init,alsa_hwdep_init);		
ADD_MODULE_DEPENDENCY(patch_cmedia_init,alsa_hwdep_init);		
ADD_MODULE_DEPENDENCY(patch_analog_init,alsa_hwdep_init);		

ADD_MODULE_DEPENDENCY(coretemp,hwmon);
ADD_MODULE_DEPENDENCY(gpio_fan,hwmon);
ADD_MODULE_DEPENDENCY(acpi_processor_driver_init,hwmon);				

ADD_MODULE_DEPENDENCY(ubi_init,init_mtd);
ADD_MODULE_DEPENDENCY(uio_cif,uio);
ADD_MODULE_DEPENDENCY(mxm_wmi,wmi);
ADD_MODULE_DEPENDENCY(speedstep_ich,speedstep);

ADD_MODULE_DEPENDENCY(mmc_block,mmc_core);
ADD_MODULE_DEPENDENCY(videodev,usb_core);
ADD_MODULE_DEPENDENCY(v4l2_common,videodev);
ADD_MODULE_DEPENDENCY(videobuf2_core,v4l2_common);
ADD_MODULE_DEPENDENCY(videobuf2_memops,videobuf2_core);
ADD_MODULE_DEPENDENCY(videobuf2_vmalloc,videobuf2_memops);
	
ADD_MODULE_DEPENDENCY(uvcvideo,videobuf2_vmalloc);
ADD_MODULE_DEPENDENCY(gspca_main,videodev);
//USB
ADD_MODULE_DEPENDENCY(usb_core,usb_common);
ADD_MODULE_DEPENDENCY(ohci_hcd_mod_init,usb_core);
ADD_MODULE_DEPENDENCY(uhci_hcd_init,usb_core);
ADD_MODULE_DEPENDENCY(usbmon,usb_core);
ADD_MODULE_DEPENDENCY(usb_storage_driver_init,usb_core);
ADD_MODULE_DEPENDENCY(led_driver_init,usb_core);
ADD_MODULE_DEPENDENCY(hid_init,usb_core);
ADD_MODULE_DEPENDENCY(ehci_hcd_init,usb_core);

ADD_MODULE_DEPENDENCY(ohci_pci_init,ohci_hcd_mod_init);
ADD_MODULE_DEPENDENCY(ehci_platform_init,ohci_hcd_mod_init);
ADD_MODULE_DEPENDENCY(hid_init,usb_storage_driver_init);
ADD_MODULE_DEPENDENCY(uas_driver_init,usb_storage_driver_init);
ADD_MODULE_DEPENDENCY(realtek_cr_driver_init,usb_storage_driver_init);
ADD_MODULE_DEPENDENCY(ene_ub6250_driver_init,usb_storage_driver_init);		

ADD_MODULE_DEPENDENCY(ehci_pci_init,ehci_hcd_init);
ADD_MODULE_DEPENDENCY(ehci_platform_init,ehci_hcd_init);

ADD_MODULE_DEPENDENCY(smsc,libphy);
		
		
#define MAX_TASKS 200

/**
 * all initcall will be enums. a tbl will store all names
 */
struct task_t
{
    task_type_t type;           //
    modules_e id;           // idx in string table
    initcall_t fnc;             // ptr to init function
    unsigned waiting_for;
    unsigned waiting_count; // how many task does it depend on
    unsigned child_count;   // how many task it triggers
};

// TODO join together all task with taskid table to allow dynamic allocation
struct task_list_t
{
    unsigned waiting_last;  // last task waiting to be release
    unsigned ready_last;    // last task to be execute
    unsigned running_last;  // the last task running
    unsigned last;       // last element on the list
    unsigned idx_list[MAX_TASKS];
    unsigned task_end;      // number of tasks
    unsigned task_left;     // how many of global task are left to done
    struct task_t all[MAX_TASKS];
};

static struct task_list_t tasks;

void FillTasks(struct init_fn_t* begin, struct init_fn_t* end)
{
    struct dependency_t *it_dependency;
    unsigned idx;
    struct init_fn_t* it_init_fnc;
    struct task_t* task = tasks.all;
    for (it_init_fnc = begin; it_init_fnc < end; ++it_init_fnc, ++task)
    {
        task->id = it_init_fnc->id;
        task->type = it_init_fnc->type_;
        task->fnc = it_init_fnc->fnc;
        task->waiting_for = 0;
        task->waiting_count = 1;
    }
    tasks.task_end = end - begin;
    tasks.task_left = tasks.task_end;
    // resolve dependencies
    for (idx = 0; idx < tasks.task_end; ++idx)
    {
        for (it_dependency = __async_modules_depends_start; it_dependency != __async_modules_depends_end; ++it_dependency)
        {
            // at the moment only one dependency is supported
            if (it_dependency->task_id == tasks.all[tasks.idx_list[idx]].id)
            {
				// TODO check for parent process to be in the list with the same type
                ++tasks.all[tasks.idx_list[idx]].waiting_count;
                tasks.all[tasks.idx_list[idx]].waiting_for = it_dependency->parent_id;
            }
            if (it_dependency->parent_id == tasks.all[tasks.idx_list[idx]].id)
            {
                ++tasks.all[tasks.idx_list[idx]].child_count;
            }
        }
        printk_debug("async registered '%s' depends on '%s'\n", module_name[tasks.all[tasks.idx_list[idx]].id], tasks.all[ tasks.idx_list[idx]].waiting_count != 0 ? module_name[tasks.all[ tasks.idx_list[idx]].waiting_for]: "");
    }
}
/**
 * Prepare dependencies structure to process an specific type of task
 */
void Prepare(task_type_t type)
{
    // Pick only task of type from all task
    unsigned idx, idx2, id;
    tasks.last = 0;
    //
    for (idx = 0; idx < tasks.task_end; ++idx)
    {
        if (tasks.all[idx].type == type)
        {
            tasks.idx_list[tasks.last] = idx;
            ++tasks.last;
        }
    }
    tasks.ready_last = tasks.last;
    tasks.waiting_last = tasks.last;
    tasks.running_last = tasks.last;
    // jump waiting task
    idx2 = 0;
    while (tasks.all[tasks.idx_list[idx2]].waiting_count != 0)
        ++idx2;
    for (idx = idx2 + 1; idx < tasks.task_end; ++idx)
    {
        //move waiting task to front
        if (tasks.all[tasks.idx_list[idx]].waiting_count != 0)
        {
            id = tasks.idx_list[idx2];
            tasks.idx_list[idx2] = tasks.idx_list[idx];
            tasks.idx_list[idx] = id;
            ++idx2;
        }
    }
    tasks.waiting_last = idx2;
}

/**
 * wait on the queue until unlocked != 0 then pick a task
 * locked == running; unlock all remaining task and scheduler
 *
 * main thread wait for unlocked !=0 or waiting == running
 * threads end when waiting == 0
 * last thread is when running became 0
 */

static DEFINE_SPINLOCK(list_lock);
static DECLARE_WAIT_QUEUE_HEAD( list_wait);

/**
 * Mark task as done and get
 * Get a task from the list for execution
 * nullptr - no more task available
 */
unsigned TaskDone(unsigned task_idx)
{
    unsigned i, j;
    unsigned child_count = 0;    // how many task has been wakeup
    if (tasks.running_last == 0)
        return 0;
    //lock
    spin_lock(&list_lock);
    //waiting = depends.waiting;    // detect how many task has been wake up
    if (task_idx < tasks.task_end)
    {
        tasks.task_left--;
        // move to done list
        for (i = tasks.ready_last; i < tasks.running_last; ++i)
        {
            if (tasks.idx_list[i] == task_idx)
            {
                // bring down a task to do
                --tasks.running_last;
                tasks.idx_list[i] = tasks.idx_list[tasks.running_last];
                break;
            }
        }
        i = 0;
        child_count = tasks.all[task_idx].child_count;
        while (tasks.all[task_idx].child_count != 0)
        {
            // release all task that depends_2 on the finished one and remember
            for (i = 0; i < tasks.waiting_last; ++i)
            {
                if (tasks.all[tasks.idx_list[i]].waiting_for == task_idx)
                {
                    //move to ready

                    tasks.all[i].waiting_for = 0;

                }
            }
        }
    }
    // check if not running task and not pending one
    if (tasks.running_last == tasks.waiting_last)
    {
        tasks.waiting_last = 0;
    }

    // pick a new task
    task_idx = tasks.task_end;
    if (tasks.waiting_last != tasks.ready_last)
    {
        // find the lower id task available
        j = 0;
        for (i = tasks.waiting_last; i < tasks.ready_last; ++i)
        {
            if (tasks.idx_list[i] < task_idx)
            {
                task_idx = tasks.idx_list[i];
                j = i;
            }
        }
        --tasks.waiting_last;
        tasks.idx_list[j] = tasks.idx_list[tasks.waiting_last];
        tasks.idx_list[tasks.waiting_last] = task_idx;
    }
    else
    {
        // check end of all task
        if (tasks.running_last == 0)
        {
            // clean up all memory
        }
    }
    // spin unlock
    spin_unlock(&list_lock);
    // allow other thread pick a task
    if (child_count > 1)
        wake_up_interruptible(&list_wait);
    if (tasks.running_last == 0 && (tasks.task_left == 0))
    {
        //free_initmem(); do not doit until deferred
    }
    return task_idx;
}

int WorkingThread(void *data)
{
    int ret;
    unsigned task_idx = tasks.task_end;
    printk_debug("async %d starts\n", (unsigned)data);
    do
    {
        task_idx = TaskDone(task_idx);
        if (task_idx != tasks.task_end)
        {
            printk_debug("async %d %s\n", (unsigned)data, module_name[tasks.all[task_idx].id]);
            do_one_initcall(tasks.all[task_idx].fnc);
        }
        else
        {
            printk_debug("async %d waiting ...\n", (unsigned)data);
            ret = wait_event_interruptible(list_wait, (tasks.ready_last != tasks.waiting_last || tasks.waiting_last == 0));
            if (ret != 0)
            {
                printk("async init wake up returned %d\n", ret);
                break;
            }
            //wait for (depends.unlocked !=0 or depends.waiting_last == 0)
        }
    } while (tasks.waiting_last != 0);	// something to do
    printk_debug("async %d ends\n", (unsigned)data);
    return 0;
}

/**
 * Execute all initialization for an specific type
 * We need wait for everything done as a barrier to avoid problems
 */
int doit_type(task_type_t type)
{
    unsigned max_threads = CONFIG_ASYNCHRO_MODULE_INIT_THREADS;
    unsigned max_cpus = num_online_cpus();
    static struct task_struct *thr;
    Prepare(type);
    if (max_threads == 0)
        WorkingThread(0);
    for (; max_threads != 0; --max_threads)
    {
        //start working threads
        thr = kthread_create(WorkingThread, (void* )(max_threads), "async thread");
        if (thr != ERR_PTR(-ENOMEM))
        {
            kthread_bind(thr, max_threads % max_cpus);
            wake_up_process(thr);
        }
        else
        {
            printk("Async module initialization thread failed .. fall back to normal mode");
            //WorkingThread(NULL);
        }
    }
    return 0;
}

void traceInitCalls(void)
{
    initcall_t *fn;
    int level;
    for (level = 0; level < ARRAY_SIZE(initcall_levels) - 1; level++)
    {
        for (fn = initcall_levels[level]; fn < initcall_levels[level + 1]; fn++)
        {
            printk_debug("initcall %d , %pF ", level, fn);
        }
    }
}
/**
 * First initialization of module. Disk diver and AGP  
 */
static int async_initialization(void)
{
    FillTasks(__async_initcall_start, __async_initcall_end);
    //traceInitCalls();
    printk_debug("async started asynchronized\n");
    return doit_type(asynchronized);
}
/**
 * Second initialization USB devices, some PCI
 */
static int deferred_initialization(void)
{
    printk_debug("async started deferred\n");
    //return doit_type(deferred);
    return 0;
}

module_init(async_initialization);
late_initcall_sync(deferred_initialization);		// Second stage, last to do before jump to high level initialization

#ifdef TEST
#define DOIT(x) do { printf("...\n"); Prepare(x,sizeof(x)/sizeof(*x)); WorkingThread(); } while(0)

int main()
{
    // single
    struct s_task list1[] =
    {
        {   "a", 0},
        {   "b", "a"},
        {   "c", "b"},
        {   "d", "b"}};
    // keep order for single thread but can be all together
    struct s_task list2[] =
    {
        {   "a", 0},
        {   "b",0},
        {   "c", 0},
        {   "d", 0}};
    // order is keep because dependences are resolved on time
    struct s_task list3[] =
    {
        {   "a",0},
        {   "b","a"},
        {   "c","b"},
        {   "d",0},
        {   "e", 0}};

    struct s_task list4[] =
    {
        {   "a",0},
        {   "d","c"},
        {   "e",0},
        {   "b","a"},
        {   "c","b"}

    };

    //
    struct s_task list5[] =
    {
        {   "a", 0}};

    DOIT(list1);
    DOIT(list2);
    DOIT(list3);
    DOIT(list4);
//	Prepare(list1,sizeof(list1)/sizeof(*list1));
//	WorkingThread();
//	Prepare(list2,sizeof(list2)/sizeof(*list2));
//	WorkingThread();
//	Prepare(list3,sizeof(list3)/sizeof(*list));
//	WorkingThread();

}
#endif

