/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *
 * Network Context Metadata Module[NCM]:Implementation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* START_OF_KNOX_NPA */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/sctp.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/err.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/udp.h>
#include <linux/sctp.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/net.h>
#include <linux/inet.h>

#include <net/sock.h>
#include <net/ncm.h>
#include <net/ip.h>
#include <net/protocol.h>

#include <asm/current.h>

#define SUCCESS 0

#define FAILURE 1
/* fifo size in elements (bytes) */
#define FIFO_SIZE   1024
#define WAIT_TIMEOUT  10000 /*milliseconds */
/* Lock to maintain orderly insertion of elements into kfifo */
static DEFINE_MUTEX(ncm_lock);

static unsigned int ncm_activated_flag = 1;

static unsigned int ncm_deactivated_flag;

static unsigned int device_open_count;

static struct nf_hook_ops nfho_ipv4_pr;

static struct nf_hook_ops nfho_ipv6_pr;

static struct workqueue_struct *ewq;

wait_queue_head_t ncm_wq;

static atomic_t is_ncm_enabled = ATOMIC_INIT(0);

extern struct knox_socket_metadata knox_socket_metadata;

DECLARE_KFIFO(knox_sock_info, struct knox_socket_metadata, FIFO_SIZE);


/* The function is used to check if ncm feature has been enabled or not; The default value is disabled */
unsigned int check_ncm_flag(void)
{
	return atomic_read(&is_ncm_enabled);
}
EXPORT_SYMBOL(check_ncm_flag);


/** The funcation is used to chedk if the kfifo is active or not;
 *  If the kfifo is active, then the socket metadata would be inserted into the queue which will be read by the user-space;
 *  By default the kfifo is inactive;
 */
bool kfifo_status(void)
{
	bool isKfifoActive = false;
	if (kfifo_initialized(&knox_sock_info)) {
		NCM_LOGD("The fifo queue for ncm was already intialized \n");
		isKfifoActive = true;
	} else {
		NCM_LOGE("The fifo queue for ncm is not intialized \n");
		isKfifoActive = false;
	}
	return isKfifoActive;
}
EXPORT_SYMBOL(kfifo_status);


/** The function is used to insert the socket meta-data into the fifo queue; insertion of data will happen in a seperate kernel thread;
 *  The meta data information will be collected from the context of the process which originates it;
 *  If the kfifo is full, then the kfifo is freed before inserting new meta-data;
 */
void insert_data_kfifo(struct work_struct *pwork)
{
	struct knox_socket_metadata *knox_socket_metadata;

	knox_socket_metadata = container_of(pwork, struct knox_socket_metadata, work_kfifo);

	if (IS_ERR(knox_socket_metadata)) {
		NCM_LOGE("inserting data into the kfifo failed due to unknown error \n");
		goto err;
	}

	if (mutex_lock_interruptible(&ncm_lock)) {
		NCM_LOGE("inserting data into the kfifo failed due to an interuppt \n");
		goto err;
	}

	if (kfifo_initialized(&knox_sock_info)) {
		if (kfifo_is_full(&knox_sock_info)) {
		   NCM_LOGD("The kfifo is full and need to free it \n");
		   kfree(knox_socket_metadata);
		} else {
			kfifo_in(&knox_sock_info, knox_socket_metadata, 1);
			kfree(knox_socket_metadata);
		}
	} else {
		kfree(knox_socket_metadata);
	}
	mutex_unlock(&ncm_lock);
	return;

	err:
		if (knox_socket_metadata != NULL)
			kfree(knox_socket_metadata);
		return;
}

/** The function is used to insert the socket meta-data into the kfifo in a seperate kernel thread;
 *  The kernel threads which handles the responsibility of inserting the meta-data into the kfifo is manintained by the workqueue function;
 */
void insert_data_kfifo_kthread(struct knox_socket_metadata *knox_socket_metadata)
{
	INIT_WORK(&(knox_socket_metadata->work_kfifo), insert_data_kfifo);
	if (!ewq) {
		NCM_LOGD("ewq..Single Thread created\r\n");
		ewq = create_workqueue("ncmworkqueue");
	}
	if (ewq) {
		queue_work(ewq, &(knox_socket_metadata->work_kfifo));
	}
}
EXPORT_SYMBOL(insert_data_kfifo_kthread);


/* The function is used to check if the caller is system server or not; */
static int is_system_server(void) {
    uid_t uid = current_uid().val;
    switch(uid) {
        case 1000:
            return 1;
        case 0:
            return 1;
        default:
            break;
    }
    return 0;
}

/* The function is used to intialize the kfifo */
static void initialize_kfifo(void) {
    INIT_KFIFO(knox_sock_info);
    if(kfifo_initialized(&knox_sock_info)) {
        NCM_LOGD("The kfifo for knox ncm has been initialized \n");
        init_waitqueue_head(&ncm_wq);
    }
}

