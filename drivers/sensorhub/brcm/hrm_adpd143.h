#ifndef _HRM_ADPD143_H_
#define _HRM_ADPD143_H_


int adpd_i2c_read(u32 reg, u32 *value, u32 *size);
int adpd_i2c_write(u32 reg, u32 value);
int adpd_init_device(struct i2c_client *client);
int adpd_deinit_device(void);
int adpd_enable(enum hrm_mode mode);
int adpd_disable(enum hrm_mode mode);
int adpd_get_current(u8 *d1, u8 *d2, u8 *d3, u8 *d4);
int adpd_set_current(u8 d1, u8 d2, u8 d3, u8 d4);
int adpd_read_data(struct hrm_output_data *data);
int adpd_get_chipid(u64 *chip_id);
int adpd_get_part_type(u16 *part_type);
int adpd_get_i2c_err_cnt(u32 *err_cnt);
int adpd_set_i2c_err_cnt(void);
int adpd_get_curr_adc(u16 *ir_curr, u16 *red_curr, u32 *ir_adc, u32 *red_adc);
int adpd_set_curr_adc(void);
int adpd_get_name_chipset(char *name);
int adpd_get_name_vendor(char *name);
int adpd_get_threshold(s32 *threshold);
int adpd_set_threshold(s32 threshold);
int adpd_set_eol_enable(u8 enable);
int adpd_get_eol_result(char *result);
int adpd_get_eol_status(u8 *status);
int adpd_debug_set(u8 mode);
int adpd_get_fac_cmd(char *cmd_result);
int adpd_get_version(char *version);
int adpd_get_sensor_info(char *sensor_info_data);

#endif /* _HRM_ADPD143_H_ */
