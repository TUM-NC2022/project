#ifndef __QDELAY_H_
#define __QDELAY_H_

#include <time.h>

#include "global.h"

/* Store the tx time for sequence number seq */
void qdelay_store(uint16_t seq);

/* Update the queueing delay when the drivers echoes a frame */
void qdelay_update(uint16_t seq);

/* Get the actual estimate of the queueing delay */
double qdelay_get();

/* Get the percentage of lost frames, i.e., frames for which no echo has been
   recceveid */
double qdelay_loss();

int qdelay_packet_cnt();

#endif
