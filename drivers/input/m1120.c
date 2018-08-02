/*
 *  m1120.c - Linux kernel modules for hall switch 
 *
 *  Copyright (C) 2013 Seunghwan Park <seunghwan.park@magnachip.com>
 *  Copyright (C) 2014 MagnaChip Semiconductor.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/pm.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/m1120.h>
#include <linux/hall.h>
#include <linux/notifier.h>

extern struct blocking_notifier_head hall_ic_notifier_list;
static m1120_data_t *p_m1120_data;
#define M1120_I2C_BUF_SIZE					(17)

/* ********************************************************* */
/* customer config */ 
/* ********************************************************* */
#define M1120_DBG_ENABLE					// for debugging
#define M1120_DETECTION_MODE				M1120_DETECTION_MODE_INTERRUPT
//#define M1120_DETECTION_MODE				M1120_DETECTION_MODE_POLLING
#define M1120_INTERRUPT_TYPE				M1120_VAL_INTSRS_INTTYPE_BESIDE
#define M1120_SENSITIVITY_TYPE				M1120_VAL_INTSRS_SRS_10BIT_0_009mT
#define M1120_PERSISTENCE_COUNT				M1120_VAL_PERSINT_COUNT(5)
#define M1120_OPERATION_FREQUENCY			M1120_VAL_OPF_FREQ_10HZ
#define M1120_OPERATION_RESOLUTION			M1120_VAL_OPF_BIT_10
#define M1120_RESULT_STATUS_FOLD			(0x01)	// result status folding
#define M1120_RESULT_STATUS_SPRD			(0x02)	// result status spread
#define M1120_RESULT_STATUS_BACK			(0x03)	// result status back folding
#define M1120_FOLD_CLOSE				1
#define M1120_FOLD_OPEN					0
#define M1120_BACK_CLOSE				1
#define M1120_BACK_OPEN					0

#define dbg(fmt, args...)  printk("%s: " fmt "\n", __func__, ##args)
#define dbgn(fmt, args...)  printk(fmt, ##args)
#define mxerr(pdev, fmt, args...)			\
	dev_err(pdev, "%s: " fmt "\n", __func__, ##args) 
#define mxinfo(pdev, fmt, args...)			\
	dev_info(pdev, "%s: " fmt "\n", __func__, ##args) 

static int m1120_i2c_read(struct i2c_client* client, u8 reg, u8* rdata, u8 len)
{
	int rc;
	int retry = 0;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = rdata,
		},
	};

	if ( client == NULL ) {
		mxerr(&client->dev, "client is NULL");
		return -ENODEV;
	}

	rc = i2c_transfer(client->adapter, msg, 2);
	while (rc < 0) {
		if (retry++ > 10)
			break;
		mxerr(&client->dev, "i2c_transfer was failed(%d)", rc);
		msleep(100);
		rc = i2c_transfer(client->adapter, msg, 2);
	}

	return 0;
}

static int	m1120_i2c_get_reg(struct i2c_client *client, u8 reg, u8* rdata)
{
	return m1120_i2c_read(client, reg, rdata, 1);
}

static int m1120_i2c_write(struct i2c_client* client, u8 reg, u8* wdata, u8 len)
{
	m1120_data_t *p_data = i2c_get_clientdata(client);
	u8  buf[M1120_I2C_BUF_SIZE];
	int rc;
	int i;
	int retry = 0;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = len+1,
			.buf = buf,
		},
	};

	if ( client == NULL ) {
		printk("[ERROR] %s : i2c client is NULL.\n", __func__);
		return -ENODEV;
	}
	
	buf[0] = reg;
	if (len > M1120_I2C_BUF_SIZE) {
		mxerr(&client->dev, "i2c buffer size must be less than %d", M1120_I2C_BUF_SIZE);
		return -EIO;
	}
	for( i=0 ; i<len; i++ ) buf[i+1] = wdata[i];

	rc = i2c_transfer(client->adapter, msg, 1);
	while (rc < 0) {
		if (retry++ > 10)
			break;
		mxerr(&client->dev, "i2c_transfer was failed (%d)", rc);
		msleep(100);
		rc = i2c_transfer(client->adapter, msg, 1);
	}

	if(len==1) {
		switch(reg){
		case M1120_REG_PERSINT:
			p_data->reg.map.persint = wdata[0];
			break;
		case M1120_REG_INTSRS:
			p_data->reg.map.intsrs = wdata[0];
			break;
		case M1120_REG_LTHL:
			p_data->reg.map.lthl = wdata[0];
			break;
		case M1120_REG_LTHH:
			p_data->reg.map.lthh = wdata[0];
			break;
		case M1120_REG_HTHL:
			p_data->reg.map.hthl = wdata[0];
			break;
		case M1120_REG_HTHH:
			p_data->reg.map.hthh = wdata[0];
			break;
		case M1120_REG_I2CDIS:
			p_data->reg.map.i2cdis = wdata[0];
			break;
		case M1120_REG_SRST:
			p_data->reg.map.srst = wdata[0];
			msleep(1);
			break;
		case M1120_REG_OPF:
			p_data->reg.map.opf = wdata[0];
			break;
		}
	}

	return 0;
}

