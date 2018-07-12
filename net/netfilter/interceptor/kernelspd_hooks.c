/**
   @copyright
   Copyright (c) 2013 - 2015, INSIDE Secure Oy. All rights reserved.
*/

#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/udp.h>
#include <linux/string.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif

#include "kernelspd_internal.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0)
#define HOOK_TYPE unsigned int
#define HOOK_NUM(__hook) (__hook)
#else
#define HOOK_TYPE const struct nf_hook_ops *
#define HOOK_NUM(__hook) (__hook)->hooknum
#endif

#define HOOK_NAME(hook)                         \
    (hook == NF_INET_LOCAL_IN ? "IN " :         \
     (hook == NF_INET_LOCAL_OUT ? "OUT" :       \
      (hook == NF_INET_FORWARD ? "FWD" :        \
       "???")))

#define RESULT_NAME(result)                                     \
    (result == KERNEL_SPD_DISCARD ? "DISCARD" :                 \
     (result == KERNEL_SPD_BYPASS ? "BYPASS" : "??????"))

#define HOOK_DEBUG(level, result, spdfields_p, fields_p)        \
    DEBUG_## level(                                             \
            hook,                                               \
            "%s: %s %s: %d %s->%s: %s",                         \
            RESULT_NAME(result),                                \
            HOOK_NAME(hook),                                    \
            (spdfields_p)->spd_name,                            \
            __kuid_val((spdfields_p)->packet_kuid),             \
            (spdfields_p)->in_name,                             \
            (spdfields_p)->out_name,                            \
            debug_str_ip_selector_fields(                       \
                    DEBUG_STRBUF_GET(),                         \
                    fields_p,                                   \
                    sizeof *fields_p))

struct KernelSpdFields
{
    int hook;
    kuid_t packet_kuid;
    int spd_id;
    const char *spd_name;
    const char *in_name;
    const char *out_name;
};

static void
fill_selector_fields_ports(
        struct IPSelectorFields *fields,
        struct udphdr *udph,
        bool non_initial_fragment)
{
    if (udph == NULL || non_initial_fragment == true)
    {
        fields->source_port = IP_SELECTOR_PORT_OPAQUE;
        fields->destination_port = IP_SELECTOR_PORT_OPAQUE;
    }
    else if (fields->ip_protocol == 1 ||
             fields->ip_protocol == 58)
    {
        /* ICMP Type and Code are located in the same place as udp
           source port would. */
        fields->destination_port = ntohs(udph->source);

        /* ICMP only used "destination port" in selectors. */
        fields->source_port = IP_SELECTOR_PORT_NONE;
    }
    else
    {
        fields->source_port = ntohs(udph->source);
        fields->destination_port = ntohs(udph->dest);
    }
}


static void
fill_spd_fields(
        struct KernelSpdFields *spdfields,
        int hook,
        kuid_t packet_kuid,
        const struct net_device *in,
        const struct net_device *out)
{
    bool out_protected = true;
    bool in_protected = true;

    spdfields->hook = hook;
    spdfields->packet_kuid = packet_kuid;

    if (in != NULL)
    {
        spdfields->in_name = in->name;
    }

    if (out != NULL)
    {
        spdfields->out_name = out->name;
    }

    if (hook == NF_INET_LOCAL_IN)
    {
        spdfields->out_name = "Local";
    }

    if (hook == NF_INET_LOCAL_OUT)
    {
        spdfields->in_name = "Local";
    }

    if (out != NULL)
    {
        out_protected =
            ipsec_boundary_is_protected_interface(
                    ipsec_boundary,
                    spdfields->out_name);
    }

    if (in != NULL)
    {
        in_protected =
            ipsec_boundary_is_protected_interface(
                    ipsec_boundary,
                    spdfields->in_name);
    }

    if (in_protected && !out_protected)
    {
        spdfields->spd_name = "SPD-O";
        spdfields->spd_id = KERNEL_SPD_OUT;
    }
    else if (!in_protected && out_protected)
    {
        spdfields->spd_name = "SPD-I";
        spdfields->spd_id = KERNEL_SPD_IN;
    }
    else if (!in_protected && !out_protected)
    {
        spdfields->spd_name = "UNP";
        spdfields->spd_id = -1;
    }
    else
    {
        spdfields->spd_name = "PRO";
        spdfields->spd_id = -1;
    }
}

