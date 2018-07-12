/*
 * include/linux/knox_kap.h
 *
 * Include file for the KNOX KAP mode.
 */

#ifndef _LINUX_KNOX_KAP_H
#define _LINUX_KNOX_KAP_H

#include <linux/types.h>

extern const struct file_operations knox_kap_fops;

extern unsigned int knox_kap_status;

#endif /* _LINUX_KNOX_KAP_H */
