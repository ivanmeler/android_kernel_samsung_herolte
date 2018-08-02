#ifndef __SAMSUNG_AUD_EFFWORKER_HEADER__
#define __SAMSUNG_AUD_EFFWORKER_HEADER__ __FILE__

enum exynos_effwork_cmd {
	LPEFF_EFFECT_CMD = 2,
	LPEFF_REVERB_CMD,
	LPEFF_DEBUG_CMD,
	LPEFF_EXIT_CMD,
};

int lpeff_init(struct seiren_info *si);
void queue_lpeff_cmd(enum exynos_effwork_cmd msg);
void exynos_init_lpeffworker(void);
void exynos_lpeff_finalize(void);
void lpeff_set_effect_addr(void __iomem *effect_ram);

#endif /* __SAMSUNG_AUD_EFFWORKER_HEADER__ */