static int m1120_i2c_set_reg(struct i2c_client *client, u8 reg, u8 wdata)
{
	return m1120_i2c_write(client, reg, &wdata, sizeof(wdata));
}

static int m1120_clear_interrupt(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	m1120_data_t *p_data = i2c_get_clientdata(client);
	int ret = 0;

	ret = m1120_i2c_set_reg(p_data->client, M1120_REG_PERSINT, p_data->reg.map.persint | 0x01);

	return ret;
}

void m1120_convdata_short_to_2byte(u8 opf, short x, unsigned char *hbyte, unsigned char *lbyte)
{
	if( (opf & M1120_VAL_OPF_BIT_8) == M1120_VAL_OPF_BIT_8) {
		/* 8 bit resolution */
		if(x<-128) x=-128;
		else if(x>127) x=127;

		if(x>=0) {
			*lbyte = x & 0x7F;
		} else {
			*lbyte = ( (0x80 - (x*(-1))) & 0x7F ) | 0x80;
		}
		*hbyte = 0x00;
	} else {
		/* 10 bit resolution */
		if(x<-512) x=-512;
		else if(x>511) x=511;

		if(x>=0) {
			*lbyte = x & 0xFF;
			*hbyte = (((x&0x100)>>8)&0x01) << 6;
		} else {
			*lbyte = (0x0200 - (x*(-1))) & 0xFF;
			*hbyte = ((((0x0200 - (x*(-1))) & 0x100)>>8)<<6) | 0x80;
		}
	}
}

short m1120_convdata_2byte_to_short(u8 opf, unsigned char hbyte, unsigned char lbyte)
{
	short x;

	if( (opf & M1120_VAL_OPF_BIT_8) == M1120_VAL_OPF_BIT_8) {
		/* 8 bit resolution */
		x = lbyte & 0x7F;
		if(lbyte & 0x80) {
			x -= 0x80;
		}
	} else {
		/* 10 bit resolution */
		x = ( ( (hbyte & 0x40) >> 6) << 8 ) | lbyte;
		if(hbyte&0x80) {
			x -= 0x200;
		}
	}

	return x;
}

static int m1120_update_interrupt_threshold(struct device *dev, short raw)
{
	struct i2c_client *client = to_i2c_client(dev);
	m1120_data_t *p_data = i2c_get_clientdata(client);
	u8 lthh=0, lthl=0, hthh=0, hthl=0;
	int err = -1;

	if(p_data->reg.map.intsrs & M1120_DETECTION_MODE_INTERRUPT) {

		if (p_data->last_data == M1120_RESULT_STATUS_BACK) {
			m1120_convdata_short_to_2byte(p_data->reg.map.opf, p_data->thrlow_on, &hthh, &hthl);
			m1120_convdata_short_to_2byte(p_data->reg.map.opf, -512, &lthh, &lthl);
		} else if (p_data->last_data == M1120_RESULT_STATUS_SPRD) {
			m1120_convdata_short_to_2byte(p_data->reg.map.opf, p_data->thrhigh_off, &hthh, &hthl);
			m1120_convdata_short_to_2byte(p_data->reg.map.opf, p_data->thrlow_off, &lthh, &lthl);
		} else if (p_data->last_data == M1120_RESULT_STATUS_FOLD) {
			m1120_convdata_short_to_2byte(p_data->reg.map.opf, 511, &hthh, &hthl);
			m1120_convdata_short_to_2byte(p_data->reg.map.opf, p_data->thrhigh_on, &lthh, &lthl);
		}

		err = m1120_i2c_set_reg(p_data->client, M1120_REG_HTHH, hthh);
		if(err) return err;
		err = m1120_i2c_set_reg(p_data->client, M1120_REG_HTHL, hthl);
		if(err) return err;
		err = m1120_i2c_set_reg(p_data->client, M1120_REG_LTHH, lthh);
		if(err) return err;
		err = m1120_i2c_set_reg(p_data->client, M1120_REG_LTHL, lthl);
		if(err) return err;

		pr_info("%s: threshold(0x%02X%02X, 0x%02X%02X)\n", 
				__func__, hthh, hthl, lthh, lthl);

		err = m1120_clear_interrupt(dev);
		if(err) return err;
	}

	return err;
}

