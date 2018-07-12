#ifndef _MUIC_DT_
#define _MUIC_DT_

//extern struct of_device_id muic_i2c_dt_ids[];

extern int of_update_supported_list(struct i2c_client *i2c,
				struct muic_platform_data *pdata);
extern int of_muic_dt(struct i2c_client *i2c, struct muic_platform_data *pdata,  muic_data_t *pmuic);
extern int of_muic_hv_dt(muic_data_t *pmuic);
extern int muic_set_gpio_uart_sel(int uart_sel);
#endif
