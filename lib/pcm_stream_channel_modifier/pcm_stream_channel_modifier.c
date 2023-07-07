/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "pcm_stream_channel_modifier.h"

#include <zephyr/kernel.h>
#include <errno.h>

#include <zephyr/logging/log.h>
#include <stdlib.h>
LOG_MODULE_REGISTER(pscm, CONFIG_PSCM_LOG_LEVEL);
static int32_t pcsm_avg_frame_val_left;
static int32_t pcsm_avg_frame_val_right;
/**
 * @brief      Determines whether the specified pcm bit depth is valid bit depth.
 *
 * @param[in]  pcm_bit_depth  The pcm bit depth
 *
 * @return     True if the specified pcm bit depth is valid bit depth, False otherwise.
 */
static bool is_valid_bit_depth(uint8_t pcm_bit_depth)
{
	if (pcm_bit_depth != 16 && pcm_bit_depth != 24 && pcm_bit_depth != 32) {
		LOG_ERR("Invalid bit depth: %d", pcm_bit_depth);
		return false;
	}

	return true;
}

/**
 * @brief      Determines if valid size.
 *
 * @param[in]  size              The size
 * @param[in]  bytes_per_sample  The bytes per sample
 * @param[in]  no_channels       No channels
 *
 * @return     True if valid size, False otherwise.
 */
static bool is_valid_size(size_t size, uint8_t bytes_per_sample, uint8_t no_channels)
{
	if (size % (bytes_per_sample * no_channels) != 0) {
		LOG_ERR("Size: %d is not dividable with number of bytes per sample x num channels",
			size);
		return false;
	}

	return true;
}

int pscm_zero_pad(void const *const input, size_t input_size, enum audio_channel channel,
		  uint8_t pcm_bit_depth, void *output, size_t *output_size)
{
	uint8_t bytes_per_sample = pcm_bit_depth / 8;

	int32_t total_of_all_frames_left = 0;
	int32_t total_of_all_frames_right = 0;

	int16_t temp_frame_sum_left = 0;
	int16_t temp_frame_sum_right = 0;

	if (!is_valid_bit_depth(pcm_bit_depth) || !is_valid_size(input_size, bytes_per_sample, 1)) {
		return -EINVAL;
	}

	char *pointer_input = (char *)input;
	char *pointer_output = (char *)output;

	for (uint32_t i = 0; i < input_size / bytes_per_sample; i++) {
		if (channel == AUDIO_CH_L) {
			for (uint8_t j = 0; j < bytes_per_sample; j++) {
				temp_frame_sum_left += ((*pointer_input) << 8 * j);
				temp_frame_sum_right += ((*pointer_input) << 8 * j);
				*pointer_output++ = *pointer_input++;
			}

			for (uint8_t j = 0; j < bytes_per_sample; j++) {
				*pointer_output++ = 0;
			}
		} else if (channel == AUDIO_CH_R) {
			for (uint8_t j = 0; j < bytes_per_sample; j++) {
				*pointer_output++ = 0;
			}

			for (uint8_t j = 0; j < bytes_per_sample; j++) {

				*pointer_output++ = *pointer_input++;
			}
		} else {
			LOG_ERR("Invalid channel selection");
			return -EINVAL;
		}
		total_of_all_frames_left += abs(temp_frame_sum_left);
		total_of_all_frames_right += abs(temp_frame_sum_right);

		temp_frame_sum_left = 0;
		temp_frame_sum_right = 0;
	}
	if (input_size / bytes_per_sample != 0) {

		pcsm_avg_frame_val_left =
			total_of_all_frames_left / (int)(input_size / bytes_per_sample);
		pcsm_avg_frame_val_right =
			total_of_all_frames_right / (int)(input_size / bytes_per_sample);
	} else {
		LOG_ERR("Division by zero occurred");
	}

	*output_size = input_size * 2;
	return 0;
}

int pscm_copy_pad(void const *const input, size_t input_size, uint8_t pcm_bit_depth, void *output,
		  size_t *output_size)
{
	uint8_t bytes_per_sample = pcm_bit_depth / 8;

	int32_t total_of_all_frames = 0;
	int16_t temp_frame_sum = 0;

	if (!is_valid_bit_depth(pcm_bit_depth) || !is_valid_size(input_size, bytes_per_sample, 1)) {
		return -EINVAL;
	}

	char *pointer_input = (char *)input;
	char *pointer_output = (char *)output;

	for (uint32_t i = 0; i < input_size / bytes_per_sample; i++) {
		for (uint8_t j = 0; j < bytes_per_sample; j++) {
			temp_frame_sum += ((*pointer_input) << 8 * j);
			*pointer_output++ = *pointer_input++;
		}
		/* Move back to start of sample to copy into next channel */
		pointer_input -= bytes_per_sample;

		for (uint8_t j = 0; j < bytes_per_sample; j++) {
			*pointer_output++ = *pointer_input++;
		}
		total_of_all_frames += abs(temp_frame_sum);
		temp_frame_sum = 0;
	}
	if (input_size / bytes_per_sample != 0) {

		pcsm_avg_frame_val_left =
			total_of_all_frames / (int)(input_size / bytes_per_sample);
		pcsm_avg_frame_val_right =
			total_of_all_frames / (int)(input_size / bytes_per_sample);
	} else {
		LOG_ERR("Division by zero occurred");
	}
	*output_size = input_size * 2;
	return 0;
}

