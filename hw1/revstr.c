// revstr.c
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>


SYSCALL_DEFINE2(revstr, unsigned int, len, const char __user *, str)
{

    int i=0;
    char buf[256];
    
    if(copy_from_user(buf, str, len) != 0 ) return -EFAULT;

    buf[len] = '\0';
    printk("The origin string: %s\n", buf);
    
    for(i=0;i<len/2;i++){
        buf[i] ^= buf[len-1-i];
        buf[len-1-i] ^= buf[i];
        buf[i] ^= buf[len-1-i];

    }  
    printk("The reversed string: %s\n", buf);
    
    return 0;
}