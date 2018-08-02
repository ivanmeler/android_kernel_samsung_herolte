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
#include <linux/slab.h>

#include <linux/rkp_cfp.h>


void print_all_rrk(void){
	struct task_struct * tsk;
	struct thread_info * thread;
	
	for_each_process(tsk){
		thread = task_thread_info(tsk);
		printk("rrk=0x%16lx, %s\n", thread->rrk, tsk->comm);    
	}
}

int is_prefix(const char * prefix, const char * string) 
{
	return strncmp(prefix, string, strlen(prefix)) == 0;
}



static ssize_t cfp_write(struct file *file, const char __user *buf,
				size_t datalen, loff_t *ppos)
{
	char *data = NULL;
	ssize_t result = 0;

	if (datalen >= PAGE_SIZE)
		datalen = PAGE_SIZE - 1;

	/* No partial writes. */
	result = -EINVAL;
	if (*ppos != 0)
		goto out;

	result = -ENOMEM;
	data = kmalloc(datalen + 1, GFP_KERNEL);
	if (!data)
		goto out;

	*(data + datalen) = '\0';

	result = -EFAULT;
	if (copy_from_user(data, buf, datalen))
		goto out;

	if (is_prefix("dump_stack", data)) {
		dump_stack();
	} else if (is_prefix("no_dec_dump_stack", data)) {
		dump_stack_dec = 0x0;//disable dump_stack_dec
		dump_stack();
		dump_stack_dec = 0x1;
	} else if (is_prefix("print_rrk", data)) {
		print_all_rrk();
	} else {
		result = -EINVAL;
	}

out:
	kfree(data);
	return result;
}

ssize_t	cfp_read(struct file *filep, char __user *buf, size_t size, loff_t *offset)
{	
	printk("echo dump_stack     > /proc/cfp_debug  --> will dump the ROPP encrypted stack\n");
	printk("echo dec_dump_stack > /proc/cfp_debug  --> will dump the ROPP decrypted stack\n");
	printk("echo print_rrk      > /proc/cfp_debug  --> will print RRK of all processes\n");
	return 0;
}

static const struct file_operations cfp_proc_fops = {
	.read		= cfp_read,
	.write		= cfp_write,
};

static int __init cfp_debug_read_init(void)
{
	if (proc_create("cfp_debug", 0644,NULL, &cfp_proc_fops) == NULL) {
		printk(KERN_ERR"cfp_debug_read_init: Error creating proc entry\n");
		goto error_return;
	}
	return 0;

error_return:
	return -1;
}

static void __exit cfp_debug_read_exit(void)
{
	remove_proc_entry("cfp_debug", NULL);
	printk(KERN_INFO"Deregistering /proc/cfp_debug Interface\n");
}

module_init(cfp_debug_read_init);
module_exit(cfp_debug_read_exit);
