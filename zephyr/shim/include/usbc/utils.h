/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ZEPHYR_SHIM_USBC_UTIL

/*
 * Enable interrupt from the `irq` property of an instance's node.
 *
 * @param inst: instance number
 */
#define BC12_GPIO_ENABLE_INTERRUPT(inst)             \
	IF_ENABLED(DT_INST_NODE_HAS_PROP(inst, irq), \
		   (gpio_enable_dt_interrupt(        \
			    GPIO_INT_FROM_NODE(DT_INST_PHANDLE(inst, irq)));))

/*
 * Get the port number from a child of `named-usbc-port` node.
 *
 * @param id: node id
 */
#define USBC_PORT(id) DT_REG_ADDR(DT_PARENT(id))

/*
 * Get the port number from a `named-usbc-port` node.
 *
 * @param id: `named-usbc-port` node id
 */
#define USBC_PORT_NEW(id) DT_REG_ADDR(id)

/*
 * Get the port number from a child of `named-usbc-port` node.
 *
 * @param inst: instance number of the node
 */
#define USBC_PORT_FROM_INST(inst) USBC_PORT(DT_DRV_INST(inst))

/*
 * Check that the TCPC interrupt flag defined in the devicetree is the same as
 * the hardware.
 *
 * @param id: node id of the tcpc port
 */
#define TCPC_VERIFY_NO_FLAGS_ACTIVE_ALERT_HIGH(id)                             \
	BUILD_ASSERT(                                                          \
		(DT_PROP(id, tcpc_flags) & TCPC_FLAGS_ALERT_ACTIVE_HIGH) == 0, \
		"TCPC interrupt configuration error for " DT_NODE_FULL_NAME(   \
			id));

/**
 * @brief Macros used to process USB-C driver organized as a
 * (compatible, config) tuple.  Where "compatible" is the devictree compatible
 * string and "config" is the macro used to initialize the USB-C driver
 * instance.
 *
 * The "config" macro has a single parameter, the devicetree node ID.
 */
/**
 * @brief Get compatible from @p driver
 *
 * @param driver USB mux driver description in format (compatible, config)
 */
#define USB_MUX_DRIVER_GET_COMPAT(driver) GET_ARG_N(1, __DEBRACKET driver)

/**
 * @brief Get configuration from @p driver
 *
 * @param driver USB mux driver description in format (compatible, config)
 */
#define USB_MUX_DRIVER_GET_CONFIG(driver) GET_ARG_N(2, __DEBRACKET driver)

/**
 * @brief Call @p op operation for each node that is compatible with @p driver
 *
 * @param driver USB mux driver description in format (compatible, config)
 * @param op Operation to perform on each USB mux. Should accept mux node ID and
 *           driver config as arguments.
 */
#define USB_MUX_DRIVER_CONFIG(driver, op)                                   \
	DT_FOREACH_STATUS_OKAY_VARGS(USB_MUX_DRIVER_GET_COMPAT(driver), op, \
				     USB_MUX_DRIVER_GET_CONFIG(driver))

/**
 * @brief Call @p op operation for each USB mux node that is compatible with
 *        any driver from the USB_MUX_DRIVERS list.
 *        DT_FOREACH_STATUS_OKAY_VARGS() macro can not be used in @p op
 *
 * @param op Operation to perform on each USB mux. Should accept mux node ID and
 *           driver config as arguments.
 */
#define USB_MUX_FOREACH_MUX_DT_VARGS(op) \
	FOR_EACH_FIXED_ARG(USB_MUX_DRIVER_CONFIG, (), op, USB_MUX_DRIVERS)

#endif /* __CROS_EC_ZEPHYR_SHIM_USBC_UTIL */
