// hello.c
#include <linux/syscalls.h>
#include <linux/kernel.h>

SYSCALL_DEFINE(hello)
{
    printk("Hello world\n");
    printk("310555007\n");
    return 0;
}