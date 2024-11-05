#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h> 
#include <linux/timekeeping.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <asm/processor.h>
#include <linux/utsname.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <asm/div64.h>
#include <asm/cpu.h>
#include <asm/page.h>
#include <linux/device.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dirac Su");
MODULE_DESCRIPTION("Better than neofetch"); 

#define KFETCH_NUM_INFO 6

#define KFETCH_RELEASE   (1 << 0)
#define KFETCH_NUM_CPUS  (1 << 1)
#define KFETCH_CPU_MODEL (1 << 2)
#define KFETCH_MEM       (1 << 3)
#define KFETCH_UPTIME    (1 << 4)
#define KFETCH_NUM_PROCS (1 << 5)

#define KFETCH_FULL_INFO ((1 << KFETCH_NUM_INFO) - 1)

#define DEVICE_NAME "kfetch"
#define MAX_BUF_LEN 1024
#define LOGO_COLS 7

#define func_call_check 0
#define get_info_check 0
#define output_check 0

enum { 
    CDEV_NOT_USED = 0, 
    CDEV_EXCLUSIVE_OPEN = 1, 
}; 

static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED); 

static int kfetch_major;

static struct cdev kfetch_cdev; 

static struct class *_class;

static char logo[LOGO_COLS][20]=
{
    "        .-.        ",
    "       (.. |       ",
    "       <>  |       ",
    "      / --- \\      ",
    "     ( |   | |     ",
    "   |\\\\_)___/\\)/\\   ",
    "  <__)------(__/   "
};

void cpu_detect(struct cpuinfo_x86 *c){
    /* Get vendor name */
    cpuid(0x00000000, (unsigned int *)&c->cpuid_level,
          (unsigned int *)&c->x86_vendor_id[0],
          (unsigned int *)&c->x86_vendor_id[8],
          (unsigned int *)&c->x86_vendor_id[4]);

    c->x86 = 4;
    /* Intel-defined flags: level 0x00000001 */
    if (c->cpuid_level >= 0x00000001) {
        u32 junk, tfms, cap0, misc;

        cpuid(0x00000001, &tfms, &misc, &junk, &cap0);
        c->x86      = x86_family(tfms);
        c->x86_model    = x86_model(tfms);
        c->x86_stepping = x86_stepping(tfms);

        if (cap0 & (1<<19)) {
            c->x86_clflush_size = ((misc >> 8) & 0xff) * 8;
            c->x86_cache_alignment = c->x86_clflush_size;
        }
    }
}

static void get_model_name(struct cpuinfo_x86 *c){
    unsigned int *v;
    char *p, *q, *s;

    v = (unsigned int *)c->x86_model_id;
    cpuid(0x80000002, &v[0], &v[1], &v[2], &v[3]);
    cpuid(0x80000003, &v[4], &v[5], &v[6], &v[7]);
    cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]);
    c->x86_model_id[48] = 0;

    /* Trim whitespace */
    p = q = s = &c->x86_model_id[0];

    while (*p == ' ')
        p++;

    while (*p) {
        /* Note the last non-whitespace index */
        if (!isspace(*p))
            s = q;

        *q++ = *p++;
    }

    *(s + 1) = '\0';
}

static ssize_t kfetch_read( struct file *fp,char *buf, size_t length, loff_t *fpos) {
    
    int mask = *((int *)fp->private_data);
    char output_buffer[MAX_BUF_LEN] = {0};
    int wptr = 0, j;

#if func_call_check
    pr_info("%s call.\n", __func__);
#endif

    sprintf(output_buffer, "                   %s\n%s", utsname()->nodename, logo[wptr]);
    
    wptr++;
    for(j = strlen(utsname()->nodename);j > 0;j--)
        output_buffer[strlen(output_buffer)] = '-';
    output_buffer[strlen(output_buffer)] = '\n';
    
    if(mask & KFETCH_RELEASE){
        sprintf(output_buffer + strlen(output_buffer), "%sKernel:   %s\n", logo[wptr], utsname()->release);
        wptr++;
    }

#if get_info_check
    pr_info("%s pass.\n", "KFETCH_RELEASE");
#endif

    if(mask & KFETCH_NUM_CPUS){
        int i, total_cpu=0, online_cpu=0;
        for_each_present_cpu(i){
            total_cpu++;
        }
        for_each_online_cpu(i){
            online_cpu++;
        }
        sprintf(output_buffer + strlen(output_buffer), "%sCPUs:     %d / %d\n", logo[wptr], online_cpu, total_cpu);
        wptr++;
    }

#if get_info_check
    pr_info("%s pass.\n", "KFETCH_NUM_CPUS");
#endif

    if(mask & KFETCH_CPU_MODEL){
        struct cpuinfo_x86 c;
        cpu_detect(&c);
        get_model_name(&c);
        sprintf(output_buffer + strlen(output_buffer), "%sCPU:      %s\n", logo[wptr], c.x86_model_id);
        wptr++;
    }

#if get_info_check
    pr_info("%s pass.\n", "KFETCH_CPU_MODEL");
#endif

    if(mask & KFETCH_MEM){
        struct sysinfo i;
        si_meminfo(&i);
        sprintf(output_buffer + strlen(output_buffer), "%sMem:      %lu MB / %lu MB\n", 
            logo[wptr], (i.freeram * i.mem_unit) >> 20 , (i.totalram * i.mem_unit) >> 20);
        wptr++;
    }

#if get_info_check    
    pr_info("%s pass.\n", "KFETCH_MEM");
#endif

    if(mask & KFETCH_UPTIME){
        unsigned int time = ktime_get_seconds();
        do_div(time, 60);
        sprintf(output_buffer + strlen(output_buffer), "%sUptime:   %u mins\n", logo[wptr], time);
        wptr++;
    }

#if get_info_check
    pr_info("%s pass.\n", "KFETCH_UPTIME");
#endif

    if(mask & KFETCH_NUM_PROCS){
        struct task_struct *task;
        int nr_process = 0;
        for_each_process( task ){
            nr_process++;
        }
        sprintf(output_buffer + strlen(output_buffer), "%sProcs:    %d\n", logo[wptr], nr_process);
        wptr++;
    }

#if get_info_check
    pr_info("%s pass.\n", "KFETCH_NUM_PROCS");
#endif

    while(wptr < LOGO_COLS){
        sprintf(output_buffer + strlen(output_buffer), "%s\n", logo[wptr]);
        wptr++;
    }

#if output_check
    pr_info("\n%s\n", output_buffer);
#endif

    if (copy_to_user(buf, output_buffer, strlen(output_buffer))) {
        pr_alert("Failed to copy data to user");
        return 0;
    }

#if output_check
    pr_info("%s pass.\n", "copy_to_user");
#endif

    return strlen(output_buffer);
}