/* The function is ued to free the kfifo */
static void free_kfifo(void) {
    if(kfifo_status()) {
        NCM_LOGD("The kfifo for knox ncm which was intialized is freed \n");
        kfifo_free(&knox_sock_info);
    }
}

/* The function is used to update the flag indicating whether the feature has been enabled or not */
static void update_ncm_flag(unsigned int ncmFlag)
{
	if (ncmFlag == ncm_activated_flag)
		atomic_set(&is_ncm_enabled, ncm_activated_flag);
	else
		atomic_set(&is_ncm_enabled, ncm_deactivated_flag);
}


/** The function is used to get the source ip address of the packet and update the srcaddr parameter present in struct knox_socket_metadata
 *  The function is registered in the post-routing chain and it is needed to collect the correct source ip address if iptables is involved
 */
static unsigned int hook_func_ipv4_out(const struct nf_hook_ops *ops, struct sk_buff *skb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	struct iphdr *ip_header;
	struct udphdr *udp_header;
	struct sctphdr *sctp_header;

	if ((skb->sk) && (skb->sk->sk_protocol == IPPROTO_TCP)) {
		return NF_ACCEPT;
	} else if ((skb->sk) && (skb->sk->sk_protocol == IPPROTO_UDP)) {
		if (isIpv4PostRoutingAddressEqualsNull(skb->sk->sk_udp_daddr_v6, skb->sk->sk_udp_saddr_v6)) {
			ip_header = (struct iphdr *)skb_network_header(skb);
			udp_header = (struct udphdr *)skb_transport_header(skb);

			skb->sk->sk_udp_saddr_v6[0] = ip_header->saddr;
			skb->sk->sk_udp_sport = udp_header->source;

			skb->sk->sk_udp_daddr_v6[0] = ip_header->daddr;
			skb->sk->sk_udp_dport = udp_header->dest;
		}
	} else if ((skb->sk) && (skb->sk->sk_protocol == IPPROTO_SCTP)) {
		if (isIpv4PostRoutingAddressEqualsNull(skb->sk->sk_udp_daddr_v6, skb->sk->sk_udp_saddr_v6)) {
			ip_header = (struct iphdr *)skb_network_header(skb);
			sctp_header = (struct sctphdr *)skb_transport_header(skb);

			skb->sk->sk_udp_saddr_v6[0] = ip_header->saddr;
			skb->sk->sk_udp_daddr_v6[0] = ip_header->daddr;
			// TO DO : To check how to test the ports of sctp protocols;
			if (sctp_header != NULL) {
				skb->sk->sk_udp_sport = sctp_header->source;
				skb->sk->sk_udp_dport = sctp_header->dest;
			}
		}
	} else {
		if (skb->sk) {
			if (isIpv4PostRoutingAddressEqualsNull(skb->sk->sk_udp_daddr_v6, skb->sk->sk_udp_saddr_v6)) {
				ip_header = (struct iphdr *)skb_network_header(skb);
				skb->sk->sk_udp_saddr_v6[0] = ip_header->saddr;
				skb->sk->sk_udp_daddr_v6[0] = ip_header->daddr;
			}
		}
	}
	return NF_ACCEPT;
}

static unsigned int hook_func_ipv6_out(const struct nf_hook_ops *ops, struct sk_buff *skb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	struct ipv6hdr *ipv6_header;
	struct udphdr *udp_header;
	struct sctphdr *sctp_header;

	if ((skb->sk) && (skb->sk->sk_protocol == IPPROTO_TCP)) {
		return NF_ACCEPT;
	} else if ((skb->sk) && (skb->sk->sk_protocol == IPPROTO_UDP)) {
		if (isIpv6PostRoutingAddressEqualsNull(skb->sk->sk_udp_daddr_v6, skb->sk->sk_udp_saddr_v6)) {
			ipv6_header = (struct ipv6hdr *)skb_network_header(skb);
			udp_header = (struct udphdr *)skb_transport_header(skb);

			memcpy(skb->sk->sk_udp_saddr_v6, &ipv6_header->saddr, sizeof(skb->sk->sk_udp_saddr_v6));
			skb->sk->sk_udp_sport = udp_header->source;

			memcpy(skb->sk->sk_udp_daddr_v6, &ipv6_header->daddr, sizeof(skb->sk->sk_udp_daddr_v6));
			skb->sk->sk_udp_dport = udp_header->dest;
		}
	} else if ((skb->sk) && (skb->sk->sk_protocol == IPPROTO_SCTP)) {
		if (isIpv6PostRoutingAddressEqualsNull(skb->sk->sk_udp_daddr_v6, skb->sk->sk_udp_saddr_v6)) {
			ipv6_header = (struct ipv6hdr *)skb_network_header(skb);
			sctp_header = (struct sctphdr *)skb_transport_header(skb);

			memcpy(skb->sk->sk_udp_saddr_v6, &ipv6_header->saddr, sizeof(skb->sk->sk_udp_saddr_v6));
			memcpy(skb->sk->sk_udp_daddr_v6, &ipv6_header->daddr, sizeof(skb->sk->sk_udp_daddr_v6));
			// TO DO : To check how to test the ports of sctp protocols;
			if (sctp_header != NULL) {
				skb->sk->sk_udp_sport = sctp_header->source;
				skb->sk->sk_udp_dport = sctp_header->dest;
			}
		}
	} else {
		if (skb->sk) {
			if (isIpv6PostRoutingAddressEqualsNull(skb->sk->sk_udp_daddr_v6, skb->sk->sk_udp_saddr_v6)) {
				ipv6_header = (struct ipv6hdr *)skb_network_header(skb);
				memcpy(skb->sk->sk_udp_saddr_v6, &ipv6_header->saddr, sizeof(skb->sk->sk_udp_saddr_v6));
				memcpy(skb->sk->sk_udp_daddr_v6, &ipv6_header->daddr, sizeof(skb->sk->sk_udp_daddr_v6));
			}
		}
	}
	return NF_ACCEPT;
}

