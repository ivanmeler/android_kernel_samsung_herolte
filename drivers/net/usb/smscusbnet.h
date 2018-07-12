/*
 * USB Networking Link Interface
 *
 * Copyright (C) 2000-2005 by David Brownell <dbrownell@users.sourceforge.net>
 * Copyright (C) 2003-2005 David Hollis <dhollis@davehollis.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef	__SMSCUSBNET_H
#define	__SMSCUSBNET_H

#ifndef DEBUG
#define DEBUG
#endif

typedef unsigned char BOOLEAN;
#define TRUE	((BOOLEAN)1)
#define FALSE	((BOOLEAN)0)

#define HS_USB_PKT_SIZE     512
#define FS_USB_PKT_SIZE     64

#define MAX_SINGLE_PACKET_SIZE  2048
#define DEFAULT_HS_BURST_CAP_SIZE (16 *1024 + 5 * HS_USB_PKT_SIZE)
#define DEFAULT_FS_BURST_CAP_SIZE (6 *1024 + 33 * FS_USB_PKT_SIZE)
#define DEFAULT_TX_BUFFER_SIZE (7 * 1024)
#define RX_SKB_COPY /* must be defined when rx_use_prealloc_buffers */

#define EXTRA_HEADER_LEN	8
#define VLAN_DUMMY		0xFFFF

enum{
	RX_FIXUP_VALID_SKB,
	RX_FIXUP_INVALID_SKB,
	RX_FIXUP_ERROR,
};

struct ExtraErrorCounter
{
    u32 tx_epipe;
    u32 tx_eproto;
    u32 tx_etimeout;
    u32 tx_eilseq;
    u32 tx_eoverflow;

    u32 rx_epipe;
    u32 rx_eproto;
    u32 rx_etimeout;
    u32 rx_eilseq;
    u32 rx_eoverflow;

};

#define AUTOSUSPEND_DYNAMIC 	0x01  //Suspend on s0, work for lan9500 and lan9500a
#define AUTOSUSPEND_DYNAMIC_S3 	0x02  //Suspend on s3, only for lan9500a
#define AUTOSUSPEND_LINKDOWN 	0x04  //Suspend when link is down
#define AUTOSUSPEND_INTFDOWN 	0x08 //Suspend when interface is down
#define AUTOSUSPEND_DETACH	 	0x10 //Enable net detach

enum{
	FEATURE_SUSPEND3,
	FEATURE_SMARTDETACH,
	FEATURE_NEWSTATIS_CNT,
	FEATURE_SEP_LEDS,
	FEATURE_WUFF_8,
	FEATURE_MDIX_MODE,
	FEATURE_EEE,
	FEATURE_MAX_NO
};

enum{
	EVENT_TX_HALT,
	EVENT_RX_HALT,
	EVENT_RX_MEMORY,
	EVENT_STS_SPLIT,
	EVENT_LINK_RESET,
	EVENT_SET_MULTICAST,
	EVENT_HAS_FRAME,
	EVENT_DEV_RECOVERY,
	EVENT_IDLE_CHECK,
	EVENT_IDLE_RESUME,
	EVENT_LINK_DOWN_DETACH,
	EVENT_LINK_UP,
	EVENT_LINK_DOWN,
};

/* interface from usbnet core to each USB networking link we handle */
struct usbnet {
	/* housekeeping */
	struct usb_device	*udev;
	struct usb_interface *uintf;
	struct driver_info	*driver_info;
	wait_queue_head_t	*wait;
	int StopSummitUrb;

	/* i/o info: pipes etc */
	unsigned		in, out;
	struct usb_host_endpoint *status;
	unsigned		maxpacket;
	struct timer_list	delay;
	struct timer_list    LinkPollingTimer;
	BOOLEAN StopLinkPolling;
	u16	idProduct;		//Product id from device descriptor
	u16 idVendor;		//Vendor id from device descriptor
	u32 linkDownSuspend;	//1. auto-negotiation complete; 2. energy detection;
	u32 dynamicSuspend;  //Enable dynamic suspend when there are no traffic
	u32 netDetach;  //Enable net detach, only for LAN9500A

	/* protocol/interface state */
	struct net_device	*net;
	struct net_device_stats	stats;
	struct ExtraErrorCounter extra_error_cnts;
	int			msg_enable;
	unsigned long		data [5];
	u32			xid;
	u32			hard_mtu;	/* count any extra framing */
	size_t		        rx_urb_size;    /* size for rx urbs  */
	size_t		        tx_urb_size;    /* size for tx urbs, for bundling */
	struct mii_if_info	mii;

	/* various kinds of pending driver work */
	struct sk_buff_head	rxq;
	struct sk_buff_head	txq;
	struct sk_buff_head	tx_pending_q;
	struct sk_buff_head	done;

	struct urb	*tx_urb;

	struct urb		*interrupt;
	void *interrupt_urb_buffer;
	struct tasklet_struct	bh;

	struct work_struct	kevent;
	struct work_struct	myevent;
	struct workqueue_struct  * MyWorkQueue;
	unsigned long		flags;
	BOOLEAN intr_urb_delay_submit;

	int idleCount;
	int linkcheck;
	BOOLEAN pmLock;
	struct semaphore pm_mutex;
	int suspendFlag;	//Flag indicates link down suspend and select suspend
	BOOLEAN netDetachDone;
	u32 chipID;
	u32 preRxFifoDroppedFrame; //Previous counter value

	int chipDependFeatures[FEATURE_MAX_NO]; //Flag for chip-depend feratures
	struct vlan_group	*vlgrp;  //vlan support

	int turbo_mode;
	int tx_hold_on_completion;
	unsigned long tx_qlen;
	unsigned long rx_qlen;
	struct sk_buff_head rx_pool_queue;
	int rx_use_prealloc_buffs;
	struct sk_buff_head tx_pool_queue;
	int tx_use_prealloc_buffs;
};
#define PM_IDLE_DELAY   3 //Time before auto-suspend
enum{
	WAKEPHY_OFF,
	WAKEPHY_NEGO_COMPLETE,
	WAKEPHY_ENERGY
};

static inline struct usb_driver *driver_of(struct usb_interface *intf)
{
	return to_usb_driver(intf->dev.driver);
}
/* interface from the device/framing level "minidriver" to core */
struct driver_info {
	char		*description;

	int		flags;
/* framing is CDC Ethernet, not writing ZLPs (hw issues), or optionally: */
#define FLAG_FRAMING_NC	0x0001		/* guard against device dropouts */
#define FLAG_FRAMING_GL	0x0002		/* genelink batches packets */
#define FLAG_FRAMING_Z	0x0004		/* zaurus adds a trailer */
#define FLAG_FRAMING_RN	0x0008		/* RNDIS batches, plus huge header */

#define FLAG_NO_SETINT	0x0010		/* device can't set_interface() */
#define FLAG_ETHER	0x0020		/* maybe use "eth%d" names */

#define FLAG_FRAMING_AX 0x0040          /* AX88772/178 packets */

	/* init device ... can sleep, or cause probe() failure */
	int	(*bind)(struct usbnet *, struct usb_interface *);

	/* cleanup device ... can sleep, but can't fail */
	void	(*unbind)(struct usbnet *, struct usb_interface *);

	/* reset device ... can sleep */
	int	(*reset)(struct usbnet *);

	/* see if peer is connected ... can sleep */
	int	(*check_connect)(struct usbnet *);

	/* for status polling */
	void	(*status)(struct usbnet *, struct urb *);
	/* Set max frame size*/
	int	(*set_max_frame_size)(struct usbnet *, int size);
	/* link reset handling, called from defer_kevent */
	int	(*link_reset)(struct usbnet *);

	/* fixup rx packet (strip framing) */
	int	(*rx_fixup)(struct usbnet *dev, struct sk_buff *skb);

	/*set multicast list */
	int  (*rx_setmulticastlist) (struct usbnet *dev);

	/* fixup tx packet (add framing) */
	struct sk_buff	*(*tx_fixup)(struct usbnet *dev,
				struct sk_buff *skb, int flags);

