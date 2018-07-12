/*
 * Procedures for exposing kaslr debugging information.
 *
 * Jia Ma,  Samsung Research America,  Sep 2015
 * Copyright (C) 2015 Jia Ma.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>

#if defined(CONFIG_DEBUG_FS)


static int kaslr_debug_show(struct seq_file *m, void *private){
	extern u64 *__boot_kernel_offset; 
	u64 *kernel_addr = (u64 *) &__boot_kernel_offset;
	
	seq_printf(m, "0x%016llx..0x%016llx..\n",
		   (kernel_addr[0]+kernel_addr[1]),
		   kernel_addr[1]+kernel_addr[0]-kernel_addr[2]);


	return 0 ;
}


static int kaslr_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, kaslr_debug_show, inode->i_private);
}

static const struct file_operations kaslr_debug_fops = {
	.open = kaslr_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init kaslr_init_debugfs(void)
{
	struct dentry *root = debugfs_create_dir("kaslr", NULL);
	if (!root)
		return -ENXIO;
	debugfs_create_file("kaslr", S_IRUSR, root, NULL , &kaslr_debug_fops);

	return 0;
}


__initcall(kaslr_init_debugfs);

#endif /*CONFIG_DEBUG_FS*/
