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
#include <stdlib.h>
#include <math.h>

#include <lvgl.h>
#include "tab_menu.h"
#include <nrf5340_audio_common.h>
#include <button_assignments.h>
#include <errno.h>
#include "macros_common.h"
#include <pcm_stream_channel_modifier.h>
#include <hw_codec.h>
#include <audio_defines.h>
#include <channel_assignment.h>

#define AVG_FRAME_VAL_MAX                                                                          \
	(32767 * 0.636) /* Highest avg value of a sine wave with 32767                             \
			   amplitude*/

static lv_obj_t *volume_level_label;
static lv_obj_t *play_pause_label;
static lv_obj_t *vu_bar;

static lv_style_t style_btn_gray;
static lv_style_t style_btn_red;

static enum audio_channel channel;

static int32_t frame_val;
static uint8_t
	audio_streaming_event; /* Must be declaired here to make bar 0 while state not streaming */

LOG_MODULE_DECLARE(display_log, CONFIG_DISPLAY_LOG_LEVEL);

ZBUS_CHAN_DECLARE(button_chan);
ZBUS_SUBSCRIBER_DEFINE(le_audio_evt_sub_display, CONFIG_LE_AUDIO_MSG_SUB_QUEUE_SIZE);

static void style_init(lv_color_t style_color, lv_style_t *style_btn)
{
	lv_style_init(style_btn);
	lv_style_set_bg_color(style_btn, style_color);
	lv_style_set_bg_grad_color(style_btn, style_color);
}

