#ifndef _XT_ONESHOT_MATCH_H
#define _XT_ONESHOT_MATCH_H

#include <linux/types.h>
#include <linux/spinlock.h>

#define RULE_STANDBY_UID "fw_standby_uid"

#define ONESHOT_UID_FIND_NONE 0
#define ONESHOT_UID_FIND_UIDCHAIN 1
#define ONESHOT_UID_FINE_END 2

struct xt_oneshot_match_info {
	__u8 match;
};

struct oneshot_uid {
	int map_error;
	rwlock_t lock;
	struct list_head map_list;
};

struct oneshot_uid_map {
	u64 user_id;
	u64 *map;
	struct list_head list;
};

extern struct oneshot_uid oneshot_uid_ipv4;
extern struct oneshot_uid oneshot_uid_ipv6;
extern int oneshot_uid_checkmap(struct oneshot_uid *oneshot_uid_net,
				const struct sk_buff *skb,
				struct xt_action_param *par);
extern void oneshot_uid_resetmap(struct oneshot_uid *oneshot_uid_net);
extern int oneshot_uid_addrule_to_map(struct oneshot_uid *oneshot_uid_net,
				       const void *data);
extern void oneshot_uid_cleanup_unusedmem(struct oneshot_uid *oneshot_uid_net);

#endif /* XT_ONESHOT_MATCH_H */

