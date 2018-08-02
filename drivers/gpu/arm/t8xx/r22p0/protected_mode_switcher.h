/*
 *
 * (C) COPYRIGHT 2017 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#ifndef _PROTECTED_MODE_SWITCH_H_
#define _PROTECTED_MODE_SWITCH_H_

struct protected_mode_device;
/* MALI_SEC_INTEGRATION */
#if defined(CONFIG_MALI_EXYNOS_SECURE_RENDERING) && defined(CONFIG_MALI_SEC_ASP_SECURE_BUF_CTRL)
struct kbase_device;
struct kbase_va_region;
#endif

/**
 * struct protected_mode_ops - Callbacks for protected mode switch operations
 *
 * @protected_mode_enable:  Callback to enable protected mode for device
 * @protected_mode_disable: Callback to disable protected mode for device
 */
struct protected_mode_ops {
	/**
	 * protected_mode_enable() - Enable protected mode on device
	 * @dev:	The struct device
	 *
	 * Return: 0 on success, non-zero on error
	 */
	int (*protected_mode_enable)(
			struct protected_mode_device *protected_dev);

	/**
	 * protected_mode_disable() - Disable protected mode on device, and
	 *                            reset device
	 * @dev:	The struct device
	 *
	 * Return: 0 on success, non-zero on error
	 */
	int (*protected_mode_disable)(
			struct protected_mode_device *protected_dev);

/* MALI_SEC_INTEGRATION */
#if defined(CONFIG_MALI_EXYNOS_SECURE_RENDERING) && defined(CONFIG_MALI_SEC_ASP_SECURE_BUF_CTRL)
	/**
	 * protected_mode_supported() - Check if protected mode is supported
	 * @kbdev:	The kbase device
	 *
	 * Return: 0 on success, non-zero on error
	 */
	bool (*secure_mem_init)(struct kbase_device *kbdev);

    /** Platform specific function for setting the Memory to secure mode.
     *
     * Return 0 if successful, otherwise error
     */
    int (*secure_mem_enable)(struct kbase_device *kbdev, int ion_fd, u64 flags, struct kbase_va_region *reg);

    /** Platform specific function for setting the Memory back to normal mode.
     *
     * Return 0 if successful, otherwise error
     */
    int (*secure_mem_disable)(struct kbase_device *kbdev, struct kbase_va_region *reg);
#endif
};

/**
 * struct protected_mode_device - Device structure for protected mode devices
 *
 * @ops  - Callbacks associated with this device
 * @data - Pointer to device private data
 *
 * This structure should be registered with the platform device using
 * platform_set_drvdata().
 */
struct protected_mode_device {
	struct protected_mode_ops ops;
	void *data;
};

#endif /* _PROTECTED_MODE_SWITCH_H_ */
