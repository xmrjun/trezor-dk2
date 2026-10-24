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

// Goodix GT911 capacitive touch controller as used on STM32U5G9J-DK2
// (5" 800x480 panel). The UI is shown in the middle of the panel, so touch
// coordinates are translated into that window. A portrait UI is shown rotated
// by 90 degrees (see the stm32u5g9j-dk2 display panel).

#include <trezor_bsp.h>
#include <trezor_rtl.h>

#ifdef KERNEL_MODE

#include <io/touch.h>
#include <sys/i2c_bus.h>
#include <sys/systick.h>

#include "../touch_poll.h"

// 7-bit address selected by holding INT low while leaving reset
#define GT911_I2C_ADDR 0x5D

#define GT911_REG_PRODUCT_ID 0x8140
#define GT911_REG_FW_VERSION 0x8144
#define GT911_REG_STATUS 0x814E
#define GT911_REG_POINT1 0x8150

#define GT911_STATUS_READY 0x80
#define GT911_STATUS_TOUCHES 0x0F

#define GT911_PANEL_WIDTH 800
#define GT911_PANEL_HEIGHT 480
#ifndef DISPLAY_UPSCALE_NUM
#define DISPLAY_UPSCALE_NUM 1
#define DISPLAY_UPSCALE_DEN 1
#endif

#if DISPLAY_RESY > DISPLAY_RESX
#define GT911_ROTATED_90 1
// Size of the rotated, enlarged UI on the panel
#define GT911_LAYER_WIDTH (DISPLAY_RESY * DISPLAY_UPSCALE_NUM / DISPLAY_UPSCALE_DEN)
#define GT911_LAYER_HEIGHT \
  (DISPLAY_RESX * DISPLAY_UPSCALE_NUM / DISPLAY_UPSCALE_DEN)
#define GT911_WINDOW_X ((GT911_PANEL_WIDTH - GT911_LAYER_WIDTH) / 2)
#define GT911_WINDOW_Y ((GT911_PANEL_HEIGHT - GT911_LAYER_HEIGHT) / 2)
#else
#define GT911_WINDOW_X ((GT911_PANEL_WIDTH - DISPLAY_RESX) / 2)
#define GT911_WINDOW_Y ((GT911_PANEL_HEIGHT - DISPLAY_RESY) / 2)
#endif

typedef struct {
  // Set if the driver is initialized
  secbool initialized;
  // I2C bus where the touch controller is connected
  i2c_bus_t* i2c_bus;
  // Time (in ticks) when the touch registers were read last time
  uint32_t read_ticks;
  // Last reported touch state
  uint32_t state;

} touch_driver_t;

// Touch driver instance
static touch_driver_t g_touch_driver = {
    .initialized = secfalse,
};

// Reads subsequent registers starting at the 16-bit register address `reg`.
static secbool gt911_read_regs(i2c_bus_t* bus, uint16_t reg, uint8_t* value,
                               size_t count) {
  i2c_op_t ops[] = {
      {
          .flags = I2C_FLAG_TX | I2C_FLAG_EMBED,
          .size = 2,
          .data = {reg >> 8, reg & 0xFF},
      },
      {
          .flags = I2C_FLAG_RX,
          .size = count,
          .ptr = value,
      },
  };

  i2c_packet_t pkt = {
      .address = GT911_I2C_ADDR,
      .op_count = ARRAY_LENGTH(ops),
      .ops = ops,
  };

  if (I2C_STATUS_OK != i2c_bus_submit_and_wait(bus, &pkt)) {
    return secfalse;
  }

  return sectrue;
}

// Writes a single 8-bit register at the 16-bit register address `reg`.
static secbool gt911_write_reg(i2c_bus_t* bus, uint16_t reg, uint8_t value) {
  i2c_op_t ops[] = {
      {
          .flags = I2C_FLAG_TX | I2C_FLAG_EMBED,
          .size = 3,
          .data = {reg >> 8, reg & 0xFF, value},
      },
  };

  i2c_packet_t pkt = {
      .address = GT911_I2C_ADDR,
      .op_count = ARRAY_LENGTH(ops),
      .ops = ops,
  };

  if (I2C_STATUS_OK != i2c_bus_submit_and_wait(bus, &pkt)) {
    return secfalse;
  }

  return sectrue;
}

