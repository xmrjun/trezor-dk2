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

// Power manager backend for boards permanently powered from USB without a
// battery or PMIC (STM32U5G9J-DK2). It always reports external power and a
// full battery; the board cannot switch itself off.

#include <trezor_rtl.h>

#include <io/power_manager.h>

#include "../power_manager_poll.h"

#ifdef KERNEL_MODE

pm_status_t pm_init(bool inherit_state) {
  if (!pm_poll_init()) {
    return PM_ERROR;
  }
  return PM_OK;
}

void pm_deinit(void) { pm_poll_deinit(); }

pm_status_t pm_hibernate(void) { return PM_ERROR; }

pm_status_t pm_suspend(wakeup_flags_t* wakeup_reason) {
  return PM_ERROR;
}

pm_status_t pm_turn_on(void) { return PM_OK; }
pm_status_t pm_charging_enable(void) { return PM_OK; }
pm_status_t pm_charging_disable(void) { return PM_OK; }

pm_status_t pm_get_state(pm_state_t* state) {
  memset(state, 0, sizeof(*state));
  state->usb_connected = true;
  state->battery_connected = false;
  state->ntc_connected = false;
  state->charging_status = PM_BATTERY_IDLE;
  state->power_status = PM_STATE_ACTIVE;
  state->soc = 100;
  return PM_OK;
}

bool pm_is_charging(void) { return false; }

bool pm_usb_is_connected(void) { return true; }

pm_status_t pm_set_soc_target(uint8_t target) { return PM_OK; }

#endif  // KERNEL_MODE