static int m1120_measure(m1120_data_t *p_data, short *raw)
{
	struct i2c_client *client = p_data->client;
	int err;
	u8 buf[3];
	int st1_is_ok = 0;

	// read data
	err = m1120_i2c_read(client, M1120_REG_ST1, buf, sizeof(buf));
	if(err) return err;

	// collect data
	if(p_data->reg.map.intsrs & M1120_VAL_INTSRS_INT_ON) {
		// check st1 at interrupt mode
		if( ! (buf[0] & 0x10) ) {
			st1_is_ok = 1;
		}
	} else {
		// check st1 at polling mode
		if(buf[0] & 0x01) {
			st1_is_ok = 1;
		}
	}

	if(st1_is_ok) {
		*raw = m1120_convdata_2byte_to_short(p_data->reg.map.opf, buf[2], buf[1]);
	} else {
		mxerr(&client->dev, "st1(0x%02X) is not DRDY", buf[0]);
		err = -1;
	}

	pr_info("%s: raw data (%d)\n", __func__, *raw);

	return err;
}


static int m1120_get_result_status(m1120_data_t* p_data, int raw)
{
	int status = p_data->last_data;

	switch (p_data->last_data) {
		case M1120_RESULT_STATUS_FOLD:
			if (p_data->thrlow_off > raw)
				status = M1120_RESULT_STATUS_BACK;
			else if (p_data->thrhigh_on > raw)
				status = M1120_RESULT_STATUS_SPRD;
			break;

		case M1120_RESULT_STATUS_SPRD:
			if (p_data->thrhigh_off < raw)
				status = M1120_RESULT_STATUS_FOLD;
			else if (p_data->thrlow_off > raw)
				status = M1120_RESULT_STATUS_BACK;
			break;

		case M1120_RESULT_STATUS_BACK:
			if (p_data->thrhigh_off < raw)
				status = M1120_RESULT_STATUS_FOLD;
			else if (p_data->thrlow_on < raw)
				status = M1120_RESULT_STATUS_SPRD;
			break;
	}

	switch (status) {
		case M1120_RESULT_STATUS_FOLD:
			pr_info("%s: Result is status [High]: Fold\n", __func__);
			break;
		case M1120_RESULT_STATUS_SPRD:
			pr_info("%s: Result is status [Mid]: Spread\n", __func__);
			break;
		case M1120_RESULT_STATUS_BACK:
			pr_info("%s: Result is status [Low]: Back fold\n", __func__);
			break;
	}

	return status;
}

static void m1120_irq_handle(m1120_data_t* p_data)
{
	short raw = 0;
	int err = 0;
	int prev_data = p_data->last_data;
	int new_data;

	err = m1120_measure(p_data, &raw);

	if(err) {
		pr_info("%s: cannot get raw data\n", __func__);
		return;
	}

	new_data = m1120_get_result_status(p_data, raw);

	if (new_data != prev_data) {
		if (prev_data == M1120_RESULT_STATUS_FOLD) {
			input_report_switch(p_data->input_fold, p_data->event_val_fold, M1120_FOLD_OPEN);
			input_sync(p_data->input_fold);
			blocking_notifier_call_chain(&hall_ic_notifier_list, M1120_FOLD_OPEN, NULL);
		}
		else if (prev_data == M1120_RESULT_STATUS_BACK) {
			input_report_switch(p_data->input_back, p_data->event_val_back, M1120_BACK_OPEN);
			input_sync(p_data->input_back);
		}

		if (new_data == M1120_RESULT_STATUS_FOLD) {
			input_report_switch(p_data->input_fold, p_data->event_val_fold, M1120_FOLD_CLOSE);
			input_sync(p_data->input_fold);
			blocking_notifier_call_chain(&hall_ic_notifier_list, M1120_FOLD_CLOSE, NULL);
		}
		else if (new_data == M1120_RESULT_STATUS_BACK) {
			input_report_switch(p_data->input_back, p_data->event_val_back, M1120_BACK_CLOSE);
			input_sync(p_data->input_back);
		}
	}

	p_data->last_data = new_data;

	m1120_update_interrupt_threshold(&p_data->client->dev, raw);
}

