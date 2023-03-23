#include "qdelay.h"
#include <moepcommon/util.h>

static struct timespec txtime[UINT16_MAX];
static double qdelay  = 0;
static uint64_t sent = 0;
static uint64_t missing = 0;

static uint16_t sent_seq;
static uint16_t echo_seq;

void
qdelay_store(uint16_t seq)
{
	clock_gettime(CLOCK_MONOTONIC, &txtime[seq]);
	sent++;
	sent_seq = seq;
}

void
qdelay_update(uint16_t seq)
{
	struct timespec now;
	double delay;

	if (!timespecisset(&txtime[seq])) {
		missing++;
		LOG(LOG_ERR, "missing txseq");
		return;
	}

	clock_gettime(CLOCK_MONOTONIC, &now);
	timespecsub(&now, &txtime[seq]);
	timespecclear(&txtime[seq]);

	delay = ((double)now.tv_sec*1000.0 + (double)now.tv_nsec/1000000.0);
	qdelay *= QDELAY_UPDATE_WEIGHT;
	qdelay += (1.0-QDELAY_UPDATE_WEIGHT)*delay;

	echo_seq = seq;
}

double
qdelay_get()
{
	return qdelay;
}

double
qdelay_loss()
{
	return sent == 0.0 ? 0.0 : (double)missing/(double)sent;
}

int qdelay_packet_cnt()
{
	return (uint16_t)(sent_seq - echo_seq);
}
