#ifndef _MUIC_I2C_
#define _MUIC_I2C_

extern int muic_i2c_read_byte(const struct i2c_client *client, u8 command);
extern int muic_i2c_write_byte(const struct i2c_client *client,
			u8 command, u8 value);
extern int muic_i2c_guaranteed_wbyte(const struct i2c_client *client,
			u8 command, u8 value);
#endif