static irqreturn_t m1120_irq_handler(int irq, void *dev_id)
{
	m1120_data_t *p_data = dev_id;
	pr_info("%s\n", __func__);
	if(p_m1120_data != NULL) {
		m1120_irq_handle(p_data);
	}
	return IRQ_HANDLED;
}

static int m1120_set_operation_mode(struct device *dev, int mode)
{
	struct i2c_client *client = to_i2c_client(dev);
	m1120_data_t *p_data = i2c_get_clientdata(client);
	u8 opf = p_data->reg.map.opf;
	int err = -1;

	switch(mode) {
		case OPERATION_MODE_POWERDOWN:
			if(p_data->irq_enabled) {
				/* disable irq */
				disable_irq(p_data->irq);
				free_irq(p_data->irq, NULL);
				p_data->irq_enabled = 0;
			}
			opf &= (0xFF - M1120_VAL_OPF_HSSON_ON);
			err = m1120_i2c_set_reg(client, M1120_REG_OPF, opf);
			mxinfo(&client->dev, "operation mode was chnaged to OPERATION_MODE_POWERDOWN");
			break;
		case OPERATION_MODE_MEASUREMENT:
			opf &= (0xFF - M1120_VAL_OPF_EFRD_ON);
			opf |= M1120_VAL_OPF_HSSON_ON;
			err = m1120_i2c_set_reg(client, M1120_REG_OPF, opf);
			if(p_data->reg.map.intsrs & M1120_DETECTION_MODE_INTERRUPT) {
				if(!p_data->irq_enabled) {
					/* enable irq */
					err = request_threaded_irq(p_data->irq, NULL, m1120_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, M1120_IRQ_NAME, p_data);
					if(err) {
						mxerr(dev, "request_irq was failed");
						return err;
					}
					mxinfo(dev, "request_irq was success");
					enable_irq(p_data->irq);
					p_data->irq_enabled = 1;
					p_data->last_data = M1120_RESULT_STATUS_SPRD;
				}
			}
			mxinfo(&client->dev, "operation mode was chnaged to OPERATION_MODE_MEASUREMENT");
			break;
		case OPERATION_MODE_FUSEROMACCESS:
			opf |= M1120_VAL_OPF_EFRD_ON;
			opf |= M1120_VAL_OPF_HSSON_ON;
			err = m1120_i2c_set_reg(client, M1120_REG_OPF, opf);
			mxinfo(&client->dev, "operation mode was chnaged to OPERATION_MODE_FUSEROMACCESS");
			break;
	}

	return err;
}

static int m1120_set_detection_mode(struct device *dev, u8 mode)
{
	struct i2c_client *client = to_i2c_client(dev);
	m1120_data_t *p_data = i2c_get_clientdata(client);
	u8 data;
	int err = 0;

	if(mode & M1120_DETECTION_MODE_INTERRUPT) {
		/* config threshold */
		m1120_update_interrupt_threshold(dev, p_data->last_data);

		/* write intsrs */
		data = p_data->reg.map.intsrs | M1120_DETECTION_MODE_INTERRUPT;
		err = m1120_i2c_set_reg(p_data->client, M1120_REG_INTSRS, data);
		if(err) return err;

	} else {
		/* write intsrs */
		data = p_data->reg.map.intsrs & (0xFF - M1120_DETECTION_MODE_INTERRUPT);
		err = m1120_i2c_set_reg(p_data->client, M1120_REG_INTSRS, data);
		if(err) return err;
	}

	return err;
}

static int m1120_get_delay(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	m1120_data_t *p_data = i2c_get_clientdata(client);

	int delay = 0;

	delay = atomic_read(&p_data->atm.delay);

	return delay;
}

static int m1120_get_enable(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	m1120_data_t *p_data = i2c_get_clientdata(client);

	return atomic_read(&p_data->atm.enable);
}

static void m1120_set_delay(struct device *dev, int delay)
{
	struct i2c_client *client = to_i2c_client(dev);
	m1120_data_t *p_data = i2c_get_clientdata(client);

	if(delay<M1120_DELAY_MIN) delay = M1120_DELAY_MIN;
	atomic_set(&p_data->atm.delay, delay);

	mutex_lock(&p_data->mtx.enable);

	if (m1120_get_enable(dev)) {
		if( ! (p_data->reg.map.intsrs & M1120_DETECTION_MODE_INTERRUPT)) {
			cancel_delayed_work_sync(&p_data->work);
			schedule_delayed_work(&p_data->work, msecs_to_jiffies(delay));
		}
	}

	mutex_unlock(&p_data->mtx.enable);
}

