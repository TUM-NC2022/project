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

#ifndef RINGSTAT_H_
#define RINGSTAT_H_

#include <jsm.h>

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

struct jsm80211_pdvstat {
  double  exp_adjust;
  double  total_count;
  double* slices;
};

static inline int jsm80211_pdvstat_alloc(struct jsm80211_pdvstat* pdvstat, double pdv_max)
{
  pdvstat->exp_adjust = log(pdv_max / JSM80211_PDVSTAT_SLICE_WIDTH_MIN) / JSM80211_PDVSTAT_SLICE_COUNT;
  pdvstat->total_count = 0;
  pdvstat->slices = malloc(JSM80211_PDVSTAT_SLICE_COUNT * sizeof(double));
  if(pdvstat->slices == NULL) {
    return(-ENOMEM);
  }
  memset(pdvstat->slices, 0, JSM80211_PDVSTAT_SLICE_COUNT * sizeof(double));
  return(0);
}

static inline void jsm80211_pdvstat_free(struct jsm80211_pdvstat* pdvstat)
{
  free(pdvstat->slices);
}

static inline void jsm80211_pdvstat_update(struct jsm80211_pdvstat* pdvstat, double pdv, double alpha)
{
  //adjust other slices
  off_t i;
  for(i = JSM80211_PDVSTAT_SLICE_COUNT - 1; i >= 0; i--) {
    pdvstat->slices[i] *= (1 - alpha);
  }
  pdvstat->total_count *= (1 - alpha);
  //adjust current slice
  off_t slice = (off_t)(log(fmax(pdv / JSM80211_PDVSTAT_SLICE_WIDTH_MIN, 1.0)) / pdvstat->exp_adjust);
  if(slice < 0 || slice >= JSM80211_PDVSTAT_SLICE_COUNT) {
    return;//ignore values too big to fit
  }
  pdvstat->slices[slice]++;
  pdvstat->total_count++;
}

static inline double jsm80211_pdvstat_quantile(struct jsm80211_pdvstat* pdvstat, double q) {
  double count = q * pdvstat->total_count;
  off_t slice = 0;
  while((count -= pdvstat->slices[slice]) > 0.0 && ++slice < JSM80211_PDVSTAT_SLICE_COUNT);
  return(exp(slice * pdvstat->exp_adjust) * JSM80211_PDVSTAT_SLICE_WIDTH_MIN);
}

#endif // RINGSTAT_H_