/* The fuction registers to listen for packets in the post-routing chain to collect the correct source ip address detail; */
static void registerNetfilterHooks(void)
{
	nfho_ipv4_pr.hook = hook_func_ipv4_out;
	nfho_ipv4_pr.hooknum = NF_INET_POST_ROUTING;
	nfho_ipv4_pr.pf = PF_INET;
	nfho_ipv4_pr.priority = NF_IP_PRI_LAST;

	nfho_ipv6_pr.hook = hook_func_ipv6_out;
	nfho_ipv6_pr.hooknum = NF_INET_POST_ROUTING;
	nfho_ipv6_pr.pf = PF_INET6;
	nfho_ipv6_pr.priority = NF_IP6_PRI_LAST;

	nf_register_hook(&nfho_ipv4_pr);
	nf_register_hook(&nfho_ipv6_pr);
}

/* The function un-registers the netfilter hook */
static void unregisterNetFilterHooks(void)
{
	nf_unregister_hook(&nfho_ipv4_pr);
	nf_unregister_hook(&nfho_ipv6_pr);
}

/* Function to collect the socket meta-data information. This function is called from af_inet.c when the socket gets closed. */
void knox_collect_socket_data(struct socket *sock)
{
	struct knox_socket_metadata *ksm = kzalloc(sizeof(struct knox_socket_metadata), GFP_KERNEL);

	struct sock *sk = sock->sk;
	struct inet_sock *inet = inet_sk(sk);

	struct pid *pid_struct;
	struct task_struct *task;

	struct pid *parent_pid_struct;
	struct task_struct *parent_task;

	struct timespec close_timespec;

	struct ipv6_pinfo *np;
	char ipv6_address[INET6_ADDRSTRLEN_NAP] = {0};

	char full_process_name[128] = {0};
	char full_parent_process_name[128] = {0};
	int process_returnValue;
	int parent_returnValue;

	if (ksm == NULL)
		return;

	if (!(sk->sk_family == AF_INET) && !(sk->sk_family == AF_INET6)) {
		printk("NPA feature will not record the invalid address type \n");
		kfree(ksm);
		return;
	}

	#if IS_ENABLED(CONFIG_IPV6)
	if (sk->sk_family == AF_INET6) {
		np = inet6_sk(sk);
		if (np == NULL) {
			kfree(ksm);
			return;
		}
	}
	#endif

	pid_struct = find_get_pid(current->tgid);
	task = pid_task(pid_struct, PIDTYPE_PID);
	if (task != NULL) {
		process_returnValue = get_cmdline(task, full_process_name, sizeof(full_process_name)-1);
		if (process_returnValue > 0) {
			memcpy(ksm->process_name, full_process_name, sizeof(ksm->process_name));
		} else {
			memcpy(ksm->process_name, task->comm, sizeof(task->comm));
		}
		if (task->parent != NULL) {
			parent_pid_struct = find_get_pid(task->parent->tgid);
			parent_task = pid_task(parent_pid_struct, PIDTYPE_PID);
			if (parent_task != NULL) {
				parent_returnValue = get_cmdline(parent_task, full_parent_process_name, sizeof(full_parent_process_name)-1);
				if (parent_returnValue > 0) {
					memcpy(ksm->parent_process_name, full_parent_process_name, sizeof(ksm->parent_process_name));
				} else {
					memcpy(ksm->parent_process_name, parent_task->comm, sizeof(parent_task->comm));
				}
				ksm->knox_puid = parent_task->cred->uid.val;
				ksm->knox_ppid = parent_task->tgid;
			}
		}
	}

	if (sk->sk_protocol == IPPROTO_TCP) {
			// Changes to support IPV4, IPV6, IPV4-mapped-IPV6;
		switch (sk->sk_family) {
		case AF_INET6:
			sprintf(ipv6_address, "%pI6", (void *)&sk->sk_v6_rcv_saddr);
			if (isIpv6SourceAddressEqualsNull(ipv6_address)) {
				if ((!ipv6_addr_v4mapped(&np->saddr))) {
					/* Handles IPv6 */
					sprintf(ksm->srcaddr, "%pI6", (void *)&np->saddr);
					ksm->srcport = ntohs(inet->inet_sport);
					sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_v6_daddr);
					ksm->dstport = ntohs(inet->inet_dport);
				} else {
					/* Handles IPv4MappedIPv6 */
					sprintf(ksm->srcaddr, "%pI4", (void *)&inet->inet_saddr);
					ksm->srcport = ntohs(inet->inet_sport);
					sprintf(ksm->dstaddr, "%pI4", (void *)&inet->inet_daddr);
					ksm->dstport = ntohs(inet->inet_dport);
				}
			} else {
				if ((!ipv6_addr_v4mapped(&sk->sk_v6_rcv_saddr))) {
					/* Handles IPv6 */
					sprintf(ksm->srcaddr, "%pI6", (void *)&sk->sk_v6_rcv_saddr);
					ksm->srcport = ntohs(inet->inet_sport);
					sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_v6_daddr);
					ksm->dstport = ntohs(inet->inet_dport);
				} else {
					/* Handles IPv4MappedIPv6 */
					sprintf(ksm->srcaddr, "%pI4", (void *)&inet->inet_saddr);
					ksm->srcport = ntohs(inet->inet_sport);
					sprintf(ksm->dstaddr, "%pI4", (void *)&inet->inet_daddr);
					ksm->dstport = ntohs(inet->inet_dport);
				}
			}
			break;
		case AF_INET:
				/* Handles IPv4 */
			sprintf(ksm->srcaddr, "%pI4", (void *)&inet->inet_saddr);
			ksm->srcport = ntohs(inet->inet_sport);

			sprintf(ksm->dstaddr, "%pI4", (void *)&inet->inet_daddr);
			ksm->dstport = ntohs(inet->inet_dport);
			break;
		default:
			kfree(ksm);
			return;
		}
	} else if (sk->sk_protocol == IPPROTO_UDP) {
			// Changes to support IPV4, IPV6, IPV4-mapped-IPV6;
		switch (sk->sk_family) {
		case AF_INET6:
			sprintf(ipv6_address, "%pI6", (void *)&sk->sk_v6_rcv_saddr);
			if (isIpv6SourceAddressEqualsNull(ipv6_address)) {
					/* Handles IPv6 */
				if ((!ipv6_addr_v4mapped(&np->saddr))) {
					if (isIpv6PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
						sprintf(ksm->srcaddr, "%pI6", (void *)&np->saddr);
						ksm->srcport = ntohs(inet->inet_sport);
						sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_v6_daddr);
						ksm->dstport = ntohs(inet->inet_dport);
					} else {
						sprintf(ksm->srcaddr, "%pI6", (void *)&sk->sk_udp_saddr_v6);
						ksm->srcport = ntohs(sk->sk_udp_sport);
						sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_udp_daddr_v6);
						ksm->dstport = ntohs(sk->sk_udp_dport);
					}
				} else {
						/* Handles IPv4MappedIPv6 */
					if (isIpv4PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
						sprintf(ksm->srcaddr, "%pI4", (void *)&inet->inet_saddr);
						ksm->srcport = ntohs(inet->inet_sport);
						sprintf(ksm->dstaddr, "%pI4", (void *)&inet->inet_daddr);
						ksm->dstport = ntohs(inet->inet_dport);
					} else {
						sprintf(ksm->srcaddr, "%pI4", (void *)&sk->sk_udp_saddr_v6[0]);
						ksm->srcport = ntohs(sk->sk_udp_sport);
						sprintf(ksm->dstaddr, "%pI4", (void *)&sk->sk_udp_daddr_v6[0]);
						ksm->dstport = ntohs(sk->sk_udp_dport);
					}
				}
			} else {
					/* Handles IPv6 */
				if ((!ipv6_addr_v4mapped(&sk->sk_v6_rcv_saddr))) {
					if (isIpv6PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
						sprintf(ksm->srcaddr, "%pI6", (void *)&sk->sk_v6_rcv_saddr);
						ksm->srcport = ntohs(inet->inet_sport);
						sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_v6_daddr);
						ksm->dstport = ntohs(inet->inet_dport);
					} else {
						sprintf(ksm->srcaddr, "%pI6", (void *)&sk->sk_udp_saddr_v6);
						ksm->srcport = ntohs(sk->sk_udp_sport);
						sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_udp_daddr_v6);
						ksm->dstport = ntohs(sk->sk_udp_dport);
					}
				} else {
						/* Handles IPv4MappedIPv6 */
					if (isIpv4PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
						sprintf(ksm->srcaddr, "%pI4", (void *)&inet->inet_saddr);
						ksm->srcport = ntohs(inet->inet_sport);
						sprintf(ksm->dstaddr, "%pI4", (void *)&inet->inet_daddr);
						ksm->dstport = ntohs(inet->inet_dport);
					} else {
						sprintf(ksm->srcaddr, "%pI4", (void *)&sk->sk_udp_saddr_v6[0]);
						ksm->srcport = ntohs(sk->sk_udp_sport);
						sprintf(ksm->dstaddr, "%pI4", (void *)&sk->sk_udp_daddr_v6[0]);
						ksm->dstport = ntohs(sk->sk_udp_dport);
					}
				}
			}
			break;
		case AF_INET:
				/* Handles IPv4 */
			if (isIpv4PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
				sprintf(ksm->srcaddr, "%pI4", (void *)&inet->inet_saddr);
				ksm->srcport = ntohs(inet->inet_sport);

				sprintf(ksm->dstaddr, "%pI4", (void *)&inet->inet_daddr);
				ksm->dstport = ntohs(inet->inet_dport);
			} else {
				sprintf(ksm->srcaddr, "%pI4", (void *)&sk->sk_udp_saddr_v6[0]);
				ksm->srcport = ntohs(sk->sk_udp_sport);

				sprintf(ksm->dstaddr, "%pI4", (void *)&sk->sk_udp_daddr_v6[0]);
				ksm->dstport = ntohs(sk->sk_udp_dport);
			}
			break;
		default:
			kfree(ksm);
			return;
		}
	} else if (sk->sk_protocol == IPPROTO_SCTP) {
			// Changes to support IPV4, IPV6, IPV4-mapped-IPV6
		switch (sk->sk_family) {
		case AF_INET6:
			sprintf(ipv6_address, "%pI6", (void *)&sk->sk_v6_rcv_saddr);
			if (isIpv6SourceAddressEqualsNull(ipv6_address)) {
				if ((!ipv6_addr_v4mapped(&np->saddr))) {
						/* Handles IPv6 */
					if (isIpv6PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
						sprintf(ksm->srcaddr, "%pI6", (void *)&np->saddr);
						ksm->srcport = ntohs(inet->inet_sport);
						sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_v6_daddr);
						ksm->dstport = ntohs(inet->inet_dport);
					} else {
						sprintf(ksm->srcaddr, "%pI6", (void *)&sk->sk_udp_saddr_v6);
						ksm->srcport = ntohs(sk->sk_udp_sport);
						sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_udp_daddr_v6);
						ksm->dstport = ntohs(sk->sk_udp_dport);
					}
				} else {
						/* Handles IPv4MappedIPv6 */
					if (isIpv4PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
						sprintf(ksm->srcaddr, "%pI4", (void *)&inet->inet_saddr);
						ksm->srcport = ntohs(inet->inet_sport);
						sprintf(ksm->dstaddr, "%pI4", (void *)&inet->inet_daddr);
						ksm->dstport = ntohs(inet->inet_dport);
					} else {
						sprintf(ksm->srcaddr, "%pI4", (void *)&sk->sk_udp_saddr_v6[0]);
						ksm->srcport = ntohs(sk->sk_udp_sport);
						sprintf(ksm->dstaddr, "%pI4", (void *)&sk->sk_udp_daddr_v6[0]);
						ksm->dstport = ntohs(sk->sk_udp_dport);
					}
				}
			} else {
					/* Handles IPv6 */
				if ((!ipv6_addr_v4mapped(&sk->sk_v6_rcv_saddr))) {
					if (isIpv6PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
						sprintf(ksm->srcaddr, "%pI6", (void *)&sk->sk_v6_rcv_saddr);
						ksm->srcport = ntohs(inet->inet_sport);
						sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_v6_daddr);
						ksm->dstport = ntohs(inet->inet_dport);
					} else {
						sprintf(ksm->srcaddr, "%pI6", (void *)&sk->sk_udp_saddr_v6);
						ksm->srcport = ntohs(sk->sk_udp_sport);
						sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_udp_daddr_v6);
						ksm->dstport = ntohs(sk->sk_udp_dport);
					}
				} else {
						/* Handles IPv4MappedIPv6 */
					if (isIpv4PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
						sprintf(ksm->srcaddr, "%pI4", (void *)&inet->inet_saddr);
						ksm->srcport = ntohs(inet->inet_sport);
						sprintf(ksm->dstaddr, "%pI4", (void *)&inet->inet_daddr);
						ksm->dstport = ntohs(inet->inet_dport);
					} else {
						sprintf(ksm->srcaddr, "%pI4", (void *)&sk->sk_udp_saddr_v6[0]);
						ksm->srcport = ntohs(sk->sk_udp_sport);
						sprintf(ksm->dstaddr, "%pI4", (void *)&sk->sk_udp_daddr_v6[0]);
						ksm->dstport = ntohs(sk->sk_udp_dport);
					}
				}
			}
			break;
		case AF_INET:
				/* Handles IPv4 */
			if (isIpv4PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
				sprintf(ksm->srcaddr, "%pI4", (void *)&inet->inet_saddr);
				ksm->srcport = ntohs(inet->inet_sport);

				sprintf(ksm->dstaddr, "%pI4", (void *)&inet->inet_daddr);
				ksm->dstport = ntohs(inet->inet_dport);
			} else {
				sprintf(ksm->srcaddr, "%pI4", (void *)&sk->sk_udp_saddr_v6[0]);
				ksm->srcport = ntohs(sk->sk_udp_sport);

				sprintf(ksm->dstaddr, "%pI4", (void *)&sk->sk_udp_daddr_v6[0]);
				ksm->dstport = ntohs(sk->sk_udp_dport);
			}
			break;
		default:
			kfree(ksm);
			return;
		}
	} else {
			// Changes to support IPV4, IPV6, IPV4-mapped-IPV6;
		switch (sk->sk_family) {
		case AF_INET6:
			sprintf(ipv6_address, "%pI6", (void *)&sk->sk_v6_rcv_saddr);
			if (isIpv6SourceAddressEqualsNull(ipv6_address)) {
				if ((!ipv6_addr_v4mapped(&np->saddr))) {
					if (isIpv6PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
						sprintf(ksm->srcaddr, "%pI6", (void *)&np->saddr);
						sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_v6_daddr);
					} else {
						sprintf(ksm->srcaddr, "%pI6", (void *)&sk->sk_udp_saddr_v6);
						sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_udp_daddr_v6);
					}
				} else {
					if (isIpv4PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
						sprintf(ksm->srcaddr, "%pI4", (void *)&inet->inet_saddr);
						sprintf(ksm->dstaddr, "%pI4", (void *)&inet->inet_daddr);
					} else {
						sprintf(ksm->srcaddr, "%pI4", (void *)&sk->sk_udp_saddr_v6[0]);
						sprintf(ksm->dstaddr, "%pI4", (void *)&sk->sk_udp_daddr_v6[0]);
					}
				}
			} else {
				if ((!ipv6_addr_v4mapped(&sk->sk_v6_rcv_saddr))) {
					if (isIpv6PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
						sprintf(ksm->srcaddr, "%pI6", (void *)&sk->sk_v6_rcv_saddr);
						sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_v6_daddr);
					} else {
						sprintf(ksm->srcaddr, "%pI6", (void *)&sk->sk_udp_saddr_v6);
						sprintf(ksm->dstaddr, "%pI6", (void *)&sk->sk_udp_daddr_v6);
					}
				} else {
					if (isIpv4PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
						sprintf(ksm->srcaddr, "%pI4", (void *)&inet->inet_saddr);
						sprintf(ksm->dstaddr, "%pI4", (void *)&inet->inet_daddr);
					} else {
						sprintf(ksm->srcaddr, "%pI4", (void *)&sk->sk_udp_saddr_v6[0]);
						sprintf(ksm->dstaddr, "%pI4", (void *)&sk->sk_udp_daddr_v6[0]);
					}
				}
			}
			break;
		case AF_INET:
				/* Handles IPv4 */
			if (isIpv4PostRoutingAddressEqualsNull(sk->sk_udp_daddr_v6, sk->sk_udp_saddr_v6)) {
				sprintf(ksm->srcaddr, "%pI4", (void *)&inet->inet_saddr);
				sprintf(ksm->dstaddr, "%pI4", (void *)&inet->inet_daddr);
			} else {
				sprintf(ksm->srcaddr, "%pI4", (void *)&sk->sk_udp_saddr_v6[0]);
				sprintf(ksm->dstaddr, "%pI4", (void *)&sk->sk_udp_daddr_v6[0]);
			}
			break;
		default:
			kfree(ksm);
			return;
		}
	}

		// Do not record packets which does not have valid ip addresses associated;
	if (isIpv4AddressEqualsNull(ksm->srcaddr, ksm->dstaddr) || isIpv6AddressEqualsNull(ksm->srcaddr, ksm->dstaddr)) {
		kfree(ksm);
		return;
	}

	ksm->knox_sent = sock->knox_sent;
	ksm->knox_recv = sock->knox_recv;
		// To record the uid of DNS request packet and other packets
	ksm->knox_uid = current->cred->uid.val;
	if (ksm->dstport == 53 && ksm->knox_uid > 0) {
		ksm->knox_uid_dns = ksm->knox_uid;
	} else if (ksm->dstport == 53 && ksm->knox_uid == 0 && sk->knox_dns_uid >= 0) {
		ksm->knox_uid_dns = sk->knox_dns_uid;
	} else {
		ksm->knox_uid_dns = 0;
	}

	ksm->knox_pid = current->tgid;
	ksm->trans_proto = sk->sk_protocol;

	memcpy(ksm->domain_name, sk->domain_name, sizeof(ksm->domain_name)-1);

	ksm->open_time = sock->open_time;

	close_timespec = current_kernel_time();
	ksm->close_time = close_timespec.tv_sec;

	insert_data_kfifo_kthread(ksm);
}
EXPORT_SYMBOL(knox_collect_socket_data);

