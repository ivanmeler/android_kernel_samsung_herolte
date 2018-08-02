#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/bootmem.h>

#ifdef CONFIG_NO_BOOTMEM
#include <linux/memblock.h>
#endif

#ifdef CONFIG_TIMA_RKP
#include <linux/rkp_entry.h>
#endif

#ifdef CONFIG_TIMA_RKP
#define DEBUG_RKP_LOG_START	TIMA_DEBUG_LOG_START
#define SECURE_RKP_LOG_START	TIMA_SEC_LOG 
#endif
#define	DEBUG_LOG_SIZE	(1<<20)
#define TIMA_DEBUG_LOGGING_START	(0xB1000000)
#define TIMA_SECURE_LOGGING_START (TIMA_DEBUG_LOGGING_START + DEBUG_LOG_SIZE)
#define	DEBUG_LOG_MAGIC	(0xaabbccdd)
#define	DEBUG_LOG_ENTRY_SIZE	128

typedef struct debug_log_entry_s {
	uint32_t	timestamp;          /* timestamp at which log entry was made*/
	uint32_t	logger_id;          /* id is 1 for tima, 2 for lkmauth app  */
#define	DEBUG_LOG_MSG_SIZE	(DEBUG_LOG_ENTRY_SIZE - sizeof(uint32_t) - sizeof(uint32_t))
	char	log_msg[DEBUG_LOG_MSG_SIZE];      /* buffer for the entry                 */
} __attribute__ ((packed)) debug_log_entry_t;

typedef struct debug_log_header_s {
	uint32_t	magic;              /* magic number                         */
	uint32_t	log_start_addr;     /* address at which log starts          */
	uint32_t	log_write_addr;     /* address at which next entry is written*/
	uint32_t	num_log_entries;    /* number of log entries                */
	char	padding[DEBUG_LOG_ENTRY_SIZE - 4 * sizeof(uint32_t)];
} __attribute__ ((packed)) debug_log_header_t;

#define DRIVER_DESC   "A kernel module to read tima debug log"

unsigned long *tima_log_addr = 0;
unsigned long *tima_debug_log_addr = 0;
unsigned long *tima_secure_log_addr = 0;
#ifdef CONFIG_TIMA_RKP
unsigned long *tima_debug_rkp_log_addr = 0;
unsigned long *tima_secure_rkp_log_addr = 0;
#endif

