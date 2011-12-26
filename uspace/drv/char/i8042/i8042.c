/*
 * Copyright (c) 2001-2004 Jakub Jermar
 * Copyright (c) 2006 Josef Cejka
 * Copyright (c) 2009 Jiri Svoboda
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

/** @addtogroup kbd_port
 * @ingroup kbd
 * @{
 */
/** @file
 * @brief i8042 PS/2 port driver.
 */

#include <ddi.h>
#include <devman.h>
#include <device/hw_res.h>
#include <libarch/ddi.h>
#include <loc.h>
#include <async.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <str_error.h>
#include <inttypes.h>
#include <ddf/log.h>
#include <ddf/interrupt.h>

#include <ops/char_dev.h>

#include "i8042.h"

#define NAME       "i8042"

static int i8042_write_kbd(ddf_fun_t *, char *, size_t);
static int i8042_read_kbd(ddf_fun_t *, char *, size_t);
static int i8042_write_aux(ddf_fun_t *, char *, size_t);
static int i8042_read_aux(ddf_fun_t *, char *, size_t);

static char_dev_ops_t kbd_iface = {
    .read = i8042_read_kbd,
    .write = i8042_write_kbd,
};

static char_dev_ops_t aux_iface = {
    .read = i8042_read_aux,
    .write = i8042_write_aux,
};

static ddf_dev_ops_t kbd_ops = {
	.interfaces[CHAR_DEV_IFACE] = &kbd_iface
};

static ddf_dev_ops_t aux_ops = {
	.interfaces[CHAR_DEV_IFACE] = &aux_iface
};

/* Interesting bits for status register */
#define i8042_OUTPUT_FULL	0x01
#define i8042_INPUT_FULL	0x02
#define i8042_AUX_DATA		0x20

/* Command constants */
#define i8042_CMD_WRITE_CMDB	0x60	/**< write command byte */
#define i8042_CMD_WRITE_AUX	0xd4	/**< write aux device */

/* Command byte fields */
#define i8042_KBD_IE		0x01
#define i8042_AUX_IE		0x02
#define i8042_KBD_DISABLE	0x10
#define i8042_AUX_DISABLE	0x20
#define i8042_KBD_TRANSLATE	0x40 /* Use this to switch to XT scancodes */