static void m1120_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	m1120_data_t *p_data = i2c_get_clientdata(client);
	int delay = m1120_get_delay(dev);

	mutex_lock(&p_data->mtx.enable);

	if (enable) {                   /* enable if state will be changed */
		if (!atomic_cmpxchg(&p_data->atm.enable, 0, 1)) {
			m1120_set_detection_mode(dev, p_data->reg.map.intsrs & M1120_DETECTION_MODE_INTERRUPT);
			m1120_set_operation_mode(&p_m1120_data->client->dev, OPERATION_MODE_MEASUREMENT);
			if( ! (p_data->reg.map.intsrs & M1120_DETECTION_MODE_INTERRUPT)) {
				schedule_delayed_work(&p_data->work, msecs_to_jiffies(delay));
			}
		}
	} else {                        /* disable if state will be changed */
		if (atomic_cmpxchg(&p_data->atm.enable, 1, 0)) {
			cancel_delayed_work_sync(&p_data->work);
			m1120_set_operation_mode(&p_m1120_data->client->dev, OPERATION_MODE_POWERDOWN);
		}
	}
	atomic_set(&p_data->atm.enable, enable);

	mutex_unlock(&p_data->mtx.enable);
}

static int m1120_reset_device(struct device *dev)
{
	int	err = 0;
	u8	id = 0xFF, data = 0x00;

	struct i2c_client *client = to_i2c_client(dev);
	m1120_data_t *p_data = i2c_get_clientdata(client);

	if( (p_data == NULL) || (p_data->client == NULL) ) return -ENODEV;

	/* sw reset */
	err = m1120_i2c_set_reg(p_data->client, M1120_REG_SRST, M1120_VAL_SRST_RESET);
	if(err) {
		mxerr(&client->dev, "sw-reset was failed(%d)", err);
		return err;
	}
	msleep(5); // wait 5ms

	/* check id */
	err = m1120_i2c_get_reg(p_data->client, M1120_REG_DID, &id);
	if (err < 0) return err;
	if (id != M1120_VAL_DID) {
		dev_err(&client->dev, "current device id(0x%02X) is not M1120 device id(0x%02X)\n", id, M1120_VAL_DID);
		return -ENXIO;
	}

	/* init variables */
	data = M1120_PERSISTENCE_COUNT;
	err = m1120_i2c_set_reg(p_data->client, M1120_REG_PERSINT, data);

	data = M1120_DETECTION_MODE | M1120_SENSITIVITY_TYPE;
	if(data & M1120_DETECTION_MODE_INTERRUPT) {
		data |= M1120_INTERRUPT_TYPE;
	}
	err = m1120_i2c_set_reg(p_data->client, M1120_REG_INTSRS, data);

	data = M1120_OPERATION_FREQUENCY | M1120_OPERATION_RESOLUTION;
	err = m1120_i2c_set_reg(p_data->client, M1120_REG_OPF, data);

	/* write variable to register */
	err = m1120_set_detection_mode(dev, M1120_DETECTION_MODE);
	if(err) {
		dev_err(&client->dev, "m1120_set_detection_mode was failed(%d)\n", err);
		return err;
	}

	/* set power-down mode */
	err = m1120_set_operation_mode(dev, OPERATION_MODE_POWERDOWN);
	if(err) {
		dev_err(&client->dev, "m1120_set_detection_mode was failed(%d)\n", err);
		return err;
	}

	return err;
}

static int m1120_init_device(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	m1120_data_t *p_data = i2c_get_clientdata(client);
	int err = -1;

	/* init variables */
	atomic_set(&p_data->atm.enable, 0);
	atomic_set(&p_data->atm.delay, M1120_DELAY_MIN);
#ifdef M1120_DBG_ENABLE
	atomic_set(&p_data->atm.debug, 1);
#else
	atomic_set(&p_data->atm.debug, 0);
#endif
	p_data->calibrated_data = 0;
	p_data->last_data = 0;
	p_data->irq_enabled = 0;
	p_data->irq_first = 1;
	m1120_set_delay(&client->dev, M1120_DELAY_MAX);

	/* reset registers */
	err = m1120_reset_device(dev);
	if(err) {
		dev_err(&client->dev, "m1120_reset_device was failed (%d)\n", err);
		return err;
	}

	return 0;
}

