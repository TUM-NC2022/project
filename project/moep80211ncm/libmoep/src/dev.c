/*
 * Copyright 2013, 2014		Maurice Leclaire <leclaire@in.tum.de>
 *				Stephan M. Guenther <moepi@moepi.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <moep/system.h>
#include <moep/frame.h>
#include <moep/dev.h>
#include <moep/module.h>

#include "list.h"


#define assert_module(dev, ops, ret)		\
	if ((dev)->ops.close != (ops)->close) {	\
		errno = EACCES;			\
		return ret;			\
	}


struct moep_dev {
	struct list_head list;
	int fd;
	int mtu;
	struct moep_dev_ops ops;
	void *priv;
	struct moep_frame_ops l1_ops;
	struct moep_frame_ops l2_ops;
	struct list_head tx_queue;
	dev_status_cb tx_status_cb;
	void *tx_status_cb_data;
	int rx_status;
	moep_callback_t dev_cb;
	rx_handler rx;
	rx_raw_handler rx_raw;
};

struct frame {
	struct list_head list;
	u8 *data;
	int len;
};


LIST_HEAD(moep_dev_list);


static int configure_callback(moep_dev_t dev)
{
	u32 events;

	events = EPOLLONESHOT;
	if (dev->rx_status)
		events |= EPOLLIN;
	if (!list_empty(&dev->tx_queue))
		events |= EPOLLOUT;
	return moep_callback_change(dev->dev_cb, events);
}

static int trigger_tx_status_cb(moep_dev_t dev)
{
	if (dev->tx_status_cb && dev->tx_status_cb(dev->tx_status_cb_data,
						   moep_dev_get_tx_status(dev)))
		return -1;
	return 0;
}

static int rx_cb(moep_dev_t dev)
{
	moep_frame_t frame;
	u8 *data;
	int len;

	if (!(data = malloc(dev->mtu))) {
		errno = ENOMEM;
		return -1;
	}

	while (dev->rx_status) {
		do {
			len = read(dev->fd, data, dev->mtu);
		} while (len < 0 && errno == EINTR);
		if (len < 0) {
			free(data);
			if (errno != EAGAIN && errno != EWOULDBLOCK)
				return -1;
			return 0;
		}

		if (dev->rx_raw && dev->rx_raw(dev, data, len)) {
			free(data);
			return -1;
		}

		if (dev->rx) {
			if (!(frame = moep_dev_frame_decode(dev, data, len))) {
				free(data);
				if (errno != EINVAL)
					return -1;
				return 0;
			}
			if (dev->rx(dev, frame)) {
				free(data);
				return -1;
			}
		}
	}

	free(data);
	return 0;
}

static int tx_cb(moep_dev_t dev)
{
	struct frame *f, *tmp;
	int ret;

	list_for_each_entry_safe(f, tmp, &dev->tx_queue, list) {
		do {
			ret = write(dev->fd, f->data, f->len);
		} while (ret < 0 && errno == EINTR);
		if (ret < 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				list_del(&f->list);
				free(f->data);
				free(f);
				return -1;
			}
			return 0;
		}
		if (ret != f->len) {
			list_del(&f->list);
			free(f->data);
			free(f);
			return -1;
		}
		list_del(&f->list);
		free(f->data);
		free(f);
	}

	return trigger_tx_status_cb(dev);
}

static int dev_cb(int fd, u32 events, moep_dev_t dev)
{
	int ret;

	if (events & EPOLLERR)
		events |= EPOLLIN;
	if (events & EPOLLOUT) {
		if ((ret = tx_cb(dev)))
			return ret;
	}
	if (events & EPOLLIN) {
		if ((ret = rx_cb(dev)))
			return ret;
	}
	return configure_callback(dev);
}

moep_dev_t moep_dev_open(int fd, int mtu, struct moep_dev_ops *ops, void *priv,
			 struct moep_frame_ops *l1_ops,
			 struct moep_frame_ops *l2_ops)
{
	moep_dev_t dev;
	int err;

	if (fd < 0) {
		errno = EINVAL;
		return NULL;
	}
	if (mtu <= 0) {
		errno = EINVAL;
		return NULL;
	}

	if (!(dev = malloc(sizeof(*dev)))) {
		errno = ENOMEM;
		return NULL;
	}
	memset(dev, 0, sizeof(*dev));

	dev->fd = fd;
	dev->mtu = mtu;
	if (ops)
		dev->ops = *ops;
	dev->priv = priv;
	if (l1_ops)
		dev->l1_ops = *l1_ops;
	if (l2_ops)
		dev->l2_ops = *l2_ops;

	INIT_LIST_HEAD(&dev->tx_queue);
	dev->tx_status_cb = NULL;
	dev->rx_status = 0;
	if (!(dev->dev_cb = moep_callback_create(fd, (cb_handler)dev_cb, dev,
						 0))) {
		err = errno;
		free(dev);
		errno = err;
		return NULL;
	}

	list_add(&dev->list, &moep_dev_list);

	return dev;
}

void *moep_dev_get_priv(moep_dev_t dev, struct moep_dev_ops *ops)
{
	assert_module(dev, ops, NULL);

	return dev->priv;
}

int moep_dev_set_rx_status(moep_dev_t dev, int status)
{
	dev->rx_status = status;
	return configure_callback(dev);
}

int moep_dev_get_tx_status(moep_dev_t dev)
{
	return list_empty(&dev->tx_queue);
}

int moep_dev_set_tx_status_cb(moep_dev_t dev, dev_status_cb cb, void *data)
{
	dev->tx_status_cb = cb;
	dev->tx_status_cb_data = data;
	return trigger_tx_status_cb(dev);
}

int moep_dev_pair(moep_dev_t dev1, moep_dev_t dev2)
{
	if (moep_dev_set_tx_status_cb(dev1,
				      (dev_status_cb)moep_dev_set_rx_status,
				      dev2))
		return -1;
	if (moep_dev_set_tx_status_cb(dev2,
				      (dev_status_cb)moep_dev_set_rx_status,
				      dev1))
		return -1;
	return 0;
}

static int queue_frame(moep_dev_t dev, struct frame *f)
{
	list_add_tail(&f->list, &dev->tx_queue);

	if (trigger_tx_status_cb(dev))
		return -1;
	return configure_callback(dev);
}

rx_handler moep_dev_get_rx_handler(moep_dev_t dev)
{
	return dev->rx;
}

rx_handler moep_dev_set_rx_handler(moep_dev_t dev, rx_handler handler)
{
	rx_handler old;

	old = dev->rx;
	dev->rx = handler;
	return old;
}

int moep_dev_tx(moep_dev_t dev, moep_frame_t frame)
{
	struct frame *f;

	if (!(f = malloc(sizeof(*f)))) {
		errno = ENOMEM;
		return -1;
	}

	f->data = NULL;
	if ((f->len = moep_frame_encode(frame, &f->data, dev->mtu)) < 0) {
		free(f);
		return -1;
	}

	return queue_frame(dev, f);
}

rx_raw_handler moep_dev_get_rx_raw_handler(moep_dev_t dev)
{
	return dev->rx_raw;
}

rx_raw_handler moep_dev_set_rx_raw_handler(moep_dev_t dev,
					   rx_raw_handler handler)
{
	rx_raw_handler old;

	old = dev->rx_raw;
	dev->rx_raw = handler;
	return old;
}

int moep_dev_tx_raw(moep_dev_t dev, u8 *buf, size_t buflen)
{
	struct frame *f;

	if (buflen > dev->mtu) {
		errno = EMSGSIZE;
		return -1;
	}

	if (!(f = malloc(sizeof(*f)))) {
		errno = ENOMEM;
		return -1;
	}
	if (!(f->data = malloc(buflen))) {
		free(f);
		errno = ENOMEM;
		return -1;
	}

	memcpy(f->data, buf, buflen);
	f->len = buflen;

	return queue_frame(dev, f);
}

moep_frame_t moep_dev_frame_create(moep_dev_t dev)
{
	return moep_frame_create(&dev->l1_ops, &dev->l2_ops);
}

moep_frame_t moep_dev_frame_decode(moep_dev_t dev, u8 *buf, size_t buflen)
{
	moep_frame_t frame;

	if (!(frame = moep_dev_frame_create(dev)))
		return NULL;
	if (moep_frame_decode(frame, buf, buflen)) {
		moep_frame_destroy(frame);
		return NULL;
	}
	return frame;
}

void moep_dev_frame_convert(moep_dev_t dev, moep_frame_t frame)
{
	moep_frame_convert(frame, &dev->l1_ops, &dev->l2_ops);
}

void moep_dev_close(moep_dev_t dev)
{
	struct frame *f, *tmp;

	list_for_each_entry_safe(f, tmp, &dev->tx_queue, list) {
		list_del(&f->list);
		free(f);
	}
	list_del(&dev->list);
	moep_callback_delete(dev->dev_cb);
	if (dev->ops.close)
		dev->ops.close(dev->fd, dev->priv);
	free(dev);
}
