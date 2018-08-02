#include "pwrcal-env.h"
#include "pwrcal-rae.h"

#ifdef PWRCAL_TARGET_LINUX
#include <linux/io.h>
#endif

DEFINE_SPINLOCK(pwrcal_rae_spinlock);

/* raw register access */
unsigned int pwrcal_readl(void *addr)
{
	unsigned int ret;
	void *ma;

#ifdef PWRCAL_TARGET_LINUX
	ma = v2psfrmap[MAP_IDX(addr)].ma + ((unsigned long)addr & 0xFFFF);
#else
	ma = (void *)((unsigned long)addr & ~(0x80000000));
#endif

	ret = *((volatile unsigned int *)(ma));

	return ret;
}
void pwrcal_writel(void *addr, unsigned int regv)
{
	void *ma;
	unsigned long flag = 0;
#ifdef PWRCAL_TARGET_LINUX
	ma = v2psfrmap[MAP_IDX(addr)].ma + ((unsigned long)addr & 0xFFFF);
	if (addr < spinlock_enable_offset && !((unsigned long)addr & 0x80000000))
#else
	ma = (void *)((unsigned long)addr & ~(0x80000000));
	if (!((unsigned long)addr & 0x80000000))
#endif
		spin_lock_irqsave(&pwrcal_rae_spinlock, flag);

	*((volatile unsigned int *)(ma)) = regv;

#ifdef PWRCAL_TARGET_LINUX
	if (addr < spinlock_enable_offset)
#else
	if (!((unsigned long)addr & 0x80000000))
#endif
		spin_unlock_irqrestore(&pwrcal_rae_spinlock, flag);
}

/* register access operations */
unsigned int pwrcal_getbit(void *addr, unsigned int bitp)
{
	unsigned int mask = 1 << bitp;
	unsigned int regv = pwrcal_readl(addr);
	return (regv & mask) >> bitp;
}
void pwrcal_setbit(void *addr, unsigned int bitp, unsigned int bitv)
{
	void *ma;
	unsigned int mask = 1 << bitp;
	unsigned int regv;
	unsigned long flag = 0;

#ifdef PWRCAL_TARGET_LINUX
	ma = v2psfrmap[MAP_IDX(addr)].ma + ((unsigned long)addr & 0xFFFF);
	if (addr < spinlock_enable_offset && !((unsigned long)addr & 0x80000000))
#else
	ma = (void *)((unsigned long)addr & ~(0x80000000));
	if (!((unsigned long)addr & 0x80000000))
#endif
		spin_lock_irqsave(&pwrcal_rae_spinlock, flag);


	regv = *((volatile unsigned int *)(ma));
	regv = (bitv == 1) ? (regv | mask) : (regv & ~mask);
	*((volatile unsigned int *)(ma)) = regv;

#ifdef PWRCAL_TARGET_LINUX
	if (addr < spinlock_enable_offset)
#else
	if (!((unsigned long)addr & 0x80000000))
#endif
		spin_unlock_irqrestore(&pwrcal_rae_spinlock, flag);
}
unsigned int pwrcal_getf(void *addr,
			unsigned int fieldp,
			unsigned int fieldm)
{
	unsigned int mask = fieldm << fieldp;
	unsigned int regv = pwrcal_readl(addr);
	return	(regv & mask) >> fieldp;
}
void pwrcal_setf(void *addr,
		unsigned int fieldp,
		unsigned int fieldm,
		unsigned int fieldv)
{
	void *ma;
	unsigned int mask = fieldm << fieldp;
	unsigned int regv;
	unsigned long flag = 0;

#ifdef PWRCAL_TARGET_LINUX
	ma = v2psfrmap[MAP_IDX(addr)].ma + ((unsigned long)addr & 0xFFFF);
	if (addr < spinlock_enable_offset && !((unsigned long)addr & 0x80000000))
#else
	ma = (void *)((unsigned long)addr & ~(0x80000000));
	if (!((unsigned long)addr & 0x80000000))
#endif
		spin_lock_irqsave(&pwrcal_rae_spinlock, flag);


	regv = *((volatile unsigned int *)(ma));
	regv = (regv & ~mask) | (fieldv << fieldp);
	*((volatile unsigned int *)(ma)) = regv;

#ifdef PWRCAL_TARGET_LINUX
	if (addr < spinlock_enable_offset)
#else
	if (!((unsigned long)addr & 0x80000000))
#endif
		spin_unlock_irqrestore(&pwrcal_rae_spinlock, flag);
}

void cal_rae_init(void)
{
#ifdef PWRCAL_TARGET_LINUX
	int i;

	for (i = 1; i < num_of_v2psfrmap; i++)
		if (v2psfrmap[i].pa)
			v2psfrmap[i].ma = ioremap(v2psfrmap[i].pa, SZ_64K);
#endif
}