/* The function opens the char device through which the userspace reads the socket meta-data information */
static int ncm_open(struct inode *inode, struct file *file) {
    NCM_LOGD("ncm_open is being called. \n");

    if(!is_system_server()) {
        NCM_LOGE("ncm_open failed:Caller is a non system process with uid %u \n",(current_uid().val));
        return -EACCES;
    }

    if (((file->f_flags & O_ACCMODE) == O_WRONLY) || ((file->f_flags & O_ACCMODE) == O_RDWR)) {
        NCM_LOGE("ncm_open failed:Trying to open in write mode \n");
        return -EACCES;
    }

    if (device_open_count) {
        NCM_LOGE("ncm_open failed:The device is already in open state \n");
        return -EBUSY;
    }

    device_open_count++;

    try_module_get(THIS_MODULE);

    return SUCCESS;
}

#ifdef CONFIG_64BIT
static ssize_t ncm_copy_data_user_64(char __user *buf, size_t count)
{
		struct knox_socket_metadata kcm = {0};
		struct knox_user_socket_metadata user_copy = {0};

    unsigned long copied;
    int read = 0;

    if(mutex_lock_interruptible(&ncm_lock)) {
        NCM_LOGE("ncm_copy_data_user failed:Signal interuption \n");
        return 0;
    }
    read = kfifo_out(&knox_sock_info, &kcm,1);
    mutex_unlock(&ncm_lock);
    if(read == 0) {
        return 0;
    }

		user_copy.srcport = kcm.srcport;
		user_copy.dstport = kcm.dstport;
		user_copy.trans_proto = kcm.trans_proto;
		user_copy.knox_sent = kcm.knox_sent;
		user_copy.knox_recv = kcm.knox_recv;
		user_copy.knox_uid = kcm.knox_uid;
		user_copy.knox_pid = kcm.knox_pid;
		user_copy.knox_puid = kcm.knox_puid;
		user_copy.open_time = kcm.open_time;
		user_copy.close_time = kcm.close_time;
		user_copy.knox_uid_dns = kcm.knox_uid_dns;
		user_copy.knox_ppid = kcm.knox_ppid;

    memcpy(user_copy.srcaddr,kcm.srcaddr,sizeof(user_copy.srcaddr));
    memcpy(user_copy.dstaddr,kcm.dstaddr,sizeof(user_copy.dstaddr));

    memcpy(user_copy.process_name,kcm.process_name,sizeof(user_copy.process_name));
    memcpy(user_copy.parent_process_name,kcm.parent_process_name,sizeof(user_copy.parent_process_name));

    memcpy(user_copy.domain_name,kcm.domain_name,sizeof(user_copy.domain_name)-1);

    copied = copy_to_user(buf,&user_copy,sizeof(struct knox_user_socket_metadata));
    return count;
}
#else
static ssize_t ncm_copy_data_user(char __user *buf, size_t count)
{
		struct knox_socket_metadata *kcm = NULL;
		struct knox_user_socket_metadata user_copy = {0};

		unsigned long copied;
		int read = 0;

		if (mutex_lock_interruptible(&ncm_lock)) {
			NCM_LOGE("ncm_copy_data_user failed:Signal interuption \n");
			return 0;
		}

		kcm = kzalloc(sizeof (struct knox_socket_metadata), GFP_KERNEL);
		if (kcm == NULL) {
			mutex_unlock(&ncm_lock);
			return 0;
		}

		read = kfifo_out(&knox_sock_info, kcm, 1);
		mutex_unlock(&ncm_lock);
		if (read == 0) {
			kfree(kcm);
			return 0;
		}

		user_copy.srcport = kcm->srcport;
		user_copy.dstport = kcm->dstport;
		user_copy.trans_proto = kcm->trans_proto;
		user_copy.knox_sent = kcm->knox_sent;
		user_copy.knox_recv = kcm->knox_recv;
		user_copy.knox_uid = kcm->knox_uid;
		user_copy.knox_pid = kcm->knox_pid;
		user_copy.knox_puid = kcm->knox_puid;
		user_copy.open_time = kcm->open_time;
		user_copy.close_time = kcm->close_time;
		user_copy.knox_uid_dns = kcm->knox_uid_dns;
		user_copy.knox_ppid = kcm->knox_ppid;

		memcpy(user_copy.srcaddr, kcm->srcaddr, sizeof(user_copy.srcaddr));
		memcpy(user_copy.dstaddr, kcm->dstaddr, sizeof(user_copy.dstaddr));

		memcpy(user_copy.process_name, kcm->process_name, sizeof(user_copy.process_name));
		memcpy(user_copy.parent_process_name, kcm->parent_process_name, sizeof(user_copy.parent_process_name));

		memcpy(user_copy.domain_name, kcm->domain_name, sizeof(user_copy.domain_name)-1);

		copied = copy_to_user(buf, &user_copy, sizeof(struct knox_user_socket_metadata));

		kfree(kcm);

		return count;
}
#endif