static int m1120_input_dev_init(m1120_data_t *p_data)
{
	int err;

	/* fold event */
	p_data->input_fold = input_allocate_device();
	if (!p_data->input_fold) {
		return -ENOMEM;
	}
	p_data->input_fold->name = "m1120_fold";
	p_data->event_val_fold = SW_FOLDING;
	input_set_drvdata(p_data->input_fold, p_data);
	input_set_capability(p_data->input_fold, EV_SW, p_data->event_val_fold);

	err = input_register_device(p_data->input_fold);
	if (err < 0) {
		input_free_device(p_data->input_fold);
		return err;
	}
	
	/* back event */
	p_data->input_back = input_allocate_device();
	if (!p_data->input_back) {
		return -ENOMEM;
	}
	p_data->input_back->name = "m1120_back";
	p_data->event_val_back = SW_BACKFOLDING;
	input_set_drvdata(p_data->input_back, p_data);
	input_set_capability(p_data->input_back, EV_SW, p_data->event_val_back);

	err = input_register_device(p_data->input_back);
	if (err < 0) {
		input_free_device(p_data->input_back);
		return err;
	}

	return 0;
}

static void m1120_input_dev_terminate(m1120_data_t *p_data)
{
	input_unregister_device(p_data->input_fold);
	input_free_device(p_data->input_fold);

	input_unregister_device(p_data->input_back);
	input_free_device(p_data->input_back);
}

static void m1120_work_func(struct work_struct *work)
{
	m1120_data_t* p_data = container_of((struct delayed_work *)work, m1120_data_t, work);
	unsigned long delay = msecs_to_jiffies(m1120_get_delay(&p_data->client->dev));
	short raw = 0;
	int err = 0;
	int prev_data = p_data->last_data;
	int new_data;

	err = m1120_measure(p_data, &raw);

	if(err) {
		pr_info("%s: cannot get raw data\n", __func__);
		return;
	}
		
	new_data = m1120_get_result_status(p_data, raw);

	if (new_data != prev_data) {
		if (prev_data == M1120_RESULT_STATUS_FOLD) {
			input_report_switch(p_data->input_fold, p_data->event_val_fold, M1120_FOLD_OPEN);
			input_sync(p_data->input_fold);
			blocking_notifier_call_chain(&hall_ic_notifier_list, M1120_FOLD_OPEN, NULL);
		}
		else if (prev_data == M1120_RESULT_STATUS_BACK) {
			input_report_switch(p_data->input_back, p_data->event_val_back, M1120_BACK_OPEN);
			input_sync(p_data->input_back);
		}

		if (new_data == M1120_RESULT_STATUS_FOLD) {
			input_report_switch(p_data->input_fold, p_data->event_val_fold, M1120_FOLD_CLOSE);
			input_sync(p_data->input_fold);
			blocking_notifier_call_chain(&hall_ic_notifier_list, M1120_FOLD_CLOSE, NULL);
		}
		else if (new_data == M1120_RESULT_STATUS_BACK) {
			input_report_switch(p_data->input_back, p_data->event_val_back, M1120_BACK_CLOSE);
			input_sync(p_data->input_back);
		}
	}
		
	p_data->last_data = new_data;

	schedule_delayed_work(&p_data->work, delay);
}

static ssize_t m1120_thrlow_on_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", p_m1120_data->thrlow_on);
}

static ssize_t m1120_thrlow_on_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	s16 thr;

	ret = kstrtos16(buf, 0, &thr);
	if (ret != 0) {
		dev_err(dev, "%s: fail to get value\n", __func__);
		return count;
	}
	p_m1120_data->thrlow_on = thr;

	return count;
}

static ssize_t m1120_thrlow_off_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", p_m1120_data->thrlow_off);
}

static ssize_t m1120_thrlow_off_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	s16 thr;

	ret = kstrtos16(buf, 0, &thr);
	if (ret != 0) {
		dev_err(dev, "%s: fail to get value\n", __func__);
		return count;
	}
	p_m1120_data->thrlow_off = thr;

	return count;
}

static ssize_t m1120_thrhigh_on_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", p_m1120_data->thrhigh_on);
}

static ssize_t m1120_thrhigh_on_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	s16 thr;

	ret = kstrtos16(buf, 0, &thr);
	if (ret != 0) {
		dev_err(dev, "%s: fail to get value\n", __func__);
		return count;
	}
	p_m1120_data->thrhigh_on = thr;

	return count;
}

static ssize_t m1120_thrhigh_off_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", p_m1120_data->thrhigh_off);
}

