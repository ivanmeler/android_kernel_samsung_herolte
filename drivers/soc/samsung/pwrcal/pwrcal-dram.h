#ifndef __PWRCAL_DRAM_H__
#define __PWRCAL_DRAM_H__

struct cal_dram_ops {
	void (*print_dram_info)(void);
	int (*get_dram_tdqsck)(int ch, int rank, int byte, unsigned int *tdqsck);
	int (*get_dram_tdqs2dq)(int ch, int rank, int idx, unsigned int *tdqs2dq);
};

extern struct cal_dram_ops cal_dram_ops;

#endif
