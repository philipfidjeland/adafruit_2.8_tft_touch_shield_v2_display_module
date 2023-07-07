/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _TAB_MENU_H_
#define _TAB_MENU_H_

/**
 * @brief Create all objects on the button tab
 *
 * @param current_screen Parent object of the button tab
 */
void tab_menu_create(lv_obj_t *current_screen);
/**
 * @brief Updates all objects on the button tab
 */
uint8_t tab_menu_update(uint8_t update_counter);

#endif /* _TAB_MENU_H_ */
