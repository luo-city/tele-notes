#include <linux/init.h>
#include <linux/module.h>

static int __init hello_init(void) {
    printk(KERN_ERR "Hello, world\n");
    return 0;
}

static void __exit hello_exit(void) {
    printk(KERN_ERR "Goodbye, cruel world\n");
    return;
}

/*
 * makefile
 obj-m := hello.o
 all:
    $(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
 clean:
     rm *.o *.symvers hello.mod.*
 */