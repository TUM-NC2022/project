/*
 * Copyright 2013, 2014		Maurice Leclaire <leclaire@in.tum.de>
 * 				Stephan M. Guenther <moepi@moepi.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * See COPYING for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \defgroup moep_dev Device
 * \brief The Device API contains the functionality used to manage moep devices.
 *
 * This is the generic API available for all moep devices. Specialized functions
 * are provided by the respective module.
 *
 * \{
 * \file
 */
#ifndef __MOEP80211_DEV_H
#define __MOEP80211_DEV_H

#include <stddef.h>

#include <moep/types.h>
#include <moep/frame.h>


struct moep_dev;

/**
 * \brief a moep device
 *
 * This is the opaque representation of a device.
 */
typedef struct moep_dev *moep_dev_t;

/**
 * \brief set the receive status of a moep device
 *
 * The function moep_dev_set_rx_status() is used to set the receive status of a
 * moep device. If the status is 0, the device will not receive any frames. If
 * the status is nonzero, the device will wait for incoming frames.
 *
 * \param dev the moep device
 * \param status the receive status
 *
 * \retval 0 on success
 * \retval -1 on error, errno is set appropriately.
 */
int moep_dev_set_rx_status(moep_dev_t dev, int status);

/**
 * \brief get the transmission status of a device
 *
 * The function moep_dev_get_tx_status() is used to get the transmission status
 * of a moep device, i.e., if it is able to queue frames for transmission.
 *
 * \param dev the moep device
 *
 * \return This function returns nonzero if the device accepts frames for
 * transmission and 0 otherwise.
 */
int moep_dev_get_tx_status(moep_dev_t dev);

/**
 * \brief a device status callback
 *
 * A dev_status_cb() is a function that is called by a moep device if its
 * transmission status changes.
 *
 * \param data user specified data
 * \param status the transmission status of the device
 *
 * \retval 0 on success
 * \retval nonzero on error, errno should be set appropriately.
 */
typedef int (*dev_status_cb)(void *, int);

/**
 * \brief set the transmission status callback of a device
 *
 * The function moep_dev_set_tx_status_cb() is used to register a callback. This
 * callback is called whenever the transmission status
 * (see moep_dev_get_tx_status()) changes. The callback is called with the
 * specified \paramname{data} and the new status.
 *
 * \param cb the callback function
 * \param data the argument for the callback function
 *
 * \retval 0 on success
 * \retval -1 on error, errno is set appropriately.
 */
int moep_dev_set_tx_status_cb(moep_dev_t dev, dev_status_cb cb, void *data);

/**
 * \brief pair off two moep devices
 *
 * The function moep_dev_pair() is used to pair off two moep devices. It sets
 * the receive events of the two devices to the tranmission event of the
 * respective other device.
 *
 * \param dev1 a moep device
 * \param dev2 another moep device
 *
 * \retval 0 on success
 * \retval -1 on error, errno is set appropriately.
 */
int moep_dev_pair(moep_dev_t dev1, moep_dev_t dev2);

/**
 * \brief a receive handler
 *
 * A rx_handler() is a function pointer that is called by moep_select() when a
 * frame was received. The frame must be destroyed when it is no longer needed.
 * The handler is called with signals blocked, as they are blocked when entering
 * moep_select(). When using moep_run() all signals are blocked. A rx_ handler
 * must not block, otherwise a deadlock can occur. For this reason moep_dev_tx()
 * also does not block.
 *
 * \param dev the moep device on which the frame was received
 * \param frame the frame
 *
 * \retval 0 on success
 * \retval nonzero on error, errno should be set appropriately.
 */
typedef int (*rx_handler)(moep_dev_t dev, moep_frame_t frame);

/**
 * \brief return the rx handler of a moep device
 *
 * The function moep_dev_get_rx_handler() is used to get the receive handler of
 * the moep device.
 *
 * \param dev the moep device
 *
 * \return This function returns a function pointer to the rx handler of the
 * moep device.
 */
rx_handler moep_dev_get_rx_handler(moep_dev_t dev);

/**
 * \brief set the rx handler of a moep device
 *
 * The function moep_dev_set_rx_handler() is used to set the receive handler of
 * the moep device.
 *
 * \param dev the moep device
 * \param handler the rx handler
 *
 * \return This function returns the previous rx handler.
 */
rx_handler moep_dev_set_rx_handler(moep_dev_t dev, rx_handler handler);

/**
 * \brief transmit a frame
 *
 * The function moep_dev_tx() is used to transmit a frame through the moep
 * device. This function does not actually transmit the frame, but only puts it
 * into an internal send queue. Be sure to call moep_select() or moep_run()
 * afterwards to schedule the transmission of the frame (except you have already
 * called it, because you are inside a rx handler). This function does not
 * block.
 *
 * \param dev the moep device
 * \param frame the frame
 *
 * \retval 0 on success
 * \retval -1 on error, errno is set appropriately
 *
 * \errors{These are some standard errors generated by this function.
 * Additionally the errors generated by moep_frame_encode() are relevant here.}
 * \errval{ENOMEM, Not enough memory available}
 * \enderrors
 */
int moep_dev_tx(moep_dev_t dev, moep_frame_t frame);

typedef int (*rx_raw_handler)(moep_dev_t dev, u8 *buf, size_t buflen);

rx_raw_handler moep_dev_get_rx_raw_handler(moep_dev_t dev);

rx_raw_handler moep_dev_set_rx_raw_handler(moep_dev_t dev,
					   rx_raw_handler handler);

int moep_dev_tx_raw(moep_dev_t dev, u8 *buf, size_t buflen);

/**
 * \brief create a frame
 *
 * The function moep_dev_frame_create() is used to create a frame that is able
 * to be sent through the device \paramname{dev}.
 *
 * \param dev the moep device
 *
 * \return This function returns a moep frame.
 *
 * \retval NULL on error, errno is set appropriately.
 *
 * \errors{ }
 * \errval{ENOMEM, Not enough memory available}
 * \enderrors
 */
moep_frame_t moep_dev_frame_create(moep_dev_t dev);

/**
 * \brief decode a frame
 *
 * The function moep_dev_frame_decode() decodes a frame from the buffer
 * \paramname{buf} and creates a moep frame with the parsed content. The parsing
 * is done suitable for the frame format specified by \paramname{dev}.
 *
 * \param dev the moep device
 * \param buf the buffer with the frame
 * \param buflen the length of the buffer
 *
 * \return This function returns a moep frame.
 *
 * \retval NULL on error, errno is set appropriately.
 */
moep_frame_t moep_dev_frame_decode(moep_dev_t dev, u8 *buf, size_t buflen);

/**
 * \brief convert a frame
 *
 * The function moep_dev_frame_convert() is used to convert the headers to the
 * format required by the moep device \paramname{dev}. This function does not
 * convert any header data, it only removes the old headers and creates new
 * empty headers in the specified format. This function is useful if you want to
 * convert a frame, without copying the payload.
 *
 * \param dev the moep device
 * \param frame the frame
 */
void moep_dev_frame_convert(moep_dev_t dev, moep_frame_t frame);

/**
 * \brief close a moep device
 *
 * The function moep_dev_close() is used to close a moep device.
 *
 * \param dev the moep device
 */
void moep_dev_close(moep_dev_t dev);

/** \} */
#endif /* __MOEP80211_DEV_H */