static int
spd_lookup(
        struct KernelSpdFields *spdfields,
        const struct IPSelectorFields *fields,
        bool non_initial_fragment)
{
    int result;

    if (spdfields->spd_id < 0)
    {
        result = KERNEL_SPD_BYPASS;
    }
    else if (spdfields->hook == NF_INET_FORWARD)
    {
        result = KERNEL_SPD_DISCARD;
    }
    else if (non_initial_fragment)
    {
        spdfields->spd_name = "NIF";
        spdfields->spd_id = -1;

        result = KERNEL_SPD_BYPASS;
    }
    else
    {
        read_lock(&spd_lock);
        result = ip_selector_db_lookup(&spd, spdfields->spd_id, fields);
        read_unlock(&spd_lock);
    }

    return result;
}

static int
make_spd_lookup(
        int hook,
        kuid_t packet_kuid,
        const struct net_device *in,
        const struct net_device *out,
        struct udphdr *udph,
        struct IPSelectorFields *fields,
        bool non_initial_fragment)
{
    struct KernelSpdFields spdfields = { 0 };
    int result;
    int verdict;

    fill_selector_fields_ports(fields, udph, non_initial_fragment);

    fill_spd_fields(&spdfields, hook, packet_kuid, in, out);

    result = spd_lookup(&spdfields, fields, non_initial_fragment);

    if (result == KERNEL_SPD_DISCARD)
    {
        if (uid_valid(spdfields.packet_kuid) == true)
        {
            if (uid_eq(spdfields.packet_kuid, bypass_kuid) == true)
            {
                spd_proc_new_bypass_packet(fields);

                result = KERNEL_SPD_BYPASS;
            }
        }
    }

    switch (result)
    {
    case KERNEL_SPD_BYPASS:
        verdict = NF_ACCEPT;
        break;

    case KERNEL_SPD_DISCARD:
        verdict = NF_DROP;
        break;

    default:
        DEBUG_FAIL(
                hook,
                "Unknown spd result %d; dropping packet.",
                result);

        HOOK_DEBUG(FAIL, result, &spdfields, fields);
        verdict = NF_DROP;
        break;
    }

    if (verdict == NF_DROP)
    {
        HOOK_DEBUG(HIGH, result, &spdfields, fields);
    }
    else if (spdfields.spd_id >= 0)
    {
        HOOK_DEBUG(MEDIUM, result, &spdfields, fields);
    }
    else
    {
        HOOK_DEBUG(LOW, result, &spdfields, fields);
    }

    return verdict;
}


static unsigned int
hook_ipv4(
        HOOK_TYPE hook,
        struct sk_buff *skb,
        const struct net_device *in,
        const struct net_device *out,
        int (*okfn)(struct sk_buff *))
{
    struct IPSelectorFields fields;
    unsigned int hooknum = HOOK_NUM(hook);

    struct iphdr *iph = (struct iphdr *) skb_network_header(skb);
    struct udphdr *udph;
    uint16_t ports[2];
    bool non_initial_fragment;
    kuid_t packet_kuid = INVALID_UID;

    if (iph != NULL)
    {
        memset(fields.source_address, 0, 12);
        memset(fields.destination_address, 0, 12);
        memcpy(fields.source_address + 12, &iph->saddr, 4);
        memcpy(fields.destination_address + 12, &iph->daddr, 4);
        fields.ip_protocol = iph->protocol;
        fields.ip_version = 4;

        non_initial_fragment = (((ntohs(iph->frag_off)) & 0x1fff) != 0);
        udph = skb_header_pointer(skb, (iph->ihl << 2), 4, ports);

        if (skb->sk != NULL && hooknum == NF_INET_LOCAL_OUT)
        {
            packet_kuid = sock_i_uid(skb->sk);
        }
    }
    else
    {
        DEBUG_FAIL(
                hook,
                "Dropping packet: no network header set for skb in hook %s.",
                HOOK_NAME(hooknum));

        return NF_DROP;
    }

    return
        make_spd_lookup(
                hooknum,
                packet_kuid,
                in,
                out,
                udph,
                &fields,
                non_initial_fragment);
}