#ifdef CONFIG_TIMA_RKP
static int  tima_setup_rkp_mem(void){
#ifdef CONFIG_NO_BOOTMEM
	if (memblock_is_region_reserved(TIMA_DEBUG_LOG_START, TIMA_DEBUG_LOG_SIZE) ||
			memblock_reserve(TIMA_DEBUG_LOG_START, TIMA_DEBUG_LOG_SIZE)) {
#else
	if(reserve_bootmem(TIMA_DEBUG_LOG_START, TIMA_DEBUG_LOG_SIZE, BOOTMEM_EXCLUSIVE)){
#endif
		pr_err("%s: RKP failed reserving size %d " \
			   "at base 0x%x\n", __func__, TIMA_DEBUG_LOG_SIZE, TIMA_DEBUG_LOG_START);
		goto out;
	}
	pr_info("RKP :%s, base:%x, size:%x \n", __func__,TIMA_DEBUG_LOG_START, TIMA_DEBUG_LOG_SIZE);

#ifdef CONFIG_NO_BOOTMEM
	if (memblock_is_region_reserved(TIMA_SEC_LOG, TIMA_SEC_LOG_SIZE) ||
			memblock_reserve(TIMA_SEC_LOG, TIMA_SEC_LOG_SIZE)) {
#else
	if(reserve_bootmem(TIMA_SEC_LOG, TIMA_SEC_LOG_SIZE, BOOTMEM_EXCLUSIVE)){
#endif
		pr_err("%s: RKP failed reserving size %d " \
			   "at base 0x%x\n", __func__, TIMA_SEC_LOG_SIZE, TIMA_SEC_LOG);
		goto out;
	}
	pr_info("RKP :%s, base:%x, size:%x \n", __func__,TIMA_SEC_LOG, TIMA_SEC_LOG_SIZE);

#ifdef CONFIG_NO_BOOTMEM
	if (memblock_is_region_reserved(TIMA_PHYS_MAP, TIMA_PHYS_MAP_SIZE) ||
			memblock_reserve(TIMA_PHYS_MAP, TIMA_PHYS_MAP_SIZE)) {
#else
	if(reserve_bootmem(TIMA_PHYS_MAP,  TIMA_PHYS_MAP_SIZE, BOOTMEM_EXCLUSIVE)){
#endif
		pr_err("%s: RKP failed reserving size %d "					\
			   "at base 0x%x\n", __func__, TIMA_PHYS_MAP_SIZE, TIMA_PHYS_MAP);
		goto out;
	}
	pr_info("RKP :%s, base:%x, size:%x \n", __func__,TIMA_PHYS_MAP, TIMA_PHYS_MAP_SIZE);

#ifdef CONFIG_NO_BOOTMEM
	if (memblock_is_region_reserved(TIMA_DASHBOARD_START, TIMA_DASHBOARD_SIZE) ||
			memblock_reserve(TIMA_DASHBOARD_START, TIMA_DASHBOARD_SIZE)) {
#else
	if(reserve_bootmem(TIMA_DASHBOARD_START,  TIMA_DASHBOARD_SIZE, BOOTMEM_EXCLUSIVE)){
#endif
		pr_err("%s: RKP failed reserving size %d "					\
			   "at base 0x%x\n", __func__, TIMA_DASHBOARD_SIZE, TIMA_DASHBOARD_START);
		goto out;
	}
	pr_info("RKP :%s, base:%x, size:%x \n", __func__,TIMA_DASHBOARD_START, TIMA_DASHBOARD_SIZE);

#ifdef CONFIG_NO_BOOTMEM
	if (memblock_is_region_reserved(TIMA_ROBUF_START, TIMA_ROBUF_SIZE) ||
			memblock_reserve(TIMA_ROBUF_START, TIMA_ROBUF_SIZE)) {
#else
	if(reserve_bootmem(TIMA_ROBUF_START,  TIMA_ROBUF_SIZE, BOOTMEM_EXCLUSIVE)){
#endif
		pr_err("%s: RKP failed reserving size %d "					\
			   "at base 0x%x\n", __func__, TIMA_ROBUF_SIZE, TIMA_ROBUF_START);
		goto out;
	}
	pr_info("RKP :%s, base:%x, size:%x \n", __func__,TIMA_ROBUF_START, TIMA_ROBUF_SIZE);

#ifdef CONFIG_NO_BOOTMEM
	if (memblock_is_region_reserved(TIMA_VMM_START, TIMA_VMM_SIZE) ||
			memblock_reserve(TIMA_VMM_START, TIMA_VMM_SIZE)) {
#else
	if(reserve_bootmem(TIMA_VMM_START,  TIMA_VMM_SIZE, BOOTMEM_EXCLUSIVE)){
#endif
		pr_err("%s: RKP failed reserving size %d "					\
			   "at base 0x%x\n", __func__, TIMA_VMM_SIZE, TIMA_VMM_START);
		goto out;
	}
	pr_info("RKP :%s, base:%x, size:%x \n", __func__,TIMA_VMM_START, TIMA_VMM_SIZE);

	return 1; 
 out: 
	return 0; 

}
#else /* !CONFIG_TIMA_RKP*/
static int tima_setup_rkp_mem(void){
	return 1;
}
#endif
static int __init sec_tima_log_setup(char *str)
{
	if( !tima_setup_rkp_mem())  goto out; 

	return 1;
out:
	return 0;
}
__setup("sec_tima_log=", sec_tima_log_setup);

ssize_t	tima_read(struct file *filep, char __user *buf, size_t size, loff_t *offset)
{
	/* First check is to get rid of integer overflow exploits */
	if(size > DEBUG_LOG_SIZE || (*offset) + size > DEBUG_LOG_SIZE) {
		pr_err("Extra read\n");
		return -EINVAL;
	}
	if( !strcmp(filep->f_path.dentry->d_iname, "tima_debug_log"))
		tima_log_addr = tima_debug_log_addr;
	else if( !strcmp(filep->f_path.dentry->d_iname, "tima_secure_log"))
		tima_log_addr = tima_secure_log_addr;
#ifdef CONFIG_TIMA_RKP
	else if(!strcmp(filep->f_path.dentry->d_iname, "tima_debug_rkp_log")) {
		if (*offset >= TIMA_DEBUG_LOG_SIZE) {
			return -EINVAL;
		} else if (*offset + size > TIMA_DEBUG_LOG_SIZE) {
			size = (TIMA_DEBUG_LOG_SIZE) - *offset;
		}
		tima_log_addr = tima_debug_rkp_log_addr;
	}
	else if(!strcmp(filep->f_path.dentry->d_iname, "tima_secure_rkp_log")) {
		if (*offset >= TIMA_SEC_LOG_SIZE) {
			return -EINVAL;
		} else if (*offset + size > TIMA_SEC_LOG_SIZE) {
			size = (TIMA_SEC_LOG_SIZE) - *offset;
		}
		tima_log_addr = tima_secure_rkp_log_addr;
	}
#endif
	else {
		pr_err("NO tima*log\n");
		return -1;
	}
	if(copy_to_user(buf, (const char *)tima_log_addr + (*offset), size)) {
		pr_err("Copy to user failed\n");
		return -1;
	} else {
		*offset += size;
		return size;
	}
}

static const struct file_operations tima_proc_fops = {
	.read		= tima_read,
};

/**
 *      tima_debug_log_read_init -  Initialization function for TIMA
 *
 *      It creates and initializes tima proc entry with initialized read handler
 */
static int __init tima_debug_log_read_init(void)
{
	if(proc_create("tima_debug_log", 0644, NULL, &tima_proc_fops) == NULL) {
		pr_err("tima_debug_log_read_init: Error creating proc entry\n");
		goto error_return;
	}
	if(proc_create("tima_secure_log", 0644, NULL, &tima_proc_fops) == NULL) {
		pr_err("tima_secure_log_read_init: Error creating proc entry\n");
		goto remove_debug_entry;
	}
	pr_info("%s: Registering /proc/tima_debug_log Interface\n", __func__);

#ifdef CONFIG_TIMA_RKP
	if (proc_create("tima_debug_rkp_log", 0644,NULL, &tima_proc_fops) == NULL) {
		printk(KERN_ERR"tima_debug_rkp_log_read_init: Error creating proc entry\n");
		goto remove_secure_entry;
	}
	if (proc_create("tima_secure_rkp_log", 0644,NULL, &tima_proc_fops) == NULL) {
		printk(KERN_ERR"tima_secure_rkp_log_read_init: Error creating proc entry\n");
		goto remove_debug_rkp_entry;
	}
#endif
	tima_debug_log_addr = (unsigned long *)phys_to_virt(TIMA_DEBUG_LOGGING_START);
	tima_secure_log_addr = (unsigned long *)phys_to_virt(TIMA_SECURE_LOGGING_START);
#ifdef CONFIG_TIMA_RKP
	tima_debug_rkp_log_addr  = (unsigned long *)phys_to_virt(DEBUG_RKP_LOG_START);
	tima_secure_rkp_log_addr = (unsigned long *)phys_to_virt(SECURE_RKP_LOG_START);
#endif
	return 0;

#ifdef CONFIG_TIMA_RKP
remove_debug_rkp_entry:
	remove_proc_entry("tima_debug_rkp_log", NULL);
remove_secure_entry:
	remove_proc_entry("tima_secure_log", NULL);
#endif
remove_debug_entry:
	remove_proc_entry("tima_debug_log", NULL);
error_return:
	return -1;
}

/**
 *      tima_debug_log_read_exit -  Cleanup Code for TIMA
 *
 *      It removes /proc/tima proc entry and does the required cleanup operations
 */
static void __exit tima_debug_log_read_exit(void)
{
	remove_proc_entry("tima_debug_log", NULL);
	remove_proc_entry("tima_secure_log", NULL);
#ifdef CONFIG_TIMA_RKP
	remove_proc_entry("tima_debug_rkp_log", NULL);
	remove_proc_entry("tima_secure_rkp_log", NULL);
#endif
	pr_info("Deregistering /proc/tima_debug_log Interface\n");
}

module_init(tima_debug_log_read_init);
module_exit(tima_debug_log_read_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
