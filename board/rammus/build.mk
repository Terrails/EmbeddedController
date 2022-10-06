# -*- makefile -*-
# Copyright 2018 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build
#

CHIP:=npcx
CHIP_VARIANT:=npcx5m6g

board-y=board.o cbi_ssfc.o
board-$(CONFIG_BATTERY_SMART)+=battery.o
board-$(CONFIG_LED_COMMON)+=led.o
board-$(CONFIG_USB_POWER_DELIVERY)+=usb_pd_policy.o