static void *
parse_ip6_headers(
        struct sk_buff *skb,
        void *tmpbuf,
        bool *non_initial_fragment_p,
        uint8_t *nexthdr_p)
{
    struct ipv6hdr *iph = (struct ipv6hdr *) skb_network_header(skb);
    int offset = 0;
    int nh = iph->nexthdr;
    int hl = sizeof *iph;
    bool skip;

    struct exthdr
    {
        uint8_t nh;
        uint8_t hl;
    }
    *nhp = NULL;

    *non_initial_fragment_p = false;

    do
    {
        offset += hl;

        nhp = skb_header_pointer(skb, offset, 4, tmpbuf);

        switch (nh)
        {
        case IPPROTO_HOPOPTS:
        case IPPROTO_ROUTING:
        case IPPROTO_DSTOPTS:
            skip = true;
            break;

        case IPPROTO_FRAGMENT:
            if (nhp != NULL)
            {
                uint32_t frag;

                memcpy(&frag, nhp, 4);

                frag = ntohl(frag);

                if ((frag & 0xfff8) != 0)
                {
                    *non_initial_fragment_p = true;
                }
            }
            skip = true;
            break;

        default:
            skip = false;
            break;
        }

        if (skip)
        {
            if (nhp != NULL)
            {
                hl = (nhp->hl + 1) * 8;
                nh = nhp->nh;
            }
            else
            {
                skip = false;
            }
        }
    }
    while (skip);

    *nexthdr_p = nh;

    return nhp;
}


static unsigned int
hook_ipv6(
        HOOK_TYPE hook,
        struct sk_buff *skb,
        const struct net_device *in,
        const struct net_device *out,
        int (*okfn)(struct sk_buff *))
{
    struct IPSelectorFields fields;
    unsigned int hooknum = HOOK_NUM(hook);

    struct ipv6hdr *iph = (struct ipv6hdr *) skb_network_header(skb);
    struct udphdr *udph;
    uint16_t portsbuf[4];
    bool non_initial_fragment;
    kuid_t packet_kuid = INVALID_UID;

    if (iph != NULL)
    {
        uint8_t nexthdr;
        memcpy(fields.source_address, &iph->saddr, 16);
        memcpy(fields.destination_address, &iph->daddr, 16);
        fields.ip_version = 6;

        udph =
            parse_ip6_headers(
                    skb,
                    portsbuf,
                    &non_initial_fragment,
                    &nexthdr);

        fields.ip_protocol = nexthdr;

        if (skb->sk != NULL && hooknum == NF_INET_LOCAL_OUT)
        {
            packet_kuid = sock_i_uid(skb->sk);
        }
    }
    else
    {
        DEBUG_FAIL(
                hook,
                "Dropping packet: no network header set for skb in hook %s.",
                HOOK_NAME(hooknum));

        return NF_DROP;
    }

    return
        make_spd_lookup(
                hooknum,
                packet_kuid,
                in,
                out,
                udph,
                &fields,
                non_initial_fragment);
}

#define HOOK_ENTRY(__hook, __pf, __hooknum)     \
    {                                           \
        .hook = __hook,                         \
        .owner = THIS_MODULE,                   \
        .pf = __pf,                             \
        .hooknum = __hooknum,                   \
        .priority = 1                           \
    }

static struct nf_hook_ops spd_hooks[6] =
{
    HOOK_ENTRY(hook_ipv4, NFPROTO_IPV4, NF_INET_LOCAL_IN),
    HOOK_ENTRY(hook_ipv4, NFPROTO_IPV4, NF_INET_LOCAL_OUT),
    HOOK_ENTRY(hook_ipv4, NFPROTO_IPV4, NF_INET_FORWARD),
    HOOK_ENTRY(hook_ipv6, NFPROTO_IPV6, NF_INET_LOCAL_IN),
    HOOK_ENTRY(hook_ipv6, NFPROTO_IPV6, NF_INET_LOCAL_OUT),
    HOOK_ENTRY(hook_ipv6, NFPROTO_IPV6, NF_INET_FORWARD)
};

static int initialised = 0;

int
spd_hooks_init(
        void)
{
    int result = 0;

    if (initialised == 0)
    {
        result = nf_register_hooks(spd_hooks, 6);
        if (result == 0)
        {
            initialised = 1;
            DEBUG_HIGH(hook, "Kernel spd hooks registered.");
        }
        else
        {
            DEBUG_FAIL(hook, "nf_register_hooks() failed %d.", result);
        }
    }

    return result;
}


void
spd_hooks_uninit(
        void)
{
    if (initialised != 0)
    {
        nf_unregister_hooks(spd_hooks, 6);
        initialised = 0;

        DEBUG_HIGH(hook, "Kernel spd hooks unregistered.");
    }
}
