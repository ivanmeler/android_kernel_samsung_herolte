/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include "et320.h"

int etspi_mass_read(struct etspi_data *etspi, u8 addr, u8 *buf, int buf_size)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status;
	struct spi_device *spi;
	struct spi_message m;
	u8 *write_addr = NULL, *read_data = NULL;
	u32 read_data_size = 0;

	struct spi_transfer t_set_addr = {
		.tx_buf = NULL,
		.len = 2,
	};
	struct spi_transfer t_read_data = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = read_data_size,
	};

	/* Write and read data in one data query section */

	if ((buf_size + SHIFT_BYTE_OF_IMAGE) % DIVISION_OF_IMAGE != 0)
		read_data_size = buf_size + SHIFT_BYTE_OF_IMAGE + (DIVISION_OF_IMAGE - ((buf_size + SHIFT_BYTE_OF_IMAGE) % DIVISION_OF_IMAGE));
	else
		read_data_size = buf_size + SHIFT_BYTE_OF_IMAGE;

	/* Set start address */

	read_data = kzalloc(read_data_size, GFP_KERNEL);

	if (read_data == NULL) return -ENOMEM;

	write_addr = kzalloc(2, GFP_KERNEL);
	write_addr[0] = ET320_WRITE_ADDRESS;
	write_addr[1] = addr;

	t_set_addr.tx_buf = write_addr;
	t_read_data.tx_buf = t_read_data.rx_buf = read_data;
	t_read_data.len = read_data_size;

	pr_debug("%s buf_size = %d, read_data_size = %d\n", __func__,
		buf_size, read_data_size);

	read_data[0] = ET320_READ_DATA;

	spi = etspi->spi;

	spi_message_init(&m);
	spi_message_add_tail(&t_set_addr, &m);
	status = spi_sync(spi, &m);
	spi_message_init(&m);
	spi_message_add_tail(&t_read_data, &m);
	status = spi_sync(spi, &m);

	kfree(write_addr);

	if (status == 0) {
		memcpy(buf, read_data + SHIFT_BYTE_OF_IMAGE, buf_size);
	} else {
		pr_err(KERN_ERR "%s read data error status = %d\n", __func__, status);
	}

	kfree(read_data);

	return status;
#endif
}

/* Read io register */
int etspi_io_read_register(struct etspi_data *etspi, u8 *addr, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status = 0;
	struct spi_device *spi;
	struct spi_message m;
	int read_len = 1;

	u8 write_addr[] = {ET320_WRITE_ADDRESS, 0x00};
	u8 read_value[] = {ET320_READ_DATA, 0x00};
	u8 val, addrval;
	u8 result[] = {0xFF, 0xFF};

	struct spi_transfer t_set_addr = {
		.tx_buf = write_addr,
		.len = 2,
	};
	struct spi_transfer t = {
		.tx_buf = read_value,
		.rx_buf = result,
		.len = 2,
	};

	if (copy_from_user(&addrval, (const u8 __user *) (uintptr_t) addr
		, read_len)) {
		pr_err(KERN_ERR "%s buffer copy_from_user fail", __func__);
		status = -EFAULT;
		return status;
	}

	DEBUG_PRINT("%s read_len = %d", __func__, read_len);

	spi = etspi->spi;

	write_addr[1] = addrval;

	spi_message_init(&m);
	spi_message_add_tail(&t_set_addr, &m);
	status = spi_sync(spi, &m);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	status = spi_sync(spi, &m);
	if (status < 0) {
		pr_err(KERN_ERR "%s read data error status = %d\n"
				, __func__, status);
		return status;
	}

	val = result[1];

#ifdef ET320_SPI_DEBUG
	DEBUG_PRINT("%s address = %x buf = %x", __func__, addr, val);
#endif
	if (copy_to_user((u8 __user *) (uintptr_t) buf, &val, read_len)) {
		pr_err(KERN_ERR "%s buffer copy_to_user fail status", __func__);
		status = -EFAULT;
		return status;
	}

	return status;
#endif
}