static const irq_cmd_t i8042_cmds[] = {
	{
		.cmd = CMD_PIO_READ_8,
		.addr = NULL,	/* will be patched in run-time */
		.dstarg = 1
	},
	{
		.cmd = CMD_BTEST,
		.value = i8042_OUTPUT_FULL,
		.srcarg = 1,
		.dstarg = 3
	},
	{
		.cmd = CMD_PREDICATE,
		.value = 2,
		.srcarg = 3
	},
	{
		.cmd = CMD_PIO_READ_8,
		.addr = NULL,	/* will be patched in run-time */
		.dstarg = 2
	},
	{
		.cmd = CMD_ACCEPT
	}
};
/*----------------------------------------------------------------------------*/
static void wait_ready(i8042_t *dev)
{
	assert(dev);
	while (pio_read_8(&dev->regs->status) & i8042_INPUT_FULL);
}
/*----------------------------------------------------------------------------*/
static void wait_ready_write(i8042_t *dev)
{
	assert(dev);
	while (pio_read_8(&dev->regs->status) & i8042_OUTPUT_FULL);
}
/*----------------------------------------------------------------------------*/
static void i8042_irq_handler(
    ddf_dev_t *dev, ipc_callid_t iid, ipc_call_t *call)
{
	if (!dev || !dev->driver_data)
		return;
	i8042_t *controller = dev->driver_data;
//	fibril_mutex_lock(&controller->guard);

	const uint8_t status = IPC_GET_ARG1(*call);
	const uint8_t data = IPC_GET_ARG2(*call);
	buffer_t *buffer = (status & i8042_AUX_DATA) ?
	    &controller->aux_buffer : &controller->kbd_buffer;
	buffer_write(buffer, data);
#if 0
	char ** buffer =
	    aux ? &controller->aux_buffer : &controller->kbd_buffer;
	char * buffer_end =
	    aux ? controller->aux_buffer_end : controller->kbd_buffer_end;

	if (*buffer != NULL && *buffer < buffer_end) {
		*(*buffer) = data;
		if (++(*buffer) == buffer_end)
			fibril_condvar_broadcast(&controller->data_avail);
	} else {
		ddf_msg(LVL_WARN, "Unhandled %s data: %hhx , status: %hhx.",
		    aux ? "AUX" : "KBD", data, status);
	}

	fibril_mutex_unlock(&controller->guard);
#endif
}
/*----------------------------------------------------------------------------*/
int i8042_init(i8042_t *dev, void *regs, size_t reg_size, int irq_kbd,
    int irq_mouse, ddf_dev_t *ddf_dev)
{
	assert(ddf_dev);
	assert(dev);

	if (reg_size < sizeof(i8042_regs_t))
		return EINVAL;

	if (pio_enable(regs, sizeof(i8042_regs_t), (void**)&dev->regs) != 0)
		return -1;

	dev->kbd_fun = ddf_fun_create(ddf_dev, fun_inner, "ps2a");
	if (!dev->kbd_fun)
		return ENOMEM;
	int ret = ddf_fun_add_match_id(dev->kbd_fun, "xtkbd", 90);
	if (ret != EOK) {
		ddf_fun_destroy(dev->kbd_fun);
		return ret;
	}

	dev->mouse_fun = ddf_fun_create(ddf_dev, fun_inner, "ps2b");
	if (!dev->mouse_fun) {
		ddf_fun_destroy(dev->kbd_fun);
		return ENOMEM;
	}

	ret = ddf_fun_add_match_id(dev->mouse_fun, "ps2mouse", 90);
	if (ret != EOK) {
		ddf_fun_destroy(dev->kbd_fun);
		ddf_fun_destroy(dev->mouse_fun);
		return ret;
	}

	dev->kbd_fun->ops = &kbd_ops;
	dev->mouse_fun->ops = &aux_ops;
	dev->kbd_fun->driver_data = dev;
	dev->mouse_fun->driver_data = dev;

	buffer_init(&dev->kbd_buffer, dev->kbd_data, BUFFER_SIZE);
	buffer_init(&dev->aux_buffer, dev->aux_data, BUFFER_SIZE);
	fibril_mutex_initialize(&dev->write_guard);

#define CHECK_RET_DESTROY(ret, msg...) \
if  (ret != EOK) { \
	ddf_msg(LVL_ERROR, msg); \
	if (dev->kbd_fun) { \
		dev->kbd_fun->driver_data = NULL; \
		ddf_fun_destroy(dev->kbd_fun); \
	} \
	if (dev->mouse_fun) { \
		dev->mouse_fun->driver_data = NULL; \
		ddf_fun_destroy(dev->mouse_fun); \
	} \
} else (void)0

	ret = ddf_fun_bind(dev->kbd_fun);
	CHECK_RET_DESTROY(ret,
	    "Failed to bind keyboard function: %s.\n", str_error(ret));

	ret = ddf_fun_bind(dev->mouse_fun);
	CHECK_RET_DESTROY(ret,
	    "Failed to bind mouse function: %s.\n", str_error(ret));

	/* Disable kbd and aux */
	wait_ready(dev);
	pio_write_8(&dev->regs->status, i8042_CMD_WRITE_CMDB);
	wait_ready(dev);
	pio_write_8(&dev->regs->data, i8042_KBD_DISABLE | i8042_AUX_DISABLE);

	/* Flush all current IO */
	while (pio_read_8(&dev->regs->status) & i8042_OUTPUT_FULL)
		(void) pio_read_8(&dev->regs->data);

#define CHECK_RET_UNBIND_DESTROY(ret, msg...) \
if  (ret != EOK) { \
	ddf_msg(LVL_ERROR, msg); \
	if (dev->kbd_fun) { \
		ddf_fun_unbind(dev->kbd_fun); \
		dev->kbd_fun->driver_data = NULL; \
		ddf_fun_destroy(dev->kbd_fun); \
	} \
	if (dev->mouse_fun) { \
		ddf_fun_unbind(dev->mouse_fun); \
		dev->mouse_fun->driver_data = NULL; \
		ddf_fun_destroy(dev->mouse_fun); \
	} \
} else (void)0

	const size_t cmd_count = sizeof(i8042_cmds) / sizeof(irq_cmd_t);
	irq_cmd_t cmds[cmd_count];
	memcpy(cmds, i8042_cmds, sizeof(i8042_cmds));
	cmds[0].addr = (void *) &dev->regs->status;
	cmds[3].addr = (void *) &dev->regs->data;

	irq_code_t irq_code = { .cmdcount = cmd_count, .cmds = cmds };
	ret = register_interrupt_handler(ddf_dev, irq_kbd, i8042_irq_handler,
	    &irq_code);
	CHECK_RET_UNBIND_DESTROY(ret,
	    "Failed set handler for kbd: %s.\n", str_error(ret));

	ret = register_interrupt_handler(ddf_dev, irq_mouse, i8042_irq_handler,
	    &irq_code);
	CHECK_RET_UNBIND_DESTROY(ret,
	    "Failed set handler for mouse: %s.\n", str_error(ret));

	/* Enable interrupts */
	async_sess_t *parent_sess =
	    devman_parent_device_connect(EXCHANGE_SERIALIZE, ddf_dev->handle,
	    IPC_FLAG_BLOCKING);
	ret = parent_sess ? EOK : ENOMEM;
	CHECK_RET_UNBIND_DESTROY(ret, "Failed to create parent connection.\n");

	const bool enabled = hw_res_enable_interrupt(parent_sess);
	async_hangup(parent_sess);
	ret = enabled ? EOK : EIO;
	CHECK_RET_UNBIND_DESTROY(ret, "Failed to enable interrupts: %s.\n");

	/* Enable port interrupts. */
	wait_ready(dev);
	pio_write_8(&dev->regs->status, i8042_CMD_WRITE_CMDB);
	wait_ready(dev);
	pio_write_8(&dev->regs->data, i8042_KBD_IE | i8042_KBD_TRANSLATE |
	    i8042_AUX_IE);

	return EOK;
}
/*----------------------------------------------------------------------------*/
static int i8042_write_kbd(ddf_fun_t *fun, char *buffer, size_t size)
{
	assert(fun);
	assert(fun->driver_data);
	i8042_t *controller = fun->driver_data;
	fibril_mutex_lock(&controller->write_guard);
	for (size_t i = 0; i < size; ++i) {
		wait_ready_write(controller);
		pio_write_8(&controller->regs->data, buffer[i]);
	}
	fibril_mutex_unlock(&controller->write_guard);
	return size;
}
/*----------------------------------------------------------------------------*/
static int i8042_read_kbd(ddf_fun_t *fun, char *buffer, size_t size)
{
	assert(fun);
	assert(fun->driver_data);
	bzero(buffer, size);

	i8042_t *controller = fun->driver_data;

	for (size_t i = 0; i < size; ++i) {
		*buffer++ = buffer_read(&controller->kbd_buffer);
	}
	return size;
}
/*----------------------------------------------------------------------------*/
static int i8042_write_aux(ddf_fun_t *fun, char *buffer, size_t size)
{
	assert(fun);
	assert(fun->driver_data);
	i8042_t *controller = fun->driver_data;
	fibril_mutex_lock(&controller->write_guard);
	for (size_t i = 0; i < size; ++i) {
		wait_ready_write(controller);
		pio_write_8(&controller->regs->status, i8042_CMD_WRITE_AUX);
		pio_write_8(&controller->regs->data, buffer[i]);
	}
	fibril_mutex_unlock(&controller->write_guard);
	return size;
}
/*----------------------------------------------------------------------------*/
static int i8042_read_aux(ddf_fun_t *fun, char *buffer, size_t size)
{
	assert(fun);
	assert(fun->driver_data);
	bzero(buffer, size);

	i8042_t *controller = fun->driver_data;
	for (size_t i = 0; i < size; ++i) {
		*buffer++ = buffer_read(&controller->aux_buffer);
	}
	return size;
}
/**
 * @}
 */
