/*  staging/samsung/sec_initcall_debug.c
*
* Copyright (C) 2015 Samsung Electronics
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*
*/

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sec_debug.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>

struct sec_initcall_debug_data {
	struct list_head list;
	const char *name;
	unsigned long long duration;
	bool is_pm;
};

static LIST_HEAD(initcall_list);
static LIST_HEAD(initcall_sorted_list);

static int time_ms;
static bool is_sorted;

static int sec_initcall_debug_seq_show(struct seq_file *f, void *v)
{
	struct sec_initcall_debug_data *data, *data2, *tmp;
	bool is_added = false;

	if (!is_sorted) {
		list_for_each_entry_safe(data, tmp, &initcall_list, list) {
			is_added = false;
			list_for_each_entry(data2, &initcall_sorted_list, list) {
				if ((data2->duration < data->duration) && !is_added) {
					list_add(&data->list, data2->list.prev);
					is_added = true;
					break;
				}
			}
			if (!is_added)
				list_add_tail(&data->list, &initcall_sorted_list);
		}

		is_sorted = true;
	}

	seq_puts(f, "function name\t\t\t\t\t\ttime\n");
	seq_puts(f, "-------------------------------------------------\n");

	list_for_each_entry(data, &initcall_sorted_list, list) {
		if (data->duration < time_ms)
			break;
		seq_printf(f, "%-50s : %8llu\n", data->name, data->duration);
	}

	return 0;
}

static int sec_initcall_debug_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sec_initcall_debug_seq_show, NULL);
}

static ssize_t sec_initcall_debug_write(struct file *filp,
	const char __user *buf, size_t count, loff_t *t)
{
	int ret = 0;

	ret = kstrtoint(buf, 10, &time_ms);
	if (1 != ret)
		printk(KERN_ERR
			"%s : wrong input data - enter the ms",
			 __func__);

	time_ms *= 1000;

	return count;
}

static const struct file_operations sec_initcall_debug_proc_fops = {
	.open	= sec_initcall_debug_open,
	.read	= seq_read,
	.llseek	= seq_lseek,
	.release	= single_release,
	.write	= sec_initcall_debug_write,
};

void sec_initcall_debug_add(initcall_t fn,
	unsigned long long duration)
{
	struct sec_initcall_debug_data *ddata;

	ddata = kzalloc(sizeof(struct sec_initcall_debug_data), GFP_KERNEL);
	if (!ddata)
		printk(KERN_ERR "initcall : failed to allocate\n");
	else {
		ddata->name = kasprintf(GFP_KERNEL, "%pF", fn);
		ddata->duration = duration;
		list_add_tail(&ddata->list, &initcall_list);
	}
}

static int __init sec_initcall_debug_init(void)
{
	proc_create("initcall_debug", 0, NULL,
		&sec_initcall_debug_proc_fops);
	return 0;
}

module_init(sec_initcall_debug_init);