static ssize_t kfetch_write(struct file *fp ,const char *buf , size_t length, loff_t *fpos){
    
    int mask_info;

#if func_call_check
    pr_info("%s call.\n", __func__);
#endif

    if (copy_from_user(&mask_info, buf, length)) {
        pr_alert("Failed to copy data from user");
        return 0;
    }

    *((int *)fp->private_data) = mask_info;
    
    return length;
}

static int kfetch_open(struct inode *inode , struct file *fp){
    int *mask;

#if func_call_check
    pr_info("%s call.\n", __func__);
#endif
    
    if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN)) 
        return -EBUSY; 

    mask = kmalloc(sizeof(int), GFP_KERNEL); 

    if (mask == NULL) 
        return -ENOMEM; 

    fp->private_data = mask;

    return 0;
}

static int kfetch_release(struct inode *inode , struct file *fp){

#if func_call_check
    pr_info("%s call.\n", __func__);
#endif

    if (fp->private_data){ 
        kfree(fp->private_data); 
        fp->private_data = NULL; 
    } 

    atomic_set(&already_open, CDEV_NOT_USED);

    // module_put(THIS_MODULE); 

    return 0;
}

const static struct file_operations kfetch_ops = {
    .owner   = THIS_MODULE,
    .read    = kfetch_read,
    .write   = kfetch_write,
    .open    = kfetch_open,
    .release = kfetch_release,
};

static char *kfetch_devnode(struct device *dev, umode_t *mode){
    if (!mode) return NULL;
    *mode = 0666;
    return NULL;
}

static int kfetch_init(void){

    int  cdev_ret = -1; 

#if func_call_check
    pr_info("%s call.\n", __func__);
#endif

    // alloc_ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    kfetch_major = register_chrdev(0, DEVICE_NAME, &kfetch_ops);
    
    cdev_init(&kfetch_cdev, &kfetch_ops); 
    // cdev_ret = cdev_add(&kfetch_cdev, dev, 1);
    cdev_ret = cdev_add(&kfetch_cdev, MKDEV(kfetch_major, 0), 1); 
    if (cdev_ret) 
        goto error;

    _class = class_create(THIS_MODULE, DEVICE_NAME); 
    if (IS_ERR(_class)){
        pr_alert("class_create failed\n");
        return -1;
    }

    _class->devnode = kfetch_devnode;

    device_create(_class, NULL, MKDEV(kfetch_major, 0), NULL, DEVICE_NAME);

    pr_info("%s driver, major: %d installed.\n", DEVICE_NAME, kfetch_major);
    pr_info("Device created on /dev/%s\n", DEVICE_NAME);

    return 0; 
error: 
    if (cdev_ret == 0) 
        cdev_del(&kfetch_cdev); 
    return -1; 
   
}


static void kfetch_exit(void){

#if func_call_check
    pr_info("%s call.\n", __func__);
#endif

    device_destroy(_class, MKDEV(kfetch_major, 0));

    class_destroy(_class);

    cdev_del(&kfetch_cdev);

    unregister_chrdev(kfetch_major, DEVICE_NAME); 

    pr_info("%s driver removed.\n", DEVICE_NAME); 
    
}

module_init(kfetch_init);
module_exit(kfetch_exit);