#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/highmem.h>

#ifdef CONFIG_RKP_CFP_FIX_SMC_BUG
#include <linux/rkp_cfp.h>
#endif
#define	CMD_READ_SYSTEM_IMAGE_CHECK_STATUS 3

static inline u64 exynos_smc_verity(u64 cmd, u64 arg1, u64 arg2, u64 arg3)
{
    register u64 reg0 __asm__("x0") = cmd;
    register u64 reg1 __asm__("x1") = arg1;
    register u64 reg2 __asm__("x2") = arg2;
    register u64 reg3 __asm__("x3") = arg3;

    __asm__ volatile (
#ifdef CONFIG_RKP_CFP_FIX_SMC_BUG
	PRE_SMC_INLINE
#endif
        "dsb    sy\n"
        "smc    0\n"
#ifdef CONFIG_RKP_CFP_FIX_SMC_BUG
	POST_SMC_INLINE
#endif
        : "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)

    );

    return reg0;
}


static int verity_scm_call(void)
{
	return exynos_smc_verity(0x83000006, CMD_READ_SYSTEM_IMAGE_CHECK_STATUS, 0, 0);
}

#define DRIVER_DESC   "Read whether odin flash succeeded"


#if 0
ssize_t	dmverity_read(struct file *filep, char __user *buf, size_t size, loff_t *offset)
{
	uint32_t	odin_flag;
	//int ret;

	/* First check is to get rid of integer overflow exploits */
	if (size < sizeof(uint32_t)) {
		printk(KERN_ERR"Size must be atleast %d\n", (int)sizeof(uint32_t));
		return -EINVAL;
	}

	odin_flag = verity_scm_call();
	printk(KERN_INFO"dmverity: odin flag: %x\n", odin_flag);

	if (copy_to_user(buf, &odin_flag, sizeof(uint32_t))) {
		printk(KERN_ERR"Copy to user failed\n");
		return -1;
	} else
		return sizeof(uint32_t);
}
#endif

static int dmverity_read(struct seq_file *m, void *v){
	int odin_flag = 0;
	unsigned char ret_buffer[10];
	odin_flag = verity_scm_call();

	memset(ret_buffer, 0, sizeof(ret_buffer));
	snprintf(ret_buffer, sizeof(ret_buffer), "%08x\n", odin_flag);
	seq_write(m, ret_buffer, sizeof(ret_buffer));
	printk(KERN_INFO"dmverity: odin_flag: %x\n", odin_flag);
	return 0;
}
static int dmverity_open(struct inode *inode, struct file *filep){
	return single_open(filep, dmverity_read, NULL);
}

static const struct file_operations dmverity_proc_fops = {
	.open       = dmverity_open,
	.read	    = seq_read,
	
};

/**
 *      dmverity_odin_flag_read_init -  Initialization function for DMVERITY
 *
 *      It creates and initializes dmverity proc entry with initialized read handler
 */
static int __init dmverity_odin_flag_read_init(void)
{
	//extern int boot_mode_recovery;
	if (/* boot_mode_recovery == */ 1) {
		/* Only create this in recovery mode. Not sure why I am doing this */
		if (proc_create("dmverity_odin_flag", 0644,NULL, &dmverity_proc_fops) == NULL) {
			printk(KERN_ERR"dmverity_odin_flag_read_init: Error creating proc entry\n");
			goto error_return;
		}
	        printk(KERN_INFO"dmverity_odin_flag_read_init:: Registering /proc/dmverity_odin_flag Interface \n");
	} else {
		printk(KERN_INFO"dmverity_odin_flag_read_init:: not enabling in non-recovery mode\n");
		goto error_return;
	}

        return 0;

error_return:
	return -1;
}


/**
 *      dmverity_odin_flag_read_exit -  Cleanup Code for DMVERITY
 *
 *      It removes /proc/dmverity proc entry and does the required cleanup operations
 */
static void __exit dmverity_odin_flag_read_exit(void)
{
        remove_proc_entry("dmverity_odin_flag", NULL);
        printk(KERN_INFO"Deregistering /proc/dmverity_odin_flag interface\n");
}


module_init(dmverity_odin_flag_read_init);
module_exit(dmverity_odin_flag_read_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
