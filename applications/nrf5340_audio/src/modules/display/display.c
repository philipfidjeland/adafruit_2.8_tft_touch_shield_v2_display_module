/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <display/display.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>

#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include "button_tab.h"
#include <nrf5340_audio_common.h>
#include <hw_codec.h>
#define MY_PRIORITY 5
LOG_MODULE_REGISTER(app);

struct k_thread display_data;
k_tid_t display_thread;

ZBUS_CHAN_DECLARE(button_chan);

K_THREAD_STACK_DEFINE(display_thread_STACK, 8000);

void update_thread(void *arg1, void *arg2, void *arg3)
{

	int update_counter = 0;

	while (1) {
		update_counter++;

		if (update_counter >= 100) {
			update_button_tab();
			update_counter = 0;
		}
		lv_task_handler();
		k_sleep(K_MSEC(16));
	}
}

int display_init()
{
	lv_obj_t *tabview;
	tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 50);

	lv_obj_t *button_tab = lv_tabview_add_tab(tabview, "Button Tab");
	lv_obj_t *log_tab = lv_tabview_add_tab(tabview, "Log Tab");
	const struct device *display_dev;
	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(display_dev)) {
		LOG_ERR("Device not ready, aborting test");
		return -ENODEV;
	}
	display_blanking_off(display_dev);
	button_tab_create(button_tab);

	display_thread = k_thread_create(&display_data, display_thread_STACK,
					 K_THREAD_STACK_SIZEOF(display_thread_STACK), update_thread,
					 NULL, NULL, NULL, MY_PRIORITY, 0, K_NO_WAIT);
	return 0;
}