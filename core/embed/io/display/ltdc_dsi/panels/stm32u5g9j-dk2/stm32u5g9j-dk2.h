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

#pragma once

#include <trezor_types.h>

// RK050HR18 on STM32U5G9J-DK2: 24-bit RGB parallel interface driven
// directly by LTDC, there is no DSI host in the path.
#define PANEL_LTDC_PARALLEL 1

#define REFRESH_RATE_SCALING_SUPPORTED 0

// LTDC PLL3 configuration
// LTDC_PIXEL_CLOCK_HZ = ((HSE_VALUE / PLL3_M) * PLL3_N) / PLL3_R
#define LTDC_PIXEL_CLOCK_HZ 25000000UL  // Output of PLL3R
// 4MHz is used as PLL3 block input clock
#define PLL3_M (HSE_VALUE / 4000000UL)
#define PLL3_N 125
#define PLL3_P 20
#define PLL3_Q 20  // Not used output clock branch
#define PLL3_R 20

// Display timing parameters (~60Hz at 25MHz pixel clock)
#define HSYNC 5   // Horizontal Sync
#define HBP 8     // Horizontal Back Porch
#define HACT 800  // Horizontal Active Time
#define HFP 8     // Horizontal Front Porch

#define VSYNC 5   // Vertical Sync
#define VBP 8     // Vertical Back Porch
#define VACT 480  // Vertical Active Time
#define VFP 14    // Vertical Front Porch

#define PANEL_LTDC_PIXEL_FORMAT LTDC_PIXEL_FORMAT_RGB565

#define LCD_WIDTH 800
#define LCD_HEIGHT 480

// The portrait UI (DISPLAY_RESX x DISPLAY_RESY) is shown rotated by 90 degrees
// (board turned clockwise, connectors at the bottom) and enlarged by
// DISPLAY_UPSCALE_NUM / DISPLAY_UPSCALE_DEN (board header). The UI draws into
// frame buffer 1; display_refresh() rotates and scales it into frame buffer 0,
// which is the only buffer scanned by LTDC.
#define PANEL_ROTATED_90 1

// LTDC layer: the rotated, enlarged UI, centered on the panel
#define LAYER_WIDTH (DISPLAY_RESY * DISPLAY_UPSCALE_NUM / DISPLAY_UPSCALE_DEN)
#define LAYER_HEIGHT (DISPLAY_RESX * DISPLAY_UPSCALE_NUM / DISPLAY_UPSCALE_DEN)
#define LCD_X_OFFSET ((LCD_WIDTH - LAYER_WIDTH) / 2)
#define LCD_Y_OFFSET ((LCD_HEIGHT - LAYER_HEIGHT) / 2)

// Size of the physical frame buffer in bytes (RGB565), big enough for both
// the LTDC layer and the frame the UI draws into
#define PHYSICAL_FRAME_BUFFER_SIZE (LAYER_WIDTH * LAYER_HEIGHT * 2)

// Pitch (in pixels) of the frame buffer the UI draws into
#define FRAME_BUFFER_PIXELS_PER_LINE DISPLAY_RESX

// Size of the virtual frame buffer in bytes
#define VIRTUAL_FRAME_BUFFER_SIZE PHYSICAL_FRAME_BUFFER_SIZE
