/*
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define PN547_MAGIC	0xE9

/*
 * PN544 power control via ioctl
 * PN544_SET_PWR(0): power off
 * PN544_SET_PWR(1): power on
 * PN544_SET_PWR(>1): power on with firmware download enabled
 */
#define PN547_SET_PWR	_IOW(PN547_MAGIC, 0x01, unsigned int)

#if 1 //ese
/*
 * SPI Request NFCC to enable p61 power, only in param
 * Only for SPI
 * level 1 = Enable power
 * level 0 = Disable power
 */
#define P61_SET_SPI_PWR    _IOW(PN547_MAGIC, 0x02, unsigned int)

/* SPI or DWP can call this ioctl to get the current
 * power state of P61
 *
*/
#define P61_GET_PWR_STATUS    _IOR(PN547_MAGIC, 0x03, unsigned int)

/* DWP side this ioctl will be called
 * level 1 = Wired access is enabled/ongoing
 * level 0 = Wired access is disalbed/stopped
*/
#define P61_SET_WIRED_ACCESS _IOW(PN547_MAGIC, 0x04, unsigned int)

#ifdef CONFIG_NFC_PN547_LDO_CONTROL
#define NFC_I2C_LDO_ON	1
#define NFC_I2C_LDO_OFF	0
#endif

typedef enum p61_access_state{
    P61_STATE_IDLE, /* p61 is free to use */
    P61_STATE_WIRED,  /* p61 is being accessed by DWP (NFCC)*/
    P61_STATE_SPI, /* P61 is being accessed by SPI */
    P61_STATE_DWNLD, /* NFCC fw download is in progress */
    P61_STATE_INVALID
}p61_access_state_t;
#endif

struct pn547_i2c_platform_data {
	void (*conf_gpio) (void);
	int irq_gpio;
	int ven_gpio;
	int firm_gpio;
#if 1 //ese
	int ese_pwr_req;
#endif
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
	int clk_req_gpio;
	int clk_req_irq;
#endif
#ifdef CONFIG_NFC_PN547_8226_USE_BBCLK2
	int clk_req_gpio;
#endif
#ifdef CONFIG_SEC_K_PROJECT
	int scl_gpio;
	int sda_gpio;
#endif
#ifdef CONFIG_OF
	u32 irq_gpio_flags;
	u32 ven_gpio_flags;
	u32 firm_gpio_flags;
#ifdef CONFIG_SEC_K_PROJECT
	u32 scl_gpio_flags;
	u32 sda_gpio_flags;
#endif
	u32 ese_pwr_req_flags; ///
#endif
#ifdef CONFIG_NFC_PN547_LDO_CONTROL
	const char *i2c_1p8;
#endif
};

#ifdef CONFIG_NFC_PN547_LDO_CONTROL
extern unsigned int lpcharge;
#endif