static ssize_t m1120_thrhigh_off_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	s16 thr;

	ret = kstrtos16(buf, 0, &thr);
	if (ret != 0) {
		dev_err(dev, "%s: fail to get value\n", __func__);
		return count;
	}
	p_m1120_data->thrhigh_off = thr;

	return count;
}

static DEVICE_ATTR(hall_thrlow_off, S_IRUGO|S_IWUSR|S_IWGRP, m1120_thrlow_off_show, m1120_thrlow_off_store);
static DEVICE_ATTR(hall_thrlow_on, S_IRUGO|S_IWUSR|S_IWGRP, m1120_thrlow_on_show, m1120_thrlow_on_store);
static DEVICE_ATTR(hall_thrhigh_off, S_IRUGO|S_IWUSR|S_IWGRP, m1120_thrhigh_off_show, m1120_thrhigh_off_store);
static DEVICE_ATTR(hall_thrhigh_on, S_IRUGO|S_IWUSR|S_IWGRP, m1120_thrhigh_on_show, m1120_thrhigh_on_store);

static struct attribute *m1120_attributes[] = {
	&dev_attr_hall_thrlow_off.attr,
	&dev_attr_hall_thrlow_on.attr,
	&dev_attr_hall_thrhigh_off.attr,
	&dev_attr_hall_thrhigh_on.attr,
	NULL
};

static struct attribute_group m1120_attribute_group = {
	.attrs = m1120_attributes
};

#ifdef CONFIG_OF
static int of_m1120_dt(struct device *dev, m1120_data_t *p_data)
{
	struct device_node *np_node = dev->of_node;
	int gpio;
	int err;
	u32 temp;

	if (!np_node) {
		printk("%s : error to get dt node\n", __func__);
		return -EINVAL;
	}

	gpio = of_get_named_gpio(np_node, "m1120,irq-gpio", 0);
	if (gpio < 0) {
		pr_info("%s: fail to get gpio \n", __func__ );
		return -EINVAL;
	}
	p_data->igpio = gpio;

	gpio = gpio_to_irq(gpio);
	if (gpio < 0) {
		pr_info("%s: fail to return irq corresponding gpio \n", __func__ );
		return -EINVAL;
	}
	p_data->irq = gpio;

	pr_info("%s: irq-gpio: %d \n", __func__, gpio);

	err = of_property_read_u32(np_node, "m1120,thrhigh_on", &temp);
	if (err)
		pr_info("%s, can't parsing\n", __func__);
	p_data->thrhigh_on = temp;
	pr_info("%s: thrhigh_on = %d\n", __func__, p_data->thrhigh_on);

	err = of_property_read_u32(np_node, "m1120,thrhigh_off", &temp);
	if (err)
		pr_info("%s, can't parsing\n", __func__);
	p_data->thrhigh_off = temp;
	pr_info("%s: thrhigh_off = %d\n", __func__, p_data->thrhigh_off);

	err = of_property_read_u32(np_node, "m1120,thrlow_on", &temp);
	if (err)
		pr_info("%s, can't parsing\n", __func__);
	p_data->thrlow_on = temp;
	pr_info("%s: thrlow_on = %d\n", __func__, p_data->thrlow_on);

	err = of_property_read_u32(np_node, "m1120,thrlow_off", &temp);
	if (err)
		pr_info("%s, can't parsing\n", __func__);
	p_data->thrlow_off = temp;
	pr_info("%s: thrlow_off = %d\n", __func__, p_data->thrlow_off);

	return 0;
}
#endif

int m1120_i2c_drv_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	m1120_data_t *p_data;
	int err = 0;
	int wakeup = 0;

	/* (1) allocation memory for p_m1120_data */
	p_data = kzalloc(sizeof(m1120_data_t), GFP_KERNEL);
	if (!p_data) {
		dev_err(&client->dev, "kernel memory alocation was failed\n");
		err = -ENOMEM;
		goto error_0;
	}

	/* (2) init mutex variable */
	mutex_init(&p_data->mtx.enable);
	mutex_init(&p_data->mtx.data);

	/* (3) config i2c client */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c_check_functionality was failed\n");
		err = -ENODEV;
		goto error_1;
	}
	i2c_set_clientdata(client, p_data);
	p_data->client = client;

#ifdef CONFIG_OF
	/* (4) get platform data */
	if(client->dev.of_node) {
		err = of_m1120_dt(&client->dev, p_data);
		if (err < 0) {
			dev_err(&client->dev, "fail to get the dt\n");
			goto error_1;
		}
	}
