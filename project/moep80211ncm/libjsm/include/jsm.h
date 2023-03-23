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

#ifndef LIBJSM80211_H_
#define LIBJSM80211_H_

#ifndef JSM80211_RINGBUFFER_LENGTH
#define JSM80211_RINGBUFFER_LENGTH 65536 ///<Length of the packet ringbuffer (must be a power of 2)
#endif

#ifndef JSM80211_PDVSTAT_SLICE_COUNT
#define JSM80211_PDVSTAT_SLICE_COUNT 1024 ///<Number of pdv quantile estimation slices
#endif

#ifndef JSM80211_PDVSTAT_SLICE_WIDTH_MIN
#define JSM80211_PDVSTAT_SLICE_WIDTH_MIN 0.000001 ///<Minumum width of pdv quantile estimation slices
#endif

#ifndef JSM80211_PDVSTAT_DECAY
#define JSM80211_PDVSTAT_DECAY 0.0000001 ///<Decay of pdvstat values
#endif

#ifndef JSM80211_TIMER_MIN
#define JSM80211_TIMER_MIN 0.000001 ///<Minimum timer value/interval
#endif

#ifndef JSM80211_TIMER_UPDATE_MIN
#define JSM80211_TIMER_UPDATE_MIN 0.000005 ///<Minimum timer adjustment (i.e., difference to trigger an update)
#endif

#ifndef JSM80211_ADJ_MAX_FACTOR
#define JSM80211_ADJ_MAX_FACTOR 2.0 ///<Maximum load factor
#endif

/**
 * \brief A jsm80211 module
 * \note All functions except init and cleanup are concurrency safe for a single module. Different modules may be used concurrently in any way.
 *
 * Stores all relevant information for a single network device.
 */
struct jsm80211_module;

/**
 * \brief The parameters for the suppression algorithm
 */
struct jsm80211_parameters {
  double limit;    ///<Interval limit in seconds (i.e., inter-packet times are cutoff to this value)
  double alpha;    ///<Exponential moving average factor for rising intervals (must be between 0.0 and 1.0)
  double beta;     ///<Exponential moving average factor for falling intervals (must be between 0.0 and 1.0)
  double quantile; ///<Delay variation quantile (i.e., the quantile of the delay variations that is supposed to be suppressed)
  double max_adj;  ///<The maximum adj factor (i.e., the adj factor when the buffer is empty; must be between 1.0 and pos. infinity)
};

/**
 * \brief The jsm80211 dequeue callback (i.e., called to dequeue packets)
 * \param module The originating module
 * \param packet The packet
 * \param data The (opaque) user data
 * \return 0 for success, negative error code otherwise (passed to the application calling jsm80211_queue())
 */
typedef int(*jsm80211_dequeue_callback)(struct jsm80211_module* module, void* packet, void* data);

/**
 * \brief Initialize the jsm80211 module
 * \param module The module
 * \param parameters Jitter suppression parameters
 * \param dequeue_callback The dequeue callback
 * \param user_data The (opaque) user data for the callback
 * \return 0 for success, negative error code otherwise
 */
int jsm80211_init(struct jsm80211_module** module, struct jsm80211_parameters* parameters,
  jsm80211_dequeue_callback dequeue_callback, void* user_data);

/**
 * \brief Cleanup the jsm80211 module
 * \param module The module (or NULL)
 */
void jsm80211_cleanup(struct jsm80211_module* module);

/**
 * \brief Queue a packet into the module
 * \param module The module
 * \param packet The packet to queue
 * \return 0 for success, negative error code otherwise
 */
int jsm80211_queue(struct jsm80211_module* module, void* packet);

/**
 * \brief Log the module state
 * \param module The module
 */
void jsm80211_log_state(struct jsm80211_module* module);

#endif // LIBJSM80211_H_
