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
 * \defgroup moep_system System
 * \brief The System API is used to operate the moep system.
 *
 * \{
 * \file
 */
#ifndef __MOEP80211_SYSTEM_H
#define __MOEP80211_SYSTEM_H

#include <sys/epoll.h>
#include <sys/signalfd.h>

#include <moep/types.h>


struct moep_callback;

/**
 * \brief a moep callback
 *
 * This is the opaque representation of a callback.
 */
typedef struct moep_callback *moep_callback_t;

/**
 * \brief a callback handler
 *
 * A cb_handler() is a function pointer that is called by moep_wait() if the
 * corresponding I/O event occurred.
 *
 * \param fd the file descriptor on which the event occurred
 * \param events the event(s)
 * \param data user specified data
 *
 * \return The cb_handler() must either return 0, or moep_wait() returns with
 * this return value.
 */
typedef int (*cb_handler)(int, u32, void *);

/**
 * \brief create a new moep callback
 *
 * The moep_callback_create() call is used to create a new moep callback.
 * Whenever the I/O events specified in \paramname{events} occur on the file
 * descriptor \paramname{fd} the callback function \paramname{handler} is called
 * with \paramname{data} as argument. The \paramname{events} are the same as for
 * epoll_ctl(). See the respective manual page for details. To make the callback
 * happen a call to moep_wait() or better moep_run() is necessary.
 *
 * \param fd the file descriptor to be watched
 * \param handler the callback handler
 * \param the argument for handler
 * \param events the events to wait for
 */
moep_callback_t moep_callback_create(int fd, cb_handler handler, void *data,
				     u32 events);

/**
 * \brief change a moep callback
 *
 * The moep_callback_change() call is used to cahnge the events the callback
 * should wait for. The \paramname{events} are the same as for epoll_ctl(). See
 * the respective manual page for details.
 *
 * \param callback the moep callback
 * \param events the events to wait for
 */
int moep_callback_change(moep_callback_t callback, u32 events);

/**
 * \brief delete a moep callback
 *
 * The moep_callback_delete() call is used to delete a moep callback.
 *
 * \param callback the moep callback
 */
int moep_callback_delete(moep_callback_t callback);

/**
 * \brief set a custom wait call
 *
 * The moep_set_custom_wait() call is used to customize the moep_wait() call.
 * Normally, moep_wait() uses epoll_pwait() internally. With this call you can
 * specify a custom wait call wich should be used instead.
 *
 * \warning
 * You need very good reasons to call moep_set_custom_wait(). This call is only
 * usefull if you want to integrate another existing run loop implementation
 * that works similiar to moep_wait().
 *
 * \param wait the custom wait call
 */
void moep_set_custom_wait(int (*wait)(int, struct epoll_event *, int, int,
				      const sigset_t *));

/**
 * \brief wait for an I/O event on an epoll file descriptor
 *
 * The moep_wait() call waits for events on the epoll instance referred to by
 * the file descriptor \paramname{epfd}. The call works similiar to
 * epoll_pwait() and the parameters are identical. See the respective manual
 * page for more details. Differences to epoll_pwait() are noted below.
 *
 * During waiting for the specified I/O events moep_wait() also waits for the
 * events specified by callbacks registered with moep_callback_create(). If
 * such events occur the respective callback functions are called. Depending on
 * the return value of such a callback moep_wait() either returns this return
 * value or keeps waiting if the return value was 0.
 *
 * \warning
 * You need very good reasons to call moep_wait() directly. Normally, using
 * moep_run() is the intended usage. Its sole purpose is the ability to stack
 * another custom wait call on it. This stacking can also be done in the other
 * direction via moep_set_custom_wait().
 *
 * \return The return values are the same as for epoll_pwait. Additionally,
 * arbitrary values can be returned by a callback.
 *
 * \errors{The error values are the same as for epoll_pwait(). Additionally,
 * arbitrary error values can be returned by a callback.}
 * \enderrors
 */
int moep_wait(int epfd, struct epoll_event *events, int maxevents, int timeout,
	      const sigset_t *sigmask);

/**
 * \brief a signal handler
 *
 * A sig_handler() is a function pointer that is called by moep_run() if a
 * signal occurs. The signals are internally delivered via a signalfd. See the
 * respective manual page for details. Information on the contents of
 * \paramname{siginfo} can also be found in the manual page of signalfd. As the
 * sig_handler() is not called asynchronously, the actions of the handler are
 * not limited to signal-safe functions.
 *
 * \param siginfo information on the signal
 * \param data user specified data
 *
 * \return The signal handler must either return 0, or moep_run() returns with
 * this return value.
 */
typedef int (*sig_handler)(struct signalfd_siginfo *siginfo, void *);

/**
 * \brief run the moep multiplexer
 *
 * The moep_run() call is the main run loop of libmoep. While executing the
 * event loop, callbacks registered with moep_callback_create() will be called
 * for the specified I/O events. If the return value of such a callback is
 * nonzero, moep_run() returns with that return value. There is one exception to
 * that rule, namely if errno is \errno{EINTR}, moep_run() keeps running.
 *
 * The moep_run() call blocks all currently unblocked signals and uses a
 * signalfd to read those signals in a sychronous way. For each delivered
 * signal \paramname{sigh} is called with \paramname{data} as argument. The
 * return value of \paramname{sigh} determines if moep_run() returns, i.e., if
 * the return value is 0 moep_run() keeps running, otherwise moep_run() returns
 * the return value. If \paramname{sigh} is NULL, signals are ignored.
 *
 * \param sigh the signal handler
 * \param data the argument for sigh
 *
 * \return This call does not return until an error occured or a callback
 * returned with a nonzero value.
 * \retval -1 on error, errno is set appropriately.
 *
 * \errors{The error values are the same as for moep_wait(), except that
 * \errno{EINTR} is handled internally. Additionally, arbitrary error values can
 * be returned by a callback.}
 * \enderrors
 */
int moep_run(sig_handler sigh, void *data);

/** \} */
#endif /* __MOEP80211_SYSTEM_H */