#endif

	/* (5) setup interrupt gpio */
	if(p_data->igpio != -1) {
		err = gpio_request(p_data->igpio, "m1120_irq");
		if (err){
			dev_err(&client->dev, "gpio_request was failed(%d)\n", err); 
			goto error_1;
		}
		pr_info("%s: gpio_request was success\n", __func__);
		err = gpio_direction_input(p_data->igpio);
		if (err < 0) {
			dev_err(&client->dev, "gpio_direction_input was failed(%d)\n", err);
			goto error_2;
		}
		pr_info("%s: gpio_direction_input was success\n", __func__);
	}
	

	/* (6) reset and init device */
	err = m1120_init_device(&p_data->client->dev);
	if(err) {
		dev_err(&client->dev, "m1120_init_device was failed(%d)\n", err);
		goto error_1;
	}

	/* (7) config work function */
	INIT_DELAYED_WORK(&p_data->work, m1120_work_func);

	/* (8) init input device */
	err = m1120_input_dev_init(p_data);
	if(err) {
		dev_err(&client->dev, "m1120_input_dev_init was failed(%d)\n", err);
		goto error_1;
	}

	/* (9) create sysfs group */
	err = sysfs_create_group(&sec_key->kobj, &m1120_attribute_group);
	if(err) {
		dev_err(&client->dev, "sysfs_create_group was failed(%d)\n", err);
		goto error_3;
	}

	device_init_wakeup(&client->dev, wakeup);
	enable_irq_wake(p_data->irq);

	/* (10) imigrate p_data to p_m1120_data */
	p_m1120_data = p_data;
	m1120_set_enable(&p_m1120_data->client->dev, 1);

	pr_info("%s: %s was probed.\n", __func__, M1120_DRIVER_NAME);

	return 0;

error_3:
	m1120_input_dev_terminate(p_data);

error_2:
	if(p_data->igpio != -1) {
		gpio_free(p_data->igpio);
	}
error_1:
	kfree(p_data);

error_0:

	return err;
}

static int m1120_i2c_drv_remove(struct i2c_client *client)
{
	m1120_data_t *p_data = i2c_get_clientdata(client);

	m1120_set_enable(&client->dev, 0);
	sysfs_remove_group(&sec_key->kobj, &m1120_attribute_group);
	m1120_input_dev_terminate(p_data);
	if(p_data->igpio!= -1) {
		gpio_free(p_data->igpio);
	}
	kfree(p_data);

	return 0;
}

static int m1120_i2c_drv_suspend(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	m1120_data_t *p_data = i2c_get_clientdata(client);

	pr_info("%s: status = %s\n", __func__, p_data->last_data==1?"folded":
			(p_data->last_data==2?"spread":"back"));

	return 0;
}

static int m1120_i2c_drv_resume(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	m1120_data_t *p_data = i2c_get_clientdata(client);

	pr_info("%s: status = %s\n", __func__, p_data->last_data==1?"folded":
			(p_data->last_data==2?"spread":"back"));

	return 0;
}

static const struct i2c_device_id m1120_i2c_drv_id_table[] = {
	{M1120_DRIVER_NAME, 0 },
	{ }
};

#if defined(CONFIG_OF)
static struct of_device_id m1120_dt_ids[] = {
	{ .compatible = "m1120" },
	{ },
};
MODULE_DEVICE_TABLE(of, m1120_dt_ids);
#endif /* CONFIG_OF */

static SIMPLE_DEV_PM_OPS(m1120_pm_ops, m1120_i2c_drv_suspend, m1120_i2c_drv_resume);

static struct i2c_driver m1120_driver = {
	.driver = {
		.name	= M1120_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.pm	= &m1120_pm_ops,
#if defined(CONFIG_OF)
		.of_match_table	= m1120_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= m1120_i2c_drv_probe,
	.remove		= m1120_i2c_drv_remove,
	.id_table	= m1120_i2c_drv_id_table,
};

static int __init m1120_driver_init(void)
{
	printk(KERN_INFO "%s\n", __func__);
	return i2c_add_driver(&m1120_driver);
}
late_initcall(m1120_driver_init);

static void __exit m1120_driver_exit(void)
{
	printk(KERN_INFO "%s\n", __func__);
	i2c_del_driver(&m1120_driver);
}
module_exit(m1120_driver_exit);

MODULE_AUTHOR("shpark <seunghwan.park@magnachip.com>");
MODULE_VERSION(M1120_DRIVER_VERSION);
MODULE_DESCRIPTION("M1120 hallswitch driver");
MODULE_LICENSE("GPL");