/* Write data to register */
int etspi_io_write_register(struct etspi_data *etspi, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status = 0;
	struct spi_device *spi;
	int write_len = 2;
	struct spi_message m;

	u8 write_addr[] = {ET320_WRITE_ADDRESS, 0x00};
	u8 write_value[] = {ET320_WRITE_DATA, 0x00};
	u8 val[2];

	struct spi_transfer t1 = {
		.tx_buf = write_addr,
		.len = 2,
	};
	struct spi_transfer t2 = {
		.tx_buf = write_value,
		.len = 2,
	};

	if (copy_from_user(val, (const u8 __user *) (uintptr_t) buf
		, write_len)) {
		pr_err(KERN_ERR "%s buffer copy_from_user fail", __func__);
		status = -EFAULT;
		return status;
	}

	DEBUG_PRINT("%s write_len = %d", __func__, write_len);

#ifdef ET320_SPI_DEBUG
	DEBUG_PRINT("%s address = %x data = %x", __func__, val[0], val[1]);
#endif

	spi = etspi->spi;

	write_addr[1] = val[0];
	write_value[1] = val[1];

	spi_message_init(&m);
	spi_message_add_tail(&t1, &m);
	status = spi_sync(spi, &m);
	spi_message_init(&m);
	spi_message_add_tail(&t2, &m);
	status = spi_sync(spi, &m);
	if (status < 0) {
		pr_err(KERN_ERR "%s read data error status = %d",
				__func__, status);
		return status;
	}

	return status;
#endif
}

int etspi_read_register(struct etspi_data *etspi, u8 addr, u8 *buf)
{
	int status;
	struct spi_device *spi;
	struct spi_message m;

	u8 write_addr[] = {ET320_WRITE_ADDRESS, addr};
	u8 read_value[] = {ET320_READ_DATA, 0x00};
	u8 result[] = {0xFF, 0xFF};

	struct spi_transfer t1 = {
		.tx_buf = write_addr,
		.len = 2,
	};
	struct spi_transfer t2 = {
		.tx_buf = read_value,
		.rx_buf	= result,
		.len = 2,
	};

	spi = etspi->spi;
	spi_message_init(&m);
	spi_message_add_tail(&t1, &m);
	status = spi_sync(spi, &m);
	spi_message_init(&m);
	spi_message_add_tail(&t2, &m);
	status = spi_sync(spi, &m);

	if (status == 0) {
		*buf = result[1];
		DEBUG_PRINT("et320_read_register address = %x result = %x %x\n"
					, addr, result[0], result[1]);
	} else
		pr_err(KERN_ERR "%s read data error status = %d\n"
				, __func__, status);

	return status;
}

int etspi_io_get_one_image(struct etspi_data *etspi, u8 *buf, u8 *image_buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	uint8_t /* read_val */
		*tx_buf = (uint8_t *)buf,
		*work_buf = NULL,
		*val = kzalloc(6, GFP_KERNEL);
	int status;
	uint32_t read_count;

	pr_debug("%s\n", __func__);

	if (val == NULL)
		return -ENOMEM;
	if (copy_from_user(val, (const u8 __user *) (uintptr_t) tx_buf, 6)) {
		pr_err(KERN_ERR "%s buffer copy_from_user fail", __func__);
		status = -EFAULT;
		goto end;
	}
	read_count = val[0] * val[1];          /* total pixel , width * hight */

	work_buf = kzalloc(read_count, GFP_KERNEL);
	if (work_buf == NULL) {
		status = -ENOMEM;
		goto end;
	}
	status = etspi_mass_read(etspi, FDATA_ET320_ADDR, work_buf, read_count);
	if (status < 0) {
		pr_err(KERN_ERR "%s call et320_mass_read error status = %d"
				, __func__, status);
		goto end;
	}
	if (copy_to_user((u8 __user *) (uintptr_t) image_buf,
		work_buf, read_count)) {
		pr_err(KERN_ERR "buffer copy_to_user fail status = %d", status);
		status = -EFAULT;
	}
end:
	kfree(val);
	kfree(work_buf);
	return status;
#endif
}
/*----------------------- EEPROM ------------------------*/

int etspi_eeprom_wren(struct etspi_data *etspi)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status = 0;
	struct spi_device *spi;
	struct spi_message m;

	u8 write_data[] = {FP_EEPROM_WREN_OP};
	struct spi_transfer t = {
		.tx_buf = write_data,
		.len = 1,
	};

	DEBUG_PRINT("%s opcode = %x\n", __func__, write_data[0]);

	spi = etspi->spi;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	status = spi_sync(spi, &m);
	if (status < 0) {
		pr_err("%s spi_sync error status = %d",
				__func__, status);
		return status;
	}

	return status;
#endif
}

int etspi_eeprom_wrdi(struct etspi_data *etspi)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status = 0;
	struct spi_device *spi;
	struct spi_message m;

	u8 write_data[] = {FP_EEPROM_WRDI_OP};
	struct spi_transfer t = {
		.tx_buf = write_data,
		.len = 1,
	};

	DEBUG_PRINT("%s opcode = %x\n", __func__, write_data[0]);

	spi = etspi->spi;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	status = spi_sync(spi, &m);
	if (status < 0) {
		pr_err("%s spi_sync error status = %d",
				__func__, status);
		return status;
	}

	return status;