int pscm_combine(void const *const input_left, void const *const input_right, size_t input_size,
		 uint8_t pcm_bit_depth, void *output, size_t *output_size)
{
	uint8_t bytes_per_sample = pcm_bit_depth / 8;

	int32_t total_of_all_frames = 0;
	int16_t temp_frame_sum = 0;

	if (!is_valid_bit_depth(pcm_bit_depth) || !is_valid_size(input_size, bytes_per_sample, 1)) {
		return -EINVAL;
	}

	char *pointer_input_left = (char *)input_left;
	char *pointer_input_right = (char *)input_right;
	char *pointer_output = (char *)output;

	for (uint32_t i = 0; i < input_size / bytes_per_sample; i++) {
		for (uint8_t j = 0; j < bytes_per_sample; j++) {
			temp_frame_sum += ((*pointer_input_left) << 8 * j);
			*pointer_output++ = *pointer_input_left++;
		}
		for (uint8_t j = 0; j < bytes_per_sample; j++) {
			*pointer_output++ = *pointer_input_right++;
		}
		total_of_all_frames += abs(temp_frame_sum);
		temp_frame_sum = 0;
	}
	if (input_size / bytes_per_sample != 0) {

		pcsm_avg_frame_val_left =
			total_of_all_frames / (int)(input_size / bytes_per_sample);
		pcsm_avg_frame_val_right =
			total_of_all_frames / (int)(input_size / bytes_per_sample);
	} else {
		LOG_ERR("Division by zero occurred");
	}

	*output_size = input_size * 2;
	return 0;
}

int pscm_one_channel_split(void const *const input, size_t input_size, enum audio_channel channel,
			   uint8_t pcm_bit_depth, void *output, size_t *output_size)
{
	uint8_t bytes_per_sample = pcm_bit_depth / 8;

	int32_t total_of_all_frames_left = 0;
	int32_t total_of_all_frames_right = 0;

	int16_t temp_frame_sum_left = 0;
	int16_t temp_frame_sum_right = 0;

	if (!is_valid_bit_depth(pcm_bit_depth) || !is_valid_size(input_size, bytes_per_sample, 2)) {
		return -EINVAL;
	}

	char *pointer_input = (char *)input;
	char *pointer_output = (char *)output;

	for (uint32_t i = 0; i < input_size / bytes_per_sample; i += 2) {
		if (channel == AUDIO_CH_L) {
			for (uint8_t j = 0; j < bytes_per_sample; j++) {
				temp_frame_sum_left += ((*pointer_input) << 8 * j);
				*pointer_output++ = *pointer_input++;
			}
			pointer_input += bytes_per_sample;

		} else if (channel == AUDIO_CH_R) {
			pointer_input += bytes_per_sample;

			for (uint8_t j = 0; j < bytes_per_sample; j++) {
				temp_frame_sum_right += ((*pointer_input) << 8 * j);
				*pointer_output++ = *pointer_input++;
			}
		} else {
			LOG_ERR("Invalid channel selection");
			return -EINVAL;
		}
		total_of_all_frames_left += abs(temp_frame_sum_left);
		total_of_all_frames_right += abs(temp_frame_sum_right);
		temp_frame_sum_left = 0;
		temp_frame_sum_right = 0;
	}
	if (input_size / bytes_per_sample != 0) {

		pcsm_avg_frame_val_left =
			total_of_all_frames_left / (int)(input_size / bytes_per_sample);
		pcsm_avg_frame_val_right =
			total_of_all_frames_right / (int)(input_size / bytes_per_sample);
	} else {
		LOG_ERR("Division by zero occurred");
	}

	*output_size = input_size / 2;
	return 0;
}

int pscm_two_channel_split(void const *const input, size_t input_size, uint8_t pcm_bit_depth,
			   void *output_left, void *output_right, size_t *output_size)
{
	uint8_t bytes_per_sample = pcm_bit_depth / 8;

	int32_t total_of_all_frames_left = 0;
	int32_t total_of_all_frames_right = 0;

	int16_t temp_frame_sum_left = 0;
	int16_t temp_frame_sum_right = 0;

	if (!is_valid_bit_depth(pcm_bit_depth) || !is_valid_size(input_size, bytes_per_sample, 2)) {
		return -EINVAL;
	}

	char *pointer_input = (char *)input;
	char *pointer_output_left = (char *)output_left;
	char *pointer_output_right = (char *)output_right;

	for (uint32_t i = 0; i < input_size / bytes_per_sample; i += 2) {
		for (uint8_t j = 0; j < bytes_per_sample; j++) {
			temp_frame_sum_left += ((*pointer_input) << 8 * j);
			*pointer_output_left++ = *pointer_input++;
		}
		for (uint8_t j = 0; j < bytes_per_sample; j++) {
			temp_frame_sum_right += ((*pointer_input) << 8 * j);
			*pointer_output_right++ = *pointer_input++;
		}
		total_of_all_frames_left += abs(temp_frame_sum_left);
		total_of_all_frames_right += abs(temp_frame_sum_right);
		temp_frame_sum_left = 0;
		temp_frame_sum_right = 0;
	}
	if (input_size / bytes_per_sample != 0) {

		pcsm_avg_frame_val_left =
			total_of_all_frames_left / (int)(input_size / bytes_per_sample);
		pcsm_avg_frame_val_right =
			total_of_all_frames_right / (int)(input_size / bytes_per_sample);
	} else {
		LOG_ERR("Division by zero occurred");
	}
	*output_size = input_size / 2;
	return 0;
}

int32_t pcsm_avg_frame_val_left_get(void)
{
	return pcsm_avg_frame_val_left;
}
int32_t pcsm_avg_frame_val_right_get(void)
{
	return pcsm_avg_frame_val_right;
}
