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

#ifndef TIMEUTIL_H_
#define TIMEUTIL_H_

#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#include <math.h>
#include <time.h>

static inline void jsm80211_ts_add(struct timespec* res, const struct timespec* val1, const struct timespec* val2)
{
  res->tv_sec = val1->tv_sec + val2->tv_sec;
  res->tv_nsec = val1->tv_nsec + val2->tv_nsec;
  if(res->tv_nsec >= 1000000000L) {
    ++res->tv_sec;
    res->tv_nsec -= 1000000000L;
  }
}

static inline int jsm80211_ts_sub(struct timespec* res, const struct timespec* val1, const struct timespec* val2)
{
  res->tv_sec = val1->tv_sec - val2->tv_sec;
  res->tv_nsec = val1->tv_nsec - val2->tv_nsec;
  if(res->tv_nsec < 0) {
    --res->tv_sec;
    res->tv_nsec += 1000000000L;
  }
  return(res->tv_sec < 0);
}

static inline long jsm80211_ts_cmp(const struct timespec* val1, const struct timespec* val2)
{
  return(val1->tv_sec - val2->tv_sec ?: val1->tv_nsec - val2->tv_nsec);
}

static inline int64_t jsm80211_ts_ts2ns(const struct timespec* val)
{
  return(val->tv_sec * 1000000000L + val->tv_nsec);
}

static inline void jsm80211_ts_ns2ts(struct timespec* res, int64_t val)
{
  res->tv_sec = (time_t)(val / 1000000000L);
  res->tv_nsec = (long)(val % 1000000000L);
}

static inline double jsm80211_ts_ts2dbl(const struct timespec* val)
{
  return((double)val->tv_sec + (double)val->tv_nsec / 1000000000.0);
}

static inline int jsm80211_ts_dbl2ts(struct timespec* res, double val)
{
  if(val < 0) {
    return(-EINVAL);
  }
  res->tv_sec = (time_t)val;
  res->tv_nsec = (long)((val - (time_t)val) * 1000000000.0);
  return(0);
}

#endif // TIMEUTIL_H_
