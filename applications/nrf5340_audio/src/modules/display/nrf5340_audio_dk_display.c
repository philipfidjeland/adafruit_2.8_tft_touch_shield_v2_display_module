/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <lvgl.h>
#include "tab_menu.h"
#include "tab_log.h"
#include "macros_common.h"

#define DISPLAY_THREAD_PRIORITY	  5
#define DISPLAY_INIT_PRIORITY	  91 /* Must have lower priority than LVGL init */
#define DISPLAY_THREAD_STACK_SIZE 4096

LOG_MODULE_REGISTER(display_log, CONFIG_DISPLAY_LOG_LEVEL);

struct k_thread display_data;
k_tid_t display_thread;
lv_obj_t *tab_log;

ZBUS_CHAN_DECLARE(button_chan);
ZBUS_CHAN_DECLARE(le_audio_chan);

ZBUS_OBS_DECLARE(le_audio_evt_sub_display);

K_THREAD_STACK_DEFINE(display_thread_STACK, DISPLAY_THREAD_STACK_SIZE);

void nrf5340_audio_dk_display_update_thread(void *arg1, void *arg2, void *arg3)
{

	int update_counter = 0;
	int update_counter_menu = 0;

	while (1) {
		update_counter++;
		if (update_counter >= 10) {
			update_counter_menu = tab_menu_update(update_counter_menu);
			update_counter = 0;
		}
		uint32_t time_till_next = lv_timer_handler();

		k_sleep(K_MSEC(time_till_next));
	}
}

lv_obj_t *tab_log_get(void)
{
	return tab_log;
}

static int nrf5340_audio_dk_display_init(void)
{
	const struct device *display_dev;
	int ret;
	lv_obj_t *tabview;
	lv_obj_t *tab_menu;

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Device not ready, aborting test");
		return -ENODEV;
	}

	tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 50);

	tab_menu = lv_tabview_add_tab(tabview, "Menu");
	tab_log = lv_tabview_add_tab(tabview, "Logging");

	ret = zbus_chan_add_obs(&le_audio_chan, &le_audio_evt_sub_display, K_MSEC(200));
	ERR_CHK(ret);

	display_blanking_off(display_dev);

	tab_menu_create(tab_menu);

	tab_log_create(tab_log);

	lv_obj_clear_flag(lv_tabview_get_content(tabview), LV_OBJ_FLAG_SCROLLABLE);

	display_thread = k_thread_create(&display_data, display_thread_STACK,
					 K_THREAD_STACK_SIZEOF(display_thread_STACK),
					 nrf5340_audio_dk_display_update_thread, NULL, NULL, NULL,
					 K_PRIO_PREEMPT(DISPLAY_THREAD_PRIORITY), 0, K_NO_WAIT);
	k_thread_name_set(display_thread, "Nrf5340_audio__dk display thread");

	return 0;
}
SYS_INIT(nrf5340_audio_dk_display_init, APPLICATION, DISPLAY_INIT_PRIORITY);
