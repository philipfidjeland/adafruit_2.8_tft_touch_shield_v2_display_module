/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
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
#include <math.h>
#include <nrf5340_audio_common.h>
#include <button_assignments.h>
#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL

lv_obj_t *volume_level_label;
lv_obj_t *play_pause_label;

LOG_MODULE_REGISTER(display, CONFIG_DISPLAY_LOG_LEVEL);

ZBUS_CHAN_DECLARE(button_chan);
ZBUS_SUBSCRIBER_DEFINE(le_audio_evt_sub_display, CONFIG_LE_AUDIO_MSG_SUB_QUEUE_SIZE);

static void button_create(lv_obj_t *current_screen, char *label_text, lv_align_t button_position,
			  void (*button_callback_function)(lv_event_t *e))
{
	lv_obj_t *label;
	lv_obj_t *button = lv_btn_create(current_screen);
	lv_obj_set_size(button, 40, 40);
	lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
	lv_obj_align(button, button_position, 0, 0);
	label = lv_label_create(button);
	lv_label_set_text(label, label_text);
	lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
	lv_obj_add_event_cb(button, button_callback_function, LV_EVENT_CLICKED, NULL);
}
static void play_pause_button_create(lv_obj_t *current_screen, char *label_text,
				     lv_align_t button_position,
				     void (*button_callback_function)(lv_event_t *e))
{
	lv_obj_t *play_pause_button = lv_btn_create(current_screen);

	lv_obj_set_size(play_pause_button, 40, 40);
	lv_obj_set_style_radius(play_pause_button, LV_RADIUS_CIRCLE, 0);
	lv_obj_align(play_pause_button, button_position, 0, 0);
	play_pause_label = lv_label_create(play_pause_button);
	lv_label_set_text(play_pause_label, label_text);
	lv_obj_align(play_pause_label, LV_ALIGN_CENTER, 0, 0);
	lv_obj_add_event_cb(play_pause_button, button_callback_function, LV_EVENT_CLICKED, NULL);
}

static void play_pause_button_event_cb(lv_event_t *e)
{
	int ret;
	struct button_msg msg;

	msg.button_action = BUTTON_PRESS;
	msg.button_pin = BUTTON_PLAY_PAUSE;

	ret = zbus_chan_pub(&button_chan, &msg, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Failed to publish button msg, ret: %d", ret);
	}
}

static void volume_up_button_event_cb(lv_event_t *e)
{
	int ret;
	struct button_msg msg;

	msg.button_action = BUTTON_PRESS;
	msg.button_pin = BUTTON_VOLUME_UP;

	ret = zbus_chan_pub(&button_chan, &msg, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Failed to publish button msg, ret: %d", ret);
	}
}

static void volume_down_button_event_cb(lv_event_t *e)

{
	int ret;
	struct button_msg msg;

	msg.button_action = BUTTON_PRESS;
	msg.button_pin = BUTTON_VOLUME_DOWN;

	ret = zbus_chan_pub(&button_chan, &msg, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Failed to publish button msg, ret: %d", ret);
	}
}

static void btn4_button_event_cb(lv_event_t *e)
{
	int ret;
	struct button_msg msg;

	msg.button_action = BUTTON_PRESS;
	msg.button_pin = BUTTON_4;

	ret = zbus_chan_pub(&button_chan, &msg, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Failed to publish button msg, ret: %d", ret);
	}
}

static void btn5_button_event_cb(lv_event_t *e)
{
	int ret;
	struct button_msg msg;

	msg.button_action = BUTTON_PRESS;
	msg.button_pin = BUTTON_5;

	ret = zbus_chan_pub(&button_chan, &msg, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Failed to publish button msg, ret: %d", ret);
	}
}

static void update_streaming_state(lv_obj_t *play_pause_button)
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
			lv_label_set_text(play_pause_label, LV_SYMBOL_PAUSE);
			break;

		case LE_AUDIO_EVT_NOT_STREAMING:
			lv_label_set_text(play_pause_label, LV_SYMBOL_PLAY);
			break;
		default:
			break;
		}
		if (ret == 35) {
			ret = 0;
		} else {
			// include en ERR_CHK here
		}
	}
}
static void update_volume_label(lv_obj_t *volume_label)
{
	int volume_level_int;
	char volume_level_str[8];
#if (CONFIG_AUDIO_DEV == 1)
	volume_level_int = hw_codec_volume_get();
	sprintf(volume_level_str, "%d", volume_level_int);
	lv_label_set_text(volume_level_label, volume_level_str);
#endif
}

void update_button_tab()
{
	update_volume_label(volume_level_label);
	update_streaming_state(play_pause_label);
}
static void volume_label_create(lv_obj_t *current_screen)
{

	volume_level_label = lv_label_create(current_screen);
	lv_label_set_text(volume_level_label, "");
	lv_obj_align(volume_level_label, LV_ALIGN_RIGHT_MID, 0, 0);
}
static void devicetype_label_create(lv_obj_t *current_screen)
{
	char *what_dev_am_i;

#if (CONFIG_TRANSPORT_CIS)
#if (CONFIG_AUDIO_DEV == 1)
	what_dev_am_i = "Headset\n CIS";
#endif
#if (CONFIG_AUDIO_DEV == 2)
	what_dev_am_i = "Gateway\n CIS";
#endif
#endif
#if (CONFIG_TRANSPORT_BIS)
#if (CONFIG_AUDIO_DEV == 1)
	what_dev_am_i = "Headset\n BIS";
#endif
#if (CONFIG_AUDIO_DEV == 2)
	what_dev_am_i = "Gateway\n BIS";
#endif
#endif
	lv_obj_t *what_dev_am_i_label;
	what_dev_am_i_label = lv_label_create(current_screen);
	lv_label_set_text(what_dev_am_i_label, what_dev_am_i);
	lv_obj_align(what_dev_am_i_label, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void button_tab_create(lv_obj_t *current_screen)
{
#if ((CONFIG_AUDIO_DEV == 1))
	volume_label_create(current_screen);
#endif
#if ((CONFIG_AUDIO_DEV == 1) && (CONFIG_TRANSPORT_BIS))
	button_create(current_screen, "Change audio stream", LV_ALIGN_TOP_LEFT,
		      btn4_button_event_cb);
#endif

#if ((CONFIG_AUDIO_DEV == 2) && (CONFIG_TRANSPORT_BIS))
	button_create(current_screen, LV_SYMBOL_AUDIO, LV_ALIGN_TOP_LEFT, btn4_button_event_cb);
#endif
#if (CONFIG_TRANSPORT_CIS)
	button_create(current_screen, LV_SYMBOL_AUDIO, LV_ALIGN_TOP_LEFT, btn4_button_event_cb);
#endif
	play_pause_button_create(current_screen, LV_SYMBOL_PLAY, LV_ALIGN_LEFT_MID,
				 play_pause_button_event_cb);
	button_create(current_screen, LV_SYMBOL_VOLUME_MAX, LV_ALIGN_TOP_RIGHT,
		      volume_up_button_event_cb);
	button_create(current_screen, LV_SYMBOL_VOLUME_MID, LV_ALIGN_BOTTOM_RIGHT,
		      volume_down_button_event_cb);
	button_create(current_screen, LV_SYMBOL_MUTE, LV_ALIGN_BOTTOM_LEFT, btn5_button_event_cb);

	devicetype_label_create(current_screen);
}