/* The function writes the socket meta-data to the user-space */
static ssize_t ncm_read(struct file *file, char __user *buf, size_t count, loff_t *off)
{
	if (!is_system_server()) {
		NCM_LOGE("ncm_read failed:Caller is a non system process with uid %u \n", (current_uid().val));
		return -EACCES;
	}

	#ifdef CONFIG_64BIT
		return ncm_copy_data_user_64(buf, count);
	#else
		return ncm_copy_data_user(buf, count);
	#endif

	return 0;
}

/* The function closes the char device */
static int ncm_close(struct inode *inode, struct file *file) {
    NCM_LOGD("ncm_close is being called \n");
    if(!is_system_server()) {
        NCM_LOGE("ncm_close failed:Caller is a non system process with uid %u \n",(current_uid().val));
        return -EACCES;
    }
    device_open_count--;
    module_put(THIS_MODULE);
    if(!check_ncm_flag())  {
        NCM_LOGD("ncm_close success: The device was already in closed state \n");
        return SUCCESS;
    }
    update_ncm_flag(ncm_deactivated_flag);
    free_kfifo();
    unregisterNetFilterHooks();
    return SUCCESS;
}



/* The function sets the flag which indicates whether the ncm feature needs to be enabled or disabled */
static long ncm_ioctl_evt(struct file *file, unsigned int cmd, unsigned long arg) {
    if(!is_system_server()) {
        NCM_LOGE("ncm_ioctl_evt failed:Caller is a non system process with uid %u \n",(current_uid().val));
        return -EACCES;
    }
    switch(cmd) {
        case NCM_ACTIVATED: {
            NCM_LOGD("ncm_ioctl_evt is being NCM_ACTIVATED with the ioctl command %u \n",cmd);
            if(check_ncm_flag()) return SUCCESS;
            registerNetfilterHooks();
            initialize_kfifo();
            update_ncm_flag(ncm_activated_flag);
            break;
        }
        case NCM_DEACTIVATED: {
            NCM_LOGD("ncm_ioctl_evt is being NCM_DEACTIVATED with the ioctl command %u \n",cmd);
            if(!check_ncm_flag()) return SUCCESS;
            update_ncm_flag(ncm_deactivated_flag);
            free_kfifo();
            unregisterNetFilterHooks();
            break;
        }
        default:
            break;
    }
    return SUCCESS;
}

