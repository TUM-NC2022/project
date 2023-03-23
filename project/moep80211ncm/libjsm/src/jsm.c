/*
 * This file is part of jsm80211.
 *
 * Copyright (C) 2015 	Martin E. Jobst <martin.jobst@tum.de>
 *
 * jsm80211 is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3, or (at your option) any later version.
 *
 * jsm80211 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * jsm80211; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <jsm.h>
#include "timeutil.h"
#include "ringbuffer.h"
#include "pdvstat.h"

#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <math.h>
#include <time.h>

#include <moepcommon/util.h>
#include <moepcommon/timeout.h>

struct jsm80211_module {
  struct jsm80211_parameters params;
  int64_t                    last_packet;
  double                     average_interval;
  double                     timer_interval;
  double                     target_backlog;
  timeout_t                  timer;
  struct jsm80211_ringbuffer queue;
  struct jsm80211_pdvstat    pdvstat;
  jsm80211_dequeue_callback  dequeue_callback;
  void*                      user_data;
};

static int update_average(struct jsm80211_module* module, double interval)
{
  if(interval > module->params.limit) {
    interval = module->params.limit;//cutoff to limit
  }
  double factor = interval > module->average_interval ? module->params.alpha : module->params.beta;
  module->average_interval = factor * interval + (1.0 - factor) * module->average_interval;
  return(0);
}

static double adj_function(struct jsm80211_module* module)
{
  double target_load = module->average_interval > 0.0 ? module->target_backlog / module->average_interval : 0.0;
  double queue_load = (double)jsm80211_ringbuffer_count(&module->queue);
  double load_factor = target_load > 0.0 ? queue_load / target_load : 0.0;
  double adj_factor;
  if(load_factor <= 1.0) {
    adj_factor = (module->params.max_adj - 1.0) * pow(cos(M_PI_2 * load_factor), 3.0) + 1;
  }
  else if(load_factor < JSM80211_ADJ_MAX_FACTOR) {
    adj_factor = pow(cos(M_PI_2 * ((load_factor - 1.0)/(JSM80211_ADJ_MAX_FACTOR - 1.0) + 1.0)), 3.0);
  }
  else {
    adj_factor = 0;
  }

  if (adj_factor < 0.1)
  	adj_factor = 0.05;

  return(adj_factor);
}

static int update_timer(struct jsm80211_module* module)
{
  if(jsm80211_ringbuffer_count(&module->queue) != 0) {
    double timer_value;
    double timer_interval;
    struct itimerspec timer_val;
    //determine timer interval
    timer_interval = module->average_interval * adj_function(module);
    timer_interval = fmax(timer_interval, JSM80211_TIMER_MIN);
    //skip timer update if less than 10us difference
    if(module->timer_interval != 0.0 && fabs(timer_interval - module->timer_interval) < JSM80211_TIMER_UPDATE_MIN) {
      return(0);
    }
    //determine timer value (adjust for remaining time only if current interval is greater than 1ms)
    timer_value = timer_interval;
    if(module->timer_interval > 0.001 && timeout_gettime(module->timer, &timer_val) == 0) {
      timer_value += jsm80211_ts_ts2dbl(&timer_val.it_value) - module->timer_interval;
    }
    timer_value = fmax(timer_value, JSM80211_TIMER_MIN);
    //set timer
    jsm80211_ts_dbl2ts(&timer_val.it_value, timer_value);//cannot fail
    jsm80211_ts_dbl2ts(&timer_val.it_interval, timer_interval);//cannot fail
    if(timeout_settime(module->timer, 0, &timer_val) != 0) {
      return(-EINVAL);
    }
    module->timer_interval = timer_interval;
  }
  else {
    if(timeout_settime(module->timer, 0, NULL) != 0) {
      return(-EINVAL);
    }
    //module->timer_interval = 0.0;
  }
  return(0);
}

static int dequeue(timeout_t timer, uint32_t overrun, void* data)
{
  (void)timer;//unused
  struct jsm80211_module* module = data;
  void* packet = NULL;
  int ret;
  do {
    if(jsm80211_ringbuffer_get(&module->queue, &packet) != 0) {
      return(0);
    }
    ret = module->dequeue_callback(module, packet, module->user_data);
    if(ret != 0) {
      return(ret);
    }
  } while(overrun-- > 0);
  return(update_timer(module));
}

int jsm80211_init(struct jsm80211_module** _module, struct jsm80211_parameters* parameters,
  jsm80211_dequeue_callback dequeue_callback, void* user_data)
{
  int res = 0;
  struct jsm80211_module* module = malloc(sizeof(struct jsm80211_module));
  if(module == NULL) {
    return(-ENOMEM);
  }
  memset(module, 0, sizeof(struct jsm80211_module));
  module->params = *parameters;
  module->last_packet = INT64_MAX;
  res = timeout_create(CLOCK_MONOTONIC, &module->timer, dequeue, module);
  if(res != 0) {
    return(-errno);
  }
  res = jsm80211_ringbuffer_alloc(&module->queue, JSM80211_RINGBUFFER_LENGTH);
  if(res != 0) {
    goto err_timer;
  }
  res = jsm80211_pdvstat_alloc(&module->pdvstat, parameters->limit);
  if(res != 0) {
    goto err_ringbuf;
  }
  module->dequeue_callback = dequeue_callback;
  module->user_data = user_data;
  *_module = module;
  LOG(LOG_INFO, "jsm80211: parameters = { limit=%g ms, alpha=%g, beta=%g, quantile=%g, max_adj=%g }",
    parameters->limit * 1000.0,
    parameters->alpha,
    parameters->beta,
    parameters->quantile,
    parameters->max_adj);
  return(0);
err_ringbuf:
  jsm80211_ringbuffer_free(&module->queue);
err_timer:
  timeout_delete(module->timer);
  return(res);
}

void jsm80211_cleanup(struct jsm80211_module* module)
{
  if(module == NULL) {
    return;
  }
  jsm80211_ringbuffer_free(&module->queue);
  timeout_delete(module->timer);
  free(module);
}

int jsm80211_queue(struct jsm80211_module* module, void* packet)
{
  struct timespec current_timespec;
  if(clock_gettime(CLOCK_MONOTONIC, &current_timespec) != 0) {
    return(-errno);
  }
  int res = jsm80211_ringbuffer_put(&module->queue, packet);
  if(res != 0) {
    return(res);
  }
  int64_t current_time = jsm80211_ts_ts2ns(&current_timespec);
  int64_t interval = current_time - module->last_packet;
  module->last_packet = current_time;
  if(interval < 0) {
    interval = 0;
  }
  double jitter = (double)interval / 1000000000.0 - module->average_interval;
  jsm80211_pdvstat_update(&module->pdvstat, jitter, JSM80211_PDVSTAT_DECAY);
  module->target_backlog = jsm80211_pdvstat_quantile(&module->pdvstat, module->params.quantile);
  res = update_average(module, (double)interval / 1000000000.0);
  if(res != 0) {
    goto err;
  }
  res = update_timer(module);
err:
  return(res);
}

void jsm80211_log_state(struct jsm80211_module* module)
{
  LOG(LOG_INFO, "jsm80211: ipt_avg=%g ms, ipt_timer=%g ms, backlog=%zd pkt, backlog_target=%g pkt (%g ms)",
    module->average_interval * 1000.0,
    module->timer_interval * 1000.0,
    jsm80211_ringbuffer_count(&module->queue),
    module->average_interval > 0.0 ? module->target_backlog / module->average_interval : 0.0,
    module->target_backlog * 1000.0);
}

