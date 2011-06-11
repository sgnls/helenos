/*
 * Copyright (c) 2011 Jan Vesely
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup libusbhost
 * @{
 */
/** @file
 * Device keeper structure and functions.
 *
 * Typical USB host controller needs to keep track of various settings for
 * each device that is connected to it.
 * State of toggle bit, device speed etc. etc.
 * This structure shall simplify the management.
 */
#ifndef LIBUSBHOST_HOST_DEVICE_KEEPER_H
#define LIBUSBHOST_HOST_DEVICE_KEEPER_H

#include <adt/list.h>
#include <devman.h>
#include <fibril_synch.h>
#include <usb/usb.h>
#include <usb/host/endpoint.h>

/** Number of USB address for array dimensions. */
#define USB_ADDRESS_COUNT (USB11_ADDRESS_MAX + 1)

/** Information about attached USB device. */
struct usb_device_info {
	usb_speed_t speed;
	bool occupied;
	devman_handle_t handle;
};

/** Host controller device keeper.
 * You shall not access members directly but only using functions below.
 */
typedef struct {
	struct usb_device_info devices[USB_ADDRESS_COUNT];
	fibril_mutex_t guard;
	usb_address_t last_address;
} usb_device_keeper_t;

void usb_device_keeper_init(usb_device_keeper_t *instance);

usb_address_t device_keeper_get_free_address(usb_device_keeper_t *instance,
    usb_speed_t speed);

void usb_device_keeper_bind(usb_device_keeper_t *instance,
    usb_address_t address, devman_handle_t handle);

void usb_device_keeper_release(usb_device_keeper_t *instance,
    usb_address_t address);

usb_address_t usb_device_keeper_find(usb_device_keeper_t *instance,
    devman_handle_t handle);

bool usb_device_keeper_find_by_address(usb_device_keeper_t *instance,
    usb_address_t address, devman_handle_t *handle);

usb_speed_t usb_device_keeper_get_speed(usb_device_keeper_t *instance,
    usb_address_t address);
#endif
/**
 * @}
 */