#endif
}

int etspi_eeprom_rdsr(struct etspi_data *etspi, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status;
	struct spi_device *spi;
	struct spi_message m;
	u8 val,
	   read_value[] = {FP_EEPROM_RDSR_OP, 0x00},
	   result[] = {0xFF, 0xFF};

	struct spi_transfer t = {
		.tx_buf = read_value,
		.rx_buf = result,
		.len = 2,
	};

	spi = etspi->spi;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	status = spi_sync(spi, &m);
	if (status < 0) {
		pr_err("%s spi_sync error status = %d",
				__func__, status);
		return status;
	}

	val = result[1];

	DEBUG_PRINT("%s address = %x buf = %x", __func__,
			FP_EEPROM_RDSR_OP, val);

	if (copy_to_user((u8 __user *) (uintptr_t) buf, &val, 1)) {
		pr_err("%s buffer copy_to_user fail status", __func__);
		status = -EFAULT;
		return status;
	}

	return status;
#endif
}

int etspi_eeprom_wrsr(struct etspi_data *etspi, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status;
	struct spi_device *spi;
	struct spi_message m;
	u8 val;

	u8 write_data[] = {FP_EEPROM_WRSR_OP, 0x00};

	struct spi_transfer t = {
		.tx_buf = write_data,
		.len = 2,
	};

	if (copy_from_user(&val, (const u8 __user *) (uintptr_t) buf
		, 1)) {
		pr_err("%s buffer copy_from_user fail", __func__);
		status = -EFAULT;
		return status;
	}

	DEBUG_PRINT("%s data = %x", __func__, val);

	spi = etspi->spi;

	write_data[1] = val;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	status = spi_sync(spi, &m);
	if (status < 0) {
		pr_err("%s spi_sync error status = %d",
				__func__, status);
		return status;
	}

	return status;
#endif
}

int etspi_eeprom_read(struct etspi_data *etspi, u8 *addr, u8 *buf, int read_len)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status = 0;
	struct spi_device *spi;
	struct spi_message m;
	u8 addrval, *read_value = kzalloc(read_len + 2, GFP_KERNEL);

	struct spi_transfer t = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = read_len + 2,
	};

	if (read_value == NULL)
		return -ENOMEM;

	if (copy_from_user(&addrval, (const u8 __user *) (uintptr_t) addr
		, 1)) {
		pr_err("%s buffer copy_from_user fail", __func__);
		status = -EFAULT;
		goto exit;
	}

	DEBUG_PRINT("%s read_len = %d", __func__, read_len);
	DEBUG_PRINT("%s addrval = %x", __func__, addrval);

	spi = etspi->spi;

	read_value[0] = FP_EEPROM_READ_OP;
	read_value[1] = addrval;

	t.tx_buf = t.rx_buf = read_value;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	status = spi_sync(spi, &m);
	if (status < 0) {
		pr_err("%s spi_sync error status = %d\n"
				, __func__, status);
		goto exit;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) buf,
				read_value + 2, read_len)) {
		pr_err("%s buffer copy_to_user fail status", __func__);
		status = -EFAULT;
		goto exit;
	}

exit:
	kfree(read_value);

	return status;
#endif
}

/*
 * buf - the data wrote to sensor with address info
 * write_len - the length of the data write to memory without address
 */
int etspi_eeprom_write(struct etspi_data *etspi, u8 *buf, int write_len)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int status = 0;
	struct spi_device *spi;
	struct spi_message m;

	u8 *write_value = kzalloc(write_len + 2, GFP_KERNEL);

	struct spi_transfer t = {
		.tx_buf = NULL,
		.len = write_len + 2,
	};

	if (write_value == NULL)
		return -ENOMEM;

	write_value[0] = FP_EEPROM_WRITE_OP;

	if (copy_from_user(write_value + 1, (const u8 __user *) (uintptr_t) buf
		, write_len + 1)) {
		pr_err("%s buffer copy_from_user fail", __func__);
		status = -EFAULT;
		goto exit;
	}

	DEBUG_PRINT("%s write_len = %d\n", __func__, write_len);
	DEBUG_PRINT("%s address = %x\n", __func__, write_value[1]);

	spi = etspi->spi;

	t.tx_buf = write_value;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	status = spi_sync(spi, &m);
	if (status < 0) {
		pr_err("%s read data error status = %d",
				__func__, status);
		goto exit;
	}

exit:
	kfree(write_value);

	return status;
#endif
}
