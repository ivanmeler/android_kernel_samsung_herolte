/**
   @copyright
   Copyright (c) 2013 - 2015, INSIDE Secure Oy. All rights reserved.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/moduleparam.h>

#include "kernelspd_internal.h"
#include "package_version.h"

MODULE_DESCRIPTION("Kernel IPsec SPD " PACKAGE_VERSION);

static int init_called = 0;

#ifdef DEBUG_LIGHT

#include "debug_filter.h"

#define MAX_DEBUG_STRING_LEN 1024
static char debug_filter_string[MAX_DEBUG_STRING_LEN];
static struct kparam_string kps =
{
    .string                 = debug_filter_string,
    .maxlen                 = MAX_DEBUG_STRING_LEN,
};

static int
debug_filter_string_set(
        const char *arg,
        struct kernel_param *kp)
{
    if (strlen(arg) >= MAX_DEBUG_STRING_LEN)
    {
        printk(KERN_ERR "debug_filter_string too long\n");
        return -ENOSPC;
    }

    printk(KERN_INFO "Using debug string: %s\n", arg);

    strcpy(debug_filter_string, arg);
    if (init_called != 0)
    {
        write_lock_bh(&spd_lock);
        debug_filter_set_string(debug_filter_string);
        write_unlock_bh(&spd_lock);
    }
    else
    {
        debug_filter_set_string(debug_filter_string);
    }

    return 0;
}


module_param_call(
        debug_filter_string,
        debug_filter_string_set,
        param_get_string,
        &kps,
        0644);

MODULE_PARM_DESC(
        debug_filter_string,
        "Filter for debug logs. [+-] level,flow,module,file,func ; ...");
#endif


struct IPSelectorDb spd;
rwlock_t spd_lock;

int __init linux_spd_init(void)
{
    int status = 0;

    ip_selector_db_init(&spd);

    rwlock_init(&spd_lock);
    init_called = 1;

    status = spd_proc_init();

    if (status != 0)
    {
        spd_proc_uninit();
    }

    DEBUG_HIGH(main, "Kernel spd initialised.");

    printk(KERN_INFO "%s\n", PACKAGE_VERSION);
    printk(KERN_INFO "vpnclient kernel spd loaded.\n");

    return status;
}


void __exit linux_spd_cleanup(void)
{
    DEBUG_HIGH(main, "Kernel spd cleaning up.");

    spd_hooks_uninit();
    spd_proc_uninit();

    printk(KERN_INFO "vpnclient kernel spd removed.\n");
}


MODULE_LICENSE("GPL");
module_init(linux_spd_init);
module_exit(linux_spd_cleanup);
