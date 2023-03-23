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

#ifndef RINGBUFFER_H_
#define RINGBUFFER_H_

#include <errno.h>
#include <stdlib.h>

struct jsm80211_ringbuffer {
  size_t length;
  off_t  put_index;
  off_t  get_index;
  void** elems;
};

static inline int jsm80211_ringbuffer_alloc(struct jsm80211_ringbuffer* buf, size_t length)
{
  if((length & (length - 1)) != 0) {//check for power of 2
    return(-EINVAL);
  }
  buf->length = length;
  buf->put_index = 0;
  buf->get_index = 0;
  buf->elems = malloc(sizeof(void*) * length);
  if(buf->elems == NULL) {
    return(-ENOMEM);
  }
  return(0);
}

static inline void jsm80211_ringbuffer_free(struct jsm80211_ringbuffer* buf)
{
  free(buf->elems);
}

static inline int jsm80211_ringbuffer_put(struct jsm80211_ringbuffer* buf, void* elem)
{
  off_t put_index = buf->put_index;
  off_t get_index = __atomic_load_n(&buf->get_index, __ATOMIC_SEQ_CST);
  if(put_index - get_index == (off_t)buf->length) {
    return(-EAGAIN);
  }
  buf->elems[put_index & (buf->length - 1)] = elem;
  __atomic_store_n(&buf->put_index, put_index + 1, __ATOMIC_SEQ_CST);
  return(0);
}

static inline int jsm80211_ringbuffer_get(struct jsm80211_ringbuffer* buf, void** elem)
{
  off_t put_index = __atomic_load_n(&buf->put_index, __ATOMIC_SEQ_CST);
  off_t get_index = buf->get_index;
  if(get_index == put_index) {
    return(-EAGAIN);
  }
  *elem = buf->elems[get_index & (buf->length - 1)];
  __atomic_store_n(&buf->get_index, get_index + 1, __ATOMIC_SEQ_CST);
  return(0);
}

static inline size_t jsm80211_ringbuffer_length(struct jsm80211_ringbuffer* buf)
{
  return(buf->length);
}

static inline size_t jsm80211_ringbuffer_count(struct jsm80211_ringbuffer* buf)
{
  off_t put_index = __atomic_load_n(&buf->put_index, __ATOMIC_SEQ_CST);
  off_t get_index = __atomic_load_n(&buf->get_index, __ATOMIC_SEQ_CST);
  return(put_index - get_index);
}

#endif // RINGBUFFER_H_