	/* for new devices, use the descriptor-reading code instead */
	int		in;		/* rx endpoint */
	int		out;		/* tx endpoint */

	unsigned long	data;		/* Misc driver specific data */

};

/* Minidrivers are just drivers using the "usbnet" core as a powerful
 * network-specific subroutine library ... that happens to do pretty
 * much everything except custom framing and chip-specific stuff.
 */
extern int smscusbnet_probe(struct usb_interface *, const struct usb_device_id *);
extern void smscusbnet_disconnect(struct usb_interface *);
extern void smscusbnet_linkpolling(unsigned long ptr);

/* Drivers that reuse some of the standard USB CDC infrastructure
 * (notably, using multiple interfaces according to the the CDC
 * union descriptor) get some helper code.
 */
struct cdc_state {
	struct usb_cdc_header_desc	*header;
	struct usb_cdc_union_desc	*u;
	struct usb_cdc_ether_desc	*ether;
	struct usb_interface		*control;
	struct usb_interface		*data;
};

extern int usbnet_generic_cdc_bind (struct usbnet *, struct usb_interface *);
extern void usbnet_cdc_unbind (struct usbnet *, struct usb_interface *);

/* CDC and RNDIS support the same host-chosen packet filters for IN transfers */
#define	DEFAULT_FILTER	(USB_CDC_PACKET_TYPE_BROADCAST \
 			|USB_CDC_PACKET_TYPE_ALL_MULTICAST \
 			|USB_CDC_PACKET_TYPE_PROMISCUOUS \
 			|USB_CDC_PACKET_TYPE_DIRECTED)


/* we record the state for each of our queued skbs */
enum skb_state {
	illegal = 0,
	tx_start, tx_done,
	rx_start, rx_done, rx_cleanup
};

struct skb_data {	/* skb->cb is one of these */
	struct urb		*urb;
	struct usbnet		*dev;
	enum skb_state		state;
	size_t			length;
	size_t			pkt_cnt;
};

extern int smscusbnet_IsOperationalMode(struct usbnet *);
extern int smscusbnet_get_endpoints(struct usbnet *, struct usb_interface *);
extern void smscusbnet_defer_kevent (struct usbnet *, int);
extern void smscusbnet_defer_myevent (struct usbnet *, int);
extern void smscusbnet_skb_return (struct usbnet *, struct sk_buff *);
extern int summit_IntrUrb (struct usbnet *dev);

extern u32 smscusbnet_get_msglevel (struct net_device *);
extern void smscusbnet_set_msglevel (struct net_device *, u32);
extern void smscusbnet_get_drvinfo (struct net_device *, struct ethtool_drvinfo *);
extern int smscusbnet_FreeQueue (struct usbnet		*dev);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
extern int smscusbnet_stop (struct net_device *net);
extern struct net_device_stats *smscusbnet_get_stats (struct net_device *net);
extern int smscusbnet_open (struct net_device *net);
extern int smscusbnet_start_xmit (struct sk_buff *skb, struct net_device *net);
extern int smscusbnet_change_mtu (struct net_device *net, int new_mtu);
extern void smscusbnet_tx_timeout (struct net_device *net);
#endif
/* messaging support includes the interface name, so it must not be
 * used before it has one ... notably, in minidriver bind() calls.
 */
#ifdef DEBUG
#define devdbg(usbnet, fmt, arg...) \
	printk(KERN_DEBUG "%s: " fmt "\n" , (usbnet)->net->name , ## arg)
#else
#define devdbg(usbnet, fmt, arg...) do {} while(0)
#endif

#define deverr(usbnet, fmt, arg...) \
	printk(KERN_ERR "%s: " fmt "\n" , (usbnet)->net->name , ## arg)
#define devwarn(usbnet, fmt, arg...) \
	printk(KERN_WARNING "%s: " fmt "\n" , (usbnet)->net->name , ## arg)

#define devinfo(usbnet, fmt, arg...) \
	printk(KERN_INFO "%s: " fmt "\n" , (usbnet)->net->name , ## arg); \


#endif	/* __smscusbnet_H */