// Resets the controller and selects the 0x5D I2C address
// (INT held low during the rising edge of RST).
static void gt911_reset(void) {
  TOUCH_RST_CLK_ENA();
  TOUCH_INT_CLK_ENA();

  GPIO_InitTypeDef GPIO_InitStructure = {0};
  GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStructure.Pull = GPIO_NOPULL;
  GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;

  HAL_GPIO_WritePin(TOUCH_RST_PORT, TOUCH_RST_PIN, GPIO_PIN_RESET);
  GPIO_InitStructure.Pin = TOUCH_RST_PIN;
  HAL_GPIO_Init(TOUCH_RST_PORT, &GPIO_InitStructure);

  HAL_GPIO_WritePin(TOUCH_INT_PORT, TOUCH_INT_PIN, GPIO_PIN_RESET);
  GPIO_InitStructure.Pin = TOUCH_INT_PIN;
  HAL_GPIO_Init(TOUCH_INT_PORT, &GPIO_InitStructure);

  systick_delay_ms(10);
  HAL_GPIO_WritePin(TOUCH_RST_PORT, TOUCH_RST_PIN, GPIO_PIN_SET);
  systick_delay_ms(10);

  // Release INT, the controller drives it from now on
  GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
  GPIO_InitStructure.Pin = TOUCH_INT_PIN;
  HAL_GPIO_Init(TOUCH_INT_PORT, &GPIO_InitStructure);

  // Wait for the controller to boot up
  systick_delay_ms(60);
}

static void gt911_power_down(void) {
  HAL_GPIO_WritePin(TOUCH_RST_PORT, TOUCH_RST_PIN, GPIO_PIN_RESET);
  HAL_GPIO_DeInit(TOUCH_INT_PORT, TOUCH_INT_PIN);
}

static uint16_t gt911_clamp(int value, int max) {
  if (value < 0) {
    return 0;
  }
  if (value > max) {
    return max;
  }
  return value;
}

secbool touch_init(void) {
  touch_driver_t* driver = &g_touch_driver;

  if (sectrue == driver->initialized) {
    // The driver is already initialized
    return sectrue;
  }

  memset(driver, 0, sizeof(touch_driver_t));

  gt911_reset();

  driver->i2c_bus = i2c_bus_open(TOUCH_I2C_INSTANCE);
  if (driver->i2c_bus == NULL) {
    goto cleanup;
  }

  // Product ID is the ASCII string "911"
  uint8_t id[4] = {0};
  if (sectrue !=
      gt911_read_regs(driver->i2c_bus, GT911_REG_PRODUCT_ID, id, sizeof(id))) {
    goto cleanup;
  }

  if (id[0] != '9' || id[1] != '1' || id[2] != '1') {
    goto cleanup;
  }

  // Drop any stale data
  if (sectrue != gt911_write_reg(driver->i2c_bus, GT911_REG_STATUS, 0)) {
    goto cleanup;
  }

  if (!touch_poll_init()) {
    goto cleanup;
  }

  driver->read_ticks = systick_ms();
  driver->initialized = sectrue;

  return sectrue;

cleanup:
  touch_deinit();
  return secfalse;
}

void touch_deinit(void) {
  touch_driver_t* driver = &g_touch_driver;
  touch_poll_deinit();
  i2c_bus_close(driver->i2c_bus);
  if (sectrue == driver->initialized) {
    gt911_power_down();
  }
  memset(driver, 0, sizeof(touch_driver_t));
}

void touch_power_set(bool on) {
  if (on) {
    gt911_reset();
  } else {
    touch_deinit();
  }
}

secbool touch_ready(void) {
  touch_driver_t* driver = &g_touch_driver;
  return driver->initialized;
}

