/* sec_nad.h
 *
 * Copyright (C) 2016 Samsung Electronics
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef SEC_NAD_H
#define SEC_NAD_H

#if defined(CONFIG_SEC_FACTORY)
#define NAD_PARAM_NAME "/dev/block/NAD_REFER"
#define NAD_OFFSET 8192 
#define NAD_ENV_OFFSET 10240

#define NAD_PARAM_READ  0
#define NAD_PARAM_WRITE 1 
#define NAD_PARAM_EMPTY 2

#define NAD_DRAM_TEST_NONE  0
#define NAD_DRAM_TEST_PASS  1
#define NAD_DRAM_TEST_FAIL  2

#define NAD_BUFF_SIZE       10 
#define NAD_CMD_LIST        3

#define NAD_ACAT_FAIL_SKIP_FLAG 1001
#define UNLIMITED_LOOP_FLAG     1002
#define NAD_SUPPORT_FLAG        1003
#define NAD_ACAT_FLAG           5001

#define NAD_RETRY_COUNT         30

/* MAGIC CODE for NAD API Success */
#define MAGIC_NAD_API_SUCCESS	6057

#if defined(CONFIG_SEC_NAD_API)
static char nad_api_result_string[20*30] = {0,}; 

typedef struct {
    char name[20];
    unsigned int result;
} nad_api_results;
#endif

struct nad_env {
    char nad_factory[10];
    char nad_result[4];
    unsigned int nad_data;

    char nad_acat[10];
    char nad_acat_result[4];
    unsigned int nad_acat_data;
    unsigned int nad_acat_loop_count;
    unsigned int nad_acat_real_count;

    char nad_dram_test_need[4];
    unsigned int nad_dram_test_result;
    unsigned int nad_dram_fail_data;
    unsigned long nad_dram_fail_address;

    int current_nad_status;
    unsigned int nad_acat_skip_fail;
    unsigned int unlimited_loop;
    unsigned int nad_support;
    unsigned int max_temperature;
    unsigned int nad_acat_max_temperature;
#if defined(CONFIG_SEC_NAD_API)
    int nad_api_status;
    int nad_api_magic;
    int nad_api_total_count;
    nad_api_results nad_api_info[30];
#endif
};

struct sec_nad_param {
    struct work_struct sec_nad_work;
    struct delayed_work sec_nad_delay_work;
    unsigned long offset;
    int state;
    int retry_cnt;
    int curr_cnt;
};

static struct sec_nad_param sec_nad_param_data;
static struct nad_env sec_nad_env;
extern unsigned int lpcharge;
static DEFINE_MUTEX(sec_nad_param_lock);
#endif

#endif