static unsigned int ncm_poll(struct file *file, poll_table *pt) {
    int mask = 0;
    int ret = 0;
    if (kfifo_is_empty(&knox_sock_info)) {
        ret = wait_event_interruptible_timeout(ncm_wq,!kfifo_is_empty(&knox_sock_info), msecs_to_jiffies(WAIT_TIMEOUT));
        switch(ret) {
            case -ERESTARTSYS:
                mask = -EINTR;
                break;
            case 0:
                mask = 0;
                break;
            case 1:
                mask |= POLLIN | POLLRDNORM;
                break;
            default:
                mask |= POLLIN | POLLRDNORM;
                break;
        }
        return mask;
    } else {
        mask |= POLLIN | POLLRDNORM;
    }
    return mask;
}

static const struct file_operations ncm_fops = {
    .owner          = THIS_MODULE,
    .open           = ncm_open,
    .read           = ncm_read,
    .release        = ncm_close,
    .unlocked_ioctl = ncm_ioctl_evt,
    .compat_ioctl   = ncm_ioctl_evt,
    .poll           = ncm_poll,
};

struct miscdevice ncm_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "ncm_dev",
    .fops = &ncm_fops,
};

static int __init ncm_init(void) {
    int ret;
    ret = misc_register(&ncm_misc_device);
    if (unlikely(ret)) {
        NCM_LOGE("failed to register ncm misc device!\n");
        return ret;
    }
    NCM_LOGD("Network Context Metadata Module: initialized\n");
    return SUCCESS;
}

static void __exit ncm_exit(void) {
    misc_deregister(&ncm_misc_device);
    NCM_LOGD("Network Context Metadata Module: unloaded\n");
}

module_init(ncm_init)
module_exit(ncm_exit)

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Network Context Metadata Module:");

/* END_OF_KNOX_NPA */
