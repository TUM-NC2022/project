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
#include <signal.h>
#include <time.h>

#include <moep/types.h>
#include <moep/system.h>

#include "util.h"


struct moep_callback {
	int fd;
	cb_handler handler;
	void *data;
};

struct sfd_data {
	sig_handler sigh;
	void *data;
};


static int (*moep_epoll_pwait)(int, struct epoll_event *, int, int,
			       const sigset_t *) = epoll_pwait;
static int moep_epfd;


__attribute__((constructor)) static void create_epfd(void) {
	if ((moep_epfd = epoll_create1(0)) < 0) {
		fprintf(stderr, "libmoep initialization error: "
			"Cannot create epoll instance: %s\n", strerror(errno));
		exit(-1);
	}
}

__attribute__((destructor)) static void close_epfd(void) {
	close(moep_epfd);
}

moep_callback_t moep_callback_create(int fd, cb_handler handler, void *data,
				     u32 events)
{
	struct epoll_event event;
	moep_callback_t callback;

	if (!(callback = malloc(sizeof(*callback)))) {
		errno = ENOMEM;
		return NULL;
	}
	callback->fd = fd;
	callback->handler = handler;
	callback->data = data;

	event.events = events;
	event.data.ptr = callback;
	if (epoll_ctl(moep_epfd, EPOLL_CTL_ADD, callback->fd, &event) < 0) {
		free(callback);
		return NULL;
	}

	return callback;
}

int moep_callback_change(moep_callback_t callback, u32 events)
{
	struct epoll_event event;

	event.events = events;
	event.data.ptr = callback;

	return epoll_ctl(moep_epfd, EPOLL_CTL_MOD, callback->fd, &event);
}

int moep_callback_delete(moep_callback_t callback)
{
	if (epoll_ctl(moep_epfd, EPOLL_CTL_DEL, callback->fd, NULL) < 0) {
		return -1;
	}

	free(callback);
	return 0;
}

void moep_set_custom_wait(int (*wait)(int, struct epoll_event *, int, int,
				     const sigset_t *))
{
	moep_epoll_pwait = wait;
}

int moep_wait(int epfd, struct epoll_event *events, int maxevents, int timeout,
	      const sigset_t *sigmask)
{
	struct epoll_event event;
	moep_callback_t data;
	struct timespec ts, tt;
	int t;
	int ret;
	int err;

	t = timeout;
	if (timeout > 0)
		clock_gettime(CLOCK_MONOTONIC, &ts);

	event.events = EPOLLIN | EPOLLONESHOT;
	event.data.ptr = NULL;
	if (epfd >= 0 && epoll_ctl(moep_epfd, EPOLL_CTL_ADD, epfd, &event))
		return -1;

	for (;;) {
		if (timeout > 0) {
			clock_gettime(CLOCK_MONOTONIC, &tt);
			timespecsub(&tt, &ts);
			t = timeout - tt.tv_sec * 1000 - tt.tv_nsec / 1000000;
			if (t < 0)
				t = 0;
		}

		ret = moep_epoll_pwait(moep_epfd, &event, 100, t, sigmask);
		if (ret <= 0) {
			err = errno;
			if (epfd >= 0 &&
			    epoll_ctl(moep_epfd, EPOLL_CTL_DEL, epfd, NULL) &&
			    !ret)
				return -1;
			errno = err;
			return ret;
		}

		data = event.data.ptr;
		if (!data) {
			if (epoll_ctl(moep_epfd, EPOLL_CTL_DEL, epfd, NULL))
				return -1;
			return moep_epoll_pwait(epfd, events, maxevents, 0,
						sigmask);
		}

		if ((ret = data->handler(data->fd, event.events, data->data))) {
			err = errno;
			if (epfd >= 0)
			    epoll_ctl(moep_epfd, EPOLL_CTL_DEL, epfd, NULL);
			errno = err;
			return ret;
		}
	}
}

static int sfd_cb(int fd, u32 events, struct sfd_data *sfd_data)
{
	struct signalfd_siginfo siginfo;
	int ret;

	for (;;) {
		do {
			ret = read(fd, &siginfo, sizeof(siginfo));
		} while (ret < 0 && errno == EINTR);
		if (ret < 0) {
			if (errno == EAGAIN)
				return 0;
			return -1;
		}

		if (sfd_data->sigh &&
		    (ret = sfd_data->sigh(&siginfo, sfd_data->data)))
			return ret;
	}
}

int moep_run(sig_handler sigh, void *data)
{
	sigset_t blockset, oldset, sfdset;
	int sfd;
	struct sfd_data sfd_data;
	moep_callback_t sfd_callback;
	int ret;
	int err;

	sigfillset(&blockset);
	if (sigprocmask(SIG_SETMASK, &blockset, &oldset))
		return -1;

	sigfillset(&sfdset);
	for (sfd = 1; sfd < NSIG; sfd++) {
		if (sigismember(&oldset, sfd))
			sigdelset(&sfdset, sfd);
	}

	if ((sfd = signalfd(-1, &sfdset, SFD_NONBLOCK)) < 0) {
		err = errno;
		sigprocmask(SIG_SETMASK, &oldset, NULL);
		errno = err;
		return -1;
	}

	sfd_data.sigh = sigh;
	sfd_data.data = data;
	if (!(sfd_callback = moep_callback_create(sfd,
						  (cb_handler)sfd_cb, &sfd_data,
						  EPOLLIN))) {
		err = errno;
		close(sfd);
		sigprocmask(SIG_SETMASK, &oldset, NULL);
		errno = err;
		return -1;
	}

	do {
		ret = moep_wait(-1, NULL, 0, -1, NULL);
	} while (ret == 0 || errno == EINTR);

	err = errno;
	moep_callback_delete(sfd_callback);
	close(sfd);
	sigprocmask(SIG_SETMASK, &oldset, NULL);
	errno = err;
	return ret;
}
