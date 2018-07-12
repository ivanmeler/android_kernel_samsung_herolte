/*
 * linux/regulator/max77838-regulator.h
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MAX77838_REGULATOR_H
#define __LINUX_MAX77838_REGULATOR_H

/*******************************************************************************
 * Useful Macros
 ******************************************************************************/

#undef  __CONST_FFS
#define __CONST_FFS(_x) \
	((_x) & 0x0F ? ((_x) & 0x03 ? ((_x) & 0x01 ? 0 : 1) :\
					((_x) & 0x04 ? 2 : 3)) :\
					((_x) & 0x30 ? ((_x) & 0x10 ? 4 : 5) :\
					((_x) & 0x40 ? 6 : 7)))

#undef  BIT_RSVD
#define BIT_RSVD  0

#undef  BITS
#define BITS(_end, _start) \
	((BIT(_end) - BIT(_start)) + BIT(_end))

#undef  __BITS_GET
#define __BITS_GET(_word, _mask, _shift) \
	(((_word) & (_mask)) >> (_shift))

#undef  BITS_GET
#define BITS_GET(_word, _bit) \
	__BITS_GET(_word, _bit, FFS(_bit))

#undef  __BITS_SET
#define __BITS_SET(_word, _mask, _shift, _val) \
	(((_word) & ~(_mask)) | (((_val) << (_shift)) & (_mask)))

#undef  BITS_SET
#define BITS_SET(_word, _bit, _val) \
	__BITS_SET(_word, _bit, FFS(_bit), _val)

#undef  BITS_MATCH
#define BITS_MATCH(_word, _bit) \
	(((_word) & (_bit)) == (_bit))


enum max77838_reg_id {
	MAX77838_LDO1 = 1,
	MAX77838_LDO2,
	MAX77838_LDO3,
	MAX77838_LDO4,
	MAX77838_LDO_MAX = MAX77838_LDO4,

	MAX77838_BUCK,

	MAX77838_REGULATORS = MAX77838_BUCK,
};

struct max77838_regulator_data {
	int		active_discharge_enable;

	struct regulator_init_data *initdata;
	struct device_node *of_node;
};

struct max77838_regulator_platform_data {
	int num_regulators;
	struct max77838_regulator_data *regulators;

	int	buck_ramp_up;
	int	buck_ramp_down;
	int	buck_fpwm;
	int	buck_fsrad;

	int	uvlo_fall_threshold;
};

#endif
