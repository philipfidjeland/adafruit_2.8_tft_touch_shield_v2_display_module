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
#include <zephyr/debug/stack.h>
#include <errno.h>

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

lv_obj_t *volume_level_label;
struct k_thread display_data;
k_tid_t display_thread;

ZBUS_CHAN_DECLARE(button_chan);

ZBUS_SUBSCRIBER_DEFINE(le_audio_evt_sub_display, CONFIG_LE_AUDIO_MSG_SUB_QUEUE_SIZE);

void update_streaming_state(lv_obj_t *play_pause_button)
{
	int ret;

	const struct zbus_channel *chan;
	uint8_t event;

	ret = zbus_sub_wait(&le_audio_evt_sub_display, &chan, K_NO_WAIT);
	if (ret == 0) {

		struct le_audio_msg msg;
		ret = zbus_chan_read(chan, &msg, K_NO_WAIT);

		event = msg.event;

		switch (event) {
		case LE_AUDIO_EVT_STREAMING:
			lv_label_set_text(play_pause_button, LV_SYMBOL_PAUSE);
			break;

		case LE_AUDIO_EVT_NOT_STREAMING:
			lv_label_set_text(play_pause_button, LV_SYMBOL_PLAY);
			break;
		default:
			break;
		}
		if (ret == 35) {
			ret = 0;
		} else {
		}
	}
}
void create_timestamp_label(lv_obj_t *current_screen)
{
	static uint32_t count;
	char count_str[11] = {0};
	lv_obj_t *count_label;
	count_label = lv_label_create(current_screen);
	lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, 0);
	while (1) {
		if ((count % 100) == 0U) {
			sprintf(count_str, "%d", count / 100U);
			lv_label_set_text(count_label, count_str);
		}
		lv_task_handler();
		++count;
		k_sleep(K_MSEC(10));
	}
}

K_THREAD_STACK_DEFINE(display_thread_STACK, 8000);

void update_thread(void *arg1, void *arg2, void *arg3)
{
	int volume_level_int;
	char volume_level_str[8];
	int update_counter = 0;

	while (1) {
		update_counter++;
		/* #if (CONFIG_AUDIO_DEV == 1)
				if (update_counter >= 100) {
					volume_level_int = hw_codec_volume_get();
					sprintf(volume_level_str, "%d", volume_level_int);
					lv_label_set_text(volume_level_label, volume_level_str);
					update_counter = 0;
				}
		#endif */
		if (update_counter >= 100) {
			update_streaming_state(volume_level_label);
			update_counter = 0;
		}
		lv_task_handler();
		k_sleep(K_MSEC(16));
	}
}

static void volume_label_create(lv_obj_t *current_screen)
{

	volume_level_label = lv_label_create(current_screen);
	lv_label_set_text(volume_level_label, "");
	lv_obj_align(volume_level_label, LV_ALIGN_RIGHT_MID, 0, 0);
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
	volume_label_create(button_tab);

	display_thread = k_thread_create(&display_data, display_thread_STACK,
					 K_THREAD_STACK_SIZEOF(display_thread_STACK), update_thread,
					 NULL, NULL, NULL, MY_PRIORITY, 0, K_NO_WAIT);
	return 0;
}