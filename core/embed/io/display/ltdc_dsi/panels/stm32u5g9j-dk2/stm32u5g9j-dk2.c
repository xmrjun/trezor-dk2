/*
 * This file is part of the Trezor project, https://trezor.io/
 *
 * Copyright (c) SatoshiLabs
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <trezor_bsp.h>
#include <trezor_rtl.h>

#ifdef KERNEL_MODE

#include <sys/systick.h>

#include "../../display_internal.h"

typedef struct {
  GPIO_TypeDef* port;
  uint16_t pins;
  uint8_t af;
} panel_pin_group_t;

// LTDC signals of the RGB interface (R0-R7, G0-G7, B0-B7, DE, CLK, HSYNC,
// VSYNC); PB9, PC6 and PC7 are on AF7, the rest on AF8.
static const panel_pin_group_t panel_ltdc_pins[] = {
    {GPIOB, GPIO_PIN_9, GPIO_AF7_LTDC},
    {GPIOB, GPIO_PIN_2, GPIO_AF8_LTDC},
    {GPIOC, GPIO_PIN_6 | GPIO_PIN_7, GPIO_AF7_LTDC},
    {GPIOC, GPIO_PIN_8 | GPIO_PIN_9, GPIO_AF8_LTDC},
    {GPIOD,
     GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_8 |
         GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
         GPIO_PIN_14 | GPIO_PIN_15,
     GPIO_AF8_LTDC},
    {GPIOE,
     GPIO_PIN_0 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
         GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15,
     GPIO_AF8_LTDC},
};

bool panel_init(display_driver_t* drv) {
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStructure = {0};
  GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStructure.Pull = GPIO_NOPULL;
  GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;

  for (size_t i = 0; i < ARRAY_LENGTH(panel_ltdc_pins); i++) {
    GPIO_InitStructure.Pin = panel_ltdc_pins[i].pins;
    GPIO_InitStructure.Alternate = panel_ltdc_pins[i].af;
    HAL_GPIO_Init(panel_ltdc_pins[i].port, &GPIO_InitStructure);
  }

  // Switch the panel on (LCD_ON)
  DISPLAY_ENABLE_CLK_ENA();
  HAL_GPIO_WritePin(DISPLAY_ENABLE_PORT, DISPLAY_ENABLE_PIN, GPIO_PIN_SET);
  GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStructure.Alternate = 0;
  GPIO_InitStructure.Pin = DISPLAY_ENABLE_PIN;
  HAL_GPIO_Init(DISPLAY_ENABLE_PORT, &GPIO_InitStructure);

  return true;
}

void panel_deinit(display_driver_t* drv) {
  HAL_GPIO_WritePin(DISPLAY_ENABLE_PORT, DISPLAY_ENABLE_PIN, GPIO_PIN_RESET);

  for (size_t i = 0; i < ARRAY_LENGTH(panel_ltdc_pins); i++) {
    HAL_GPIO_DeInit(panel_ltdc_pins[i].port, panel_ltdc_pins[i].pins);
  }
}

#ifdef USE_SUSPEND
bool panel_suspend(display_driver_t* drv) {
  panel_deinit(drv);
  return true;
}
#endif  // USE_SUSPEND

#endif  // KERNEL_MODE
