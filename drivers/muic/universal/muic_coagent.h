#ifndef _MUIC_COAGENT_
#define _MUIC_COAGENT_

/*
  * B[7:0] : cmd
  * B[15:8] : parameter
  *
  */

enum coagent_cmd {
	COA_INIT = 0,
	COA_GAMEPAD_STATUS,
};	

enum coagent_param {
	COA_STATUS_UNKNOWN = 0,	
	COA_STATUS_OK,
	COA_STATUS_NOK,
};	

#define COAGENT_CMD(n) (n & 0xf)
#define COAGENT_PARAM(n) ((n >> 4) & 0xf)

#define COAGENT_CMD_BITS 0
#define COAGENT_PARAM_BITS 4

extern int coagent_in(unsigned int *pbuf);
extern int coagent_out(unsigned int *pbuf);
extern bool coagent_alive(void);
extern void coagent_update_ctx(muic_data_t *pmuic);
#endif