secbool touch_set_sensitivity(uint8_t value) {
  // Not implemented for the discovery kit
  return sectrue;
}

uint8_t touch_get_version(void) {
  touch_driver_t* driver = &g_touch_driver;

  if (sectrue != driver->initialized) {
    return 0;
  }

  uint8_t version[2] = {0};
  if (sectrue != gt911_read_regs(driver->i2c_bus, GT911_REG_FW_VERSION,
                                 version, sizeof(version))) {
    return 0;
  }

  return version[0];
}

secbool touch_activity(void) {
  touch_driver_t* driver = &g_touch_driver;

  if (sectrue != driver->initialized) {
    return secfalse;
  }

  uint8_t status = 0;
  if (sectrue !=
      gt911_read_regs(driver->i2c_bus, GT911_REG_STATUS, &status, 1)) {
    return secfalse;
  }

  return ((status & GT911_STATUS_READY) && (status & GT911_STATUS_TOUCHES))
             ? sectrue
             : secfalse;
}

uint32_t touch_get_state(void) {
  touch_driver_t* driver = &g_touch_driver;


  if (sectrue != driver->initialized) {
    return 0;
  }

  uint32_t ticks = hal_ticks_ms();

  // The controller reports at ~100Hz, polling faster is useless
  if ((int32_t)(ticks - driver->read_ticks) < 10 /* ms */) {
    return driver->state;
  }

  driver->read_ticks = ticks;

  uint8_t status = 0;
  if (sectrue !=
      gt911_read_regs(driver->i2c_bus, GT911_REG_STATUS, &status, 1)) {
    return driver->state;
  }

  if ((status & GT911_STATUS_READY) == 0) {
    // No new data since the last read
    return driver->state;
  }

  uint8_t nb_touches = status & GT911_STATUS_TOUCHES;
  uint8_t point[4] = {0};

  if (nb_touches > 0) {
    if (sectrue != gt911_read_regs(driver->i2c_bus, GT911_REG_POINT1, point,
                                   sizeof(point))) {
      nb_touches = 0;
    }
  }

  // Acknowledge the data so the controller can report the next frame
  gt911_write_reg(driver->i2c_bus, GT911_REG_STATUS, 0);

  uint32_t state = driver->state;

  if (nb_touches > 0) {
    int x_raw = point[0] | (point[1] << 8);
    int y_raw = point[2] | (point[3] << 8);

#ifdef GT911_ROTATED_90
    // Layer pixel (lx, ly) shows UI pixel
    // (DISPLAY_RESX - 1 - ly * DEN / NUM, lx * DEN / NUM)
    int lx = gt911_clamp(x_raw - GT911_WINDOW_X, GT911_LAYER_WIDTH - 1);
    int ly = gt911_clamp(y_raw - GT911_WINDOW_Y, GT911_LAYER_HEIGHT - 1);
    uint16_t x = gt911_clamp(
        DISPLAY_RESX - 1 - ly * DISPLAY_UPSCALE_DEN / DISPLAY_UPSCALE_NUM,
        DISPLAY_RESX - 1);
    uint16_t y = gt911_clamp(lx * DISPLAY_UPSCALE_DEN / DISPLAY_UPSCALE_NUM,
                             DISPLAY_RESY - 1);
#else
    uint16_t x = gt911_clamp(x_raw - GT911_WINDOW_X, DISPLAY_RESX - 1);
    uint16_t y = gt911_clamp(y_raw - GT911_WINDOW_Y, DISPLAY_RESY - 1);
#endif

    uint32_t xy = touch_pack_xy(x, y);

    if ((driver->state & TOUCH_END) || (driver->state == 0)) {
      state = TOUCH_START | xy;
    } else {
      state = TOUCH_MOVE | xy;
    }
  } else {
    if (driver->state & (TOUCH_START | TOUCH_MOVE)) {
      state = (driver->state & ~(TOUCH_START | TOUCH_MOVE)) | TOUCH_END;
    }
  }

  driver->state = state;
  return state;
}

#endif  // KERNEL_MODE