static lv_obj_t *button_create(lv_obj_t *current_screen, char *label_text,
			       lv_align_t button_position,
			       void (*button_callback_function)(lv_event_t *e),
			       const lv_font_t *font_size)
{
	lv_obj_t *button = lv_btn_create(current_screen);

	lv_obj_set_size(button, 47, 47);
	lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
	lv_obj_align(button, button_position, 0, 0);

	lv_obj_t *label = lv_label_create(button);
	lv_label_set_text(label, label_text);
	lv_obj_set_style_text_font(label, font_size, LV_STATE_DEFAULT);
	lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
	lv_obj_add_event_cb(button, button_callback_function, LV_EVENT_CLICKED, NULL);

	lv_obj_add_style(button, &style_btn_red, LV_STATE_PRESSED);

	return label;
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

static void vu_bar_update(int vu_val)
{
	lv_bar_set_value(vu_bar, vu_val, LV_ANIM_ON);
}

static int streaming_state_update(lv_obj_t *play_pause_button)
{
	int ret;
	const struct zbus_channel *chan;

	ret = zbus_sub_wait(&le_audio_evt_sub_display, &chan, K_NO_WAIT);

	if (ret == -ENOMSG) {
		return 0;
	} else if (ret != 0) {
		return ret;
	}

	struct le_audio_msg msg;

	ret = zbus_chan_read(chan, &msg, K_NO_WAIT);

	if (ret) {
		return ret;
	}
	audio_streaming_event = msg.event;

	switch (audio_streaming_event) {
	case LE_AUDIO_EVT_STREAMING:
		lv_label_set_text(play_pause_label, LV_SYMBOL_PAUSE);
		break;

	case LE_AUDIO_EVT_NOT_STREAMING:
		lv_label_set_text(play_pause_label, LV_SYMBOL_PLAY);
		break;
	default:
		break;
	}
	return 0;
}

static void volume_label_update(lv_obj_t *volume_label)
{
	double volume_level_int;
	char volume_level_str[32];

	volume_level_int = hw_codec_volume_get();
	sprintf(volume_level_str, "%.0lf", floor((volume_level_int / 128) * 100));
	strcat(volume_level_str, "%");
	lv_label_set_text(volume_level_label, volume_level_str);
}

uint8_t tab_menu_update(uint8_t update_counter)
{
	update_counter++;

	if (update_counter >= 5) {
		if (CONFIG_AUDIO_DEV == HEADSET) {
			volume_label_update(volume_level_label);
			if (audio_streaming_event == LE_AUDIO_EVT_STREAMING) {
				switch (channel) {
				case AUDIO_CH_L:
					frame_val = pcsm_avg_frame_val_left_get();
					break;
				case AUDIO_CH_R:
					frame_val = pcsm_avg_frame_val_right_get();
					break;
				default:
					break;
				}
				int frame_val_percent =
					(int)round((double)frame_val / AVG_FRAME_VAL_MAX * 100);
				vu_bar_update(frame_val_percent);
			} else {
				vu_bar_update(0);
			}
		}
		streaming_state_update(play_pause_label);
		update_counter = 0;
	}
	return update_counter;
}

static void volume_label_create(lv_obj_t *current_screen)
{

	volume_level_label = lv_label_create(current_screen);

	lv_label_set_text(volume_level_label, "");
	lv_obj_align(volume_level_label, LV_ALIGN_RIGHT_MID, 0, 0);
}

static void device_info_label_create(lv_obj_t *current_screen)
{
	char *device_info;

	if (IS_ENABLED(CONFIG_TRANSPORT_CIS)) {
		if (CONFIG_AUDIO_DEV == HEADSET) {
			if (channel == AUDIO_CH_L) {
				device_info = "Headset Left\n CIS";
			}
			if (channel == AUDIO_CH_R) {
				device_info = "Headset Right\n CIS";
			}
		}

		if (CONFIG_AUDIO_DEV == GATEWAY) {
			device_info = "Gateway\n CIS";
		}
	}

	if (IS_ENABLED(CONFIG_TRANSPORT_BIS)) {
		if (CONFIG_AUDIO_DEV == HEADSET) {
			if (channel == AUDIO_CH_L) {
				device_info = "Headset Left\n BIS";
			}
			if (channel == AUDIO_CH_R) {
				device_info = "Headset Right\n BIS";
			}
		}

		if (CONFIG_AUDIO_DEV == GATEWAY) {
			device_info = "Gateway\n BIS";
		}
	}

	lv_obj_t *device_info_label = lv_label_create(current_screen);

	lv_label_set_text(device_info_label, device_info);
	lv_obj_align(device_info_label, LV_ALIGN_BOTTOM_MID, 0, 0);
}

static void vu_bar_create(lv_obj_t *current_screen)
{
	static lv_style_t style_indic;

	lv_style_init(&style_indic);
	lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
	lv_style_set_bg_color(&style_indic, lv_palette_main(LV_PALETTE_BLUE));
	lv_style_set_bg_grad_color(&style_indic, lv_palette_main(LV_PALETTE_RED));
	lv_style_set_bg_grad_dir(&style_indic, LV_GRAD_DIR_HOR);

	vu_bar = lv_bar_create(current_screen);
	lv_obj_add_style(vu_bar, &style_indic, LV_PART_INDICATOR);
	lv_obj_set_size(vu_bar, 150, 20);
	lv_obj_center(vu_bar);
}

static void vu_bar_label_create(lv_obj_t *current_screen)
{
	lv_obj_t *vu_bar_label;

	vu_bar_label = lv_label_create(current_screen);
	lv_label_set_text(vu_bar_label, "VU-bar");
	lv_obj_align(vu_bar_label, LV_ALIGN_TOP_MID, 0, 20);
}

void tab_menu_create(lv_obj_t *current_screen)
{

	style_init(lv_palette_main(LV_PALETTE_RED), &style_btn_red);
	style_init(lv_palette_main(LV_PALETTE_GREY), &style_btn_gray);

	if (CONFIG_AUDIO_DEV == HEADSET) {
		{
			volume_label_create(current_screen);
			vu_bar_create(current_screen);
			vu_bar_label_create(current_screen);
			channel_assignment_get(&channel);
		}
		lv_obj_t *change_stream_label =
			button_create(current_screen, LV_SYMBOL_BLUETOOTH, LV_ALIGN_TOP_LEFT,
				      btn4_button_event_cb, &lv_font_montserrat_20);

		if (IS_ENABLED(CONFIG_TRANSPORT_CIS) == 1) {
			lv_obj_remove_event_cb(lv_obj_get_parent(change_stream_label),
					       btn4_button_event_cb);
			lv_obj_add_style(lv_obj_get_parent(change_stream_label), &style_btn_gray,
					 LV_STATE_DEFAULT);
			lv_obj_add_style(lv_obj_get_parent(change_stream_label), &style_btn_gray,
					 LV_STATE_PRESSED);
		}
	}

	if (IS_ENABLED(CONFIG_AUDIO_TEST_TONE) && (CONFIG_AUDIO_DEV == GATEWAY)) {
		button_create(current_screen, "~", LV_ALIGN_TOP_LEFT, btn4_button_event_cb,
			      &lv_font_montserrat_48);
	}
	if (IS_ENABLED(CONFIG_AUDIO_MUTE)) {
		button_create(current_screen, LV_SYMBOL_MUTE, LV_ALIGN_BOTTOM_LEFT,
			      btn5_button_event_cb, &lv_font_montserrat_20);
	}

	play_pause_label = button_create(current_screen, LV_SYMBOL_PLAY, LV_ALIGN_LEFT_MID,
					 play_pause_button_event_cb, &lv_font_montserrat_14);
	button_create(current_screen, LV_SYMBOL_VOLUME_MAX, LV_ALIGN_TOP_RIGHT,
		      volume_up_button_event_cb, &lv_font_montserrat_20);
	button_create(current_screen, LV_SYMBOL_VOLUME_MID, LV_ALIGN_BOTTOM_RIGHT,
		      volume_down_button_event_cb, &lv_font_montserrat_20);
	device_info_label_create(current_screen);
}
