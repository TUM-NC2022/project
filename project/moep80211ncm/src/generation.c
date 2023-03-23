#include <errno.h>
#include <assert.h>
#include <math.h>

#include <moep/system.h>
#include <moep/types.h>
#include <moep/ieee80211_addr.h>
#include <moep/ieee80211_frametypes.h>
#include <moep/radiotap.h>
#include <moep/modules/ieee8023.h>
#include <moep/modules/moep80211.h>

#include <moepcommon/util.h>
#include <moepcommon/list.h>
#include <moepcommon/timeout.h>

#include "generation.h"
#include "session.h"
#include "ncm.h"
#include "qdelay.h"


static int cb_rtx(timeout_t t, u32 overrun, void *data);
static int cb_ack(timeout_t t, u32 overrun, void *data);
extern int rad_tx_event;
extern int sfd;

struct pvpos {
	int cur;
	int min;
	int max;
};

struct estimators {
	int master;
	int slave;
	int *local;
	int *remote;
};

struct generation {
	struct list_head	list;

	rlnc_block_t 		rb;

	enum MOEPGF_TYPE	gftype;
	enum GENERATION_TYPE	gentype;
	uint16_t		seq;
	size_t			packet_size;
	int			packet_count;

	struct generation_state	state;

	struct pvpos		encoder;
	struct pvpos		decoder;

	int			ack_block;
	int			missing;

	session_t		session;
	struct list_head	*gl;

	int			tx_src;
	double			tx_red;

	struct {
		timeout_t rtx;
		timeout_t ack;
	} task;
};

static double
rtx(const generation_t g)
{
	return ((double)g->tx_src + g->tx_red);
}

static void
rtx_dec(generation_t g)
{
	if (rtx(g) >= 0.0) {
		g->tx_src = 0;
		g->tx_red = 0.0;
	}
	if (g->gentype == FORWARD) {
		g->tx_red -= session_redundancy(g->session);
	} else {
		g->tx_src -= 1;
		g->tx_red -= session_redundancy(g->session) - 1.0;
	}
}

static void
rtx_inc(generation_t g)
{
	if (g->tx_src < 0) {
		g->tx_src += 1;
		g->state.tx.data++;
		return;
	}

	g->tx_red += 1.0;
	g->state.tx.redundant++;
	return;
}

static void
rtx_reset(generation_t g)
{
	g->tx_src = 0;
	g->tx_red = 0.0;
}

static struct itimerspec *
rtx_timeout(const generation_t g)
{
	double t;

	if (rtx(g) > -1) {
		t = (double)GENERATION_RTX_MIN_TIMEOUT;
		t += generation_index(g)+rtx(g)+1.0;
		t = min(t, (double)GENERATION_RTX_MAX_TIMEOUT);
	}
	else {
		t = 0;
	}

	//t += qdelay_get() / 2;

	return timeout_msec((int)t, 0);
}

inline int
generation_window_size(const struct list_head *gl)
{
	generation_t first = list_first_entry(gl, struct generation, list);
	generation_t last = list_last_entry(gl, struct generation, list);
	return delta(last->seq, first->seq, GENERATION_MAX_SEQ) + 1;
}

ssize_t
generation_feedback(generation_t g, struct generation_feedback *fb,
		size_t maxlen)
{
	int count;
	generation_t cur;

	count = generation_window_size(g->gl);
	if (count*sizeof(*fb) > maxlen) {
		LOG(LOG_ERR, "buffer too small");
		errno = ENOMEM;
		return -1;
	}

	list_for_each_entry(cur, g->gl, list) {
		fb->lock.ms	= cur->state.fms.lock;
		fb->lock.sm	= cur->state.fsm.lock;
		fb->ddim.ms	= cur->state.fms.ddim;
		fb->sdim.ms	= cur->state.fms.sdim;
		fb->ddim.sm	= cur->state.fsm.ddim;
		fb->sdim.sm	= cur->state.fsm.sdim;
		timeout_settime(cur->task.ack, 0, NULL);
		fb++;
	}

	return 0;
}

static inline void
generation_lock(generation_t g)
{
	g->state.local->lock = 1;
	g->encoder.max = g->encoder.cur - 1;
}

static inline void
generation_update_dimensions(generation_t g,
			const struct generation_feedback *fb)
{
	g->state.fms.sdim = max((int)g->state.fms.sdim, (int)fb->sdim.ms);
	g->state.fms.ddim = max((int)g->state.fms.ddim, (int)fb->ddim.ms);
	g->state.fsm.sdim = max((int)g->state.fsm.sdim, (int)fb->sdim.sm);
	g->state.fsm.ddim = max((int)g->state.fsm.ddim, (int)fb->ddim.sm);
}

// IMPORTANT: generation_update_dimension() has to be called beforehand!
static inline void
generation_update_locks(generation_t g, const struct generation_feedback *fb)
{
	g->state.fms.lock = max((int)g->state.fms.lock, (int)fb->lock.ms);
	g->state.fsm.lock = max((int)g->state.fsm.lock, (int)fb->lock.sm);

	// Forwarders are done here
	if (g->gentype == FORWARD)
		return;

	// If the remote flow was locked, we just have locked the local flow. To
	// detect when all decoded packets have been returned, we have to update
	// the decoder.max pivot.
	if (g->state.remote->lock) {
		generation_lock(g);
		g->decoder.max = g->decoder.min + g->state.remote->sdim - 1;
	}
}

int
generation_assume_complete(generation_t g)
{
	if (g->gentype == FORWARD) {
		g->state.fms.lock = 1;
		g->state.fms.ddim = g->state.fms.sdim;

		g->state.fsm.lock = 1;
		g->state.fsm.ddim = g->state.fsm.sdim;

		return 0;
	}

	g->state.remote->lock = 1;
	g->state.local->ddim = g->state.local->sdim;
	g->decoder.max = g->decoder.min + g->state.remote->sdim - 1;

	if (generation_remote_flow_decoded(g)) {
		g->state.local->lock = 1;
		g->encoder.max = g->encoder.cur - 1;
	}
	else {
		LOG(LOG_ERR, "remote->sdim=%d, remote->ddim=%d",
				g->state.remote->sdim, g->state.remote->ddim);
		DIE("something is terribly wrong");
	}

	if (!generation_local_flow_decoded(g))
		LOG(LOG_INFO, "generation seq=%d local flow NOT decoded",
				g->seq);
	if (!generation_remote_flow_decoded(g))
		LOG(LOG_INFO, "generation seq=%d remote flow NOT decoded",
				g->seq);

	if (!generation_remote_flow_decoded(g))
		LOG(LOG_INFO, "generation seq=%d remote flow NOT decoded",
				g->seq);

	if (!generation_is_complete(g))
		LOG(LOG_INFO, "generation seq=%d INCOMPLETE", g->seq);

	if (!generation_is_returned(g))
		return -1;

	return 0;
}

int
generation_local_flow_locked(const generation_t g)
{
	if (g->gentype == FORWARD)
		return EGENFAIL;

	return g->state.local->lock;
}

int
generation_remote_flow_locked(const generation_t g)
{
	if (g->gentype == FORWARD)
		return EGENFAIL;

	return g->state.remote->lock;
}

int
generation_is_locked(const generation_t g)
{
	return (g->state.fms.lock && g->state.fsm.lock);
}

int
generation_local_flow_decoded(const generation_t g)
{
	if (g->gentype == FORWARD)
		return EGENFAIL;

	if (g->state.local->sdim == g->state.local->ddim)
		return 1;

	return 0;
}

int
generation_remote_flow_decoded(const generation_t g)
{
	if (g->gentype == FORWARD)
		return EGENFAIL;

	if (g->state.remote->sdim == g->state.remote->ddim)
		return 1;

	return 0;
}

static int
generation_fms_decoded(const generation_t g)
{
	if (g->state.fms.sdim == g->state.fms.ddim)
		return 1;

	return 0;
}

static int
generation_fsm_decoded(const generation_t g)
{
	if (g->state.fsm.sdim == g->state.fsm.ddim)
		return 1;

	return 0;
}

int
generation_is_decoded(const generation_t g)
{
	return (generation_fms_decoded(g) && generation_fsm_decoded(g));
}

int
generation_is_complete(const generation_t g)
{
	return (generation_is_locked(g) && generation_is_decoded(g));
}

int
generation_is_returned(const generation_t g)
{
	if (g->gentype == FORWARD)
		return 1;

	if (g->state.remote->sdim == g->decoder.cur - g->decoder.min)
		return 1;

	return 0;
}

static void
init_pvpos(generation_t g)
{
	switch (g->gentype) {
	case MASTER:
		g->encoder.min	= 0;
		g->encoder.max	= g->packet_count / 2 - 1;
		g->encoder.cur	= g->encoder.min;
		g->decoder.min	= g->packet_count / 2;
		g->decoder.max	= g->packet_count - 1;
		g->decoder.cur	= g->decoder.min;
		g->state.local	= &g->state.fms;
		g->state.remote	= &g->state.fsm;
		break;
	case SLAVE:
		g->encoder.min 	= g->packet_count / 2;
		g->encoder.max 	= g->packet_count - 1;
		g->encoder.cur	= g->encoder.min;
		g->decoder.min 	= 0;
		g->decoder.max 	= g->packet_count / 2 - 1;
		g->decoder.cur	= g->decoder.min;
		g->state.local	= &g->state.fsm;
		g->state.remote	= &g->state.fms;
		break;
	case FORWARD:
		g->encoder.min 	= 0;
		g->encoder.max 	= g->packet_count - 1;
		g->encoder.cur	= 0;
		g->decoder.min 	= 0;
		g->decoder.max 	= g->packet_count - 1;
		g->decoder.cur	= 0;
		break;
	}
}

generation_t
generation_init(session_t s, struct list_head *gl,
		enum GENERATION_TYPE gentype, enum MOEPGF_TYPE gftype,
		int packet_count, size_t packet_size, int sequence_number)
{
	struct generation *g;

	if (!(g = malloc(sizeof(*g))))
		DIE("malloc() failed: %s", strerror(errno));

	memset(g, 0, sizeof(*g));

	g->rb = rlnc_block_init(packet_count, packet_size, MEMORY_ALIGNMENT,
								gftype);
	if (!g->rb)
		DIE("rlnc_block_init() failed");

	g->seq = sequence_number;
	g->packet_count = packet_count;
	g->packet_size  = packet_size;
	g->gftype	= gftype;
	g->gentype	= gentype;
	g->session	= s;
	g->gl		= gl;

	list_add_tail(&g->list, g->gl);

	init_pvpos(g);

	if (0 > timeout_create(CLOCK_MONOTONIC, &g->task.rtx, cb_rtx,g))
		DIE("timeout_create() failed: %s", strerror(errno));
	if (0 > timeout_create(CLOCK_MONOTONIC, &g->task.ack, cb_ack,g))
		DIE("timeout_create() failed: %s", strerror(errno));

	return g;
}

static void
generation_destroy(generation_t g)
{
	timeout_delete(g->task.rtx);
	timeout_delete(g->task.ack);
	rlnc_block_free(g->rb);
	free(g);
}

void
generation_list_destroy(struct list_head *gl)
{
	struct generation *tmp, *cur;

	list_for_each_entry_safe(cur, tmp, gl, list) {
		list_del(&cur->list);
		generation_destroy(cur);
	}
}

int
generation_reset(generation_t g, uint16_t seq)
{
	struct generation_flowstate *local, *remote;

	// Sanity checks
	if (g->tx_src > 0)
		LOG(LOG_ERR, "ERROR: tx_src = %d", g->tx_src);

	if (g->gentype != FORWARD) {
		if (g->state.tx.data != g->state.local->sdim)
			LOG(LOG_ERR, "ERROR: tx.data != sdim");
	}

	if (rlnc_block_reset(g->rb))
		return EGENFAIL;

	session_commit_state(g->session, &g->state);

	g->seq = seq;
	rtx_reset(g);

	local = g->state.local;
	remote = g->state.remote;
	memset(&g->state, 0, sizeof(g->state));
	g->state.local = local;
	g->state.remote = remote;

	init_pvpos(g);

	timeout_settime(g->task.rtx, 0, NULL);
	timeout_settime(g->task.ack, 0, NULL);

	return 0;
}

static int
encoder_add(generation_t g, const uint8_t *src, size_t len)
{
	int ret;

	if (g->gentype == FORWARD)
		return EGENINVAL;

	if (g->state.local->lock)
		return EGENLOCKED;

	if (len > g->packet_size) {
		LOG(LOG_ERR, "encoded frame size %d larger than packet_size %d",
				(int)len, (int)g->packet_size);
		return EGENNOMEM;
	}

	/* No check for free slot needed since generation becomes locked when
	   the last slot is used. */

	if ((ret = rlnc_block_add(g->rb, g->encoder.cur, src, len))) {
		LOG(LOG_ERR, "rlnc_block_add() failed: %d", ret);
		return EGENFAIL;
	}
	g->encoder.cur++;
	g->state.local->sdim++;

	/* encoder.cur points to the next available slot, i.e., the generation
	   must be locked when it points to an invalid position */
	if (g->encoder.cur > g->encoder.max)
		generation_lock(g);

	rtx_dec(g);

	timeout_settime(g->task.rtx, TIMEOUT_FLAG_SHORTEN, rtx_timeout(g));

	return 0;
}

static ssize_t
decoder_get(generation_t g, uint8_t *dst, size_t maxlen)
{
	int ret;

	if (g->decoder.cur > g->decoder.max)
		return EGENNOMORE;

	ret = rlnc_block_get(g->rb, g->decoder.cur, dst, maxlen);

	if (0 > ret) {
		LOG(LOG_ERR, "rlnc_block_get() failed");
		return EGENFAIL;
	}

	if (ret > 0)
		g->decoder.cur++;

	return ret;
}

ssize_t
generation_encoder_get(const generation_t g, void *buffer, size_t maxlen)
{
	ssize_t ret;
	int flags = RLNC_STRUCTURED;

	if (g->gentype == FORWARD)
		flags &= ~RLNC_STRUCTURED;

	ret = rlnc_block_encode(g->rb, buffer, maxlen, flags);

	if (0 > ret) {
		LOG(LOG_ERR, "rlnc_block_encode() failed");
		return EGENFAIL;
	}

	return ret;
}

static int
decoder_add(generation_t g, const void *payload, size_t len)
{
	/** Encoded packets can be added anytime:
	 * a) The packet is linear independent => the decoder rank is not full
	 * b) The packet is linear dependent => it is eliminated anyway
	 **/

	int ret;

	ret = rlnc_block_decode(g->rb, payload, len);
	if (0 > ret) {
		LOG(LOG_ERR, "rlnc_block_decode() failed");
		return EGENFAIL;
	}

	if (g->gentype != FORWARD)
		g->state.remote->ddim = rlnc_block_rank_decode(g->rb);

	return 0;
}

int
generation_encoder_space(const generation_t g)
{
	int ret;

	ret = g->encoder.max - g->encoder.cur + 1;
	assert (ret >= 0);

	return ret;
}

int
generation_encoder_dimension(const generation_t g)
{
	return rlnc_block_rank_encode(g->rb);
}

int
generation_decoder_dimension(const generation_t g)
{
	return rlnc_block_rank_decode(g->rb);
}

void
generation_debug_print_state(const generation_t g)
{
	LOG(LOG_DEBUG, "%d | %d | %d || %d | %d | %d",
		g->state.local->sdim, g->state.local->ddim, g->state.local->lock,
		g->state.remote->sdim, g->state.remote->ddim, g->state.remote->lock);
}

generation_t
generation_encoder_add(struct list_head *gl, void *buffer, size_t len)
{
	int ret;
	generation_t g;

	list_for_each_entry(g, gl, list) {
		if (!generation_encoder_space(g))
			continue;

		ret = encoder_add(g, buffer, len);

		if (0 > ret)
			DIE("generation_encoder_add() failed: %d", ret);

		return g;
	}

	return NULL;
}

static int
generation_advance(struct list_head *gl)
{
	generation_t first, last, g;
	int n, seq;

	for (n=0;; n++) {
		first = list_first_entry(gl, struct generation, list);
		last = list_last_entry(gl, struct generation, list);

		if (!generation_is_complete(first))
			break;
		if (!generation_is_returned(first))
			break;

		seq = (last->seq + 1) % (GENERATION_MAX_SEQ+1);
		generation_reset(first, seq);
		list_rotate_left(gl);
	}

	if (n == 0)
		return 0;

	list_for_each_entry(g, gl, list) {
		if (g->gentype == FORWARD) {
			if (generation_is_decoded(g))
				continue;
			if (!rlnc_block_rank_decode(g->rb))
				continue;
		} else {
			if (generation_local_flow_decoded(g))
				continue;
			if (!timeout_active(g->task.rtx))
				continue;
		}

		timeout_settime(g->task.rtx, TIMEOUT_FLAG_SHORTEN,
							rtx_timeout(g));
	}

	return n;
}

generation_t
generation_find(const struct list_head *gl, int seq)
{
	generation_t g;

	list_for_each_entry(g, gl, list) {
		if (g->seq == seq) {
			return g;
		}
	}

	return NULL;
}

generation_t
generation_get(const struct list_head *gl, int idx)
{
	generation_t g;
	int n = 0;

	list_for_each_entry(g, gl, list) {
		if (n == idx)
			return g;
		n++;
	}

	return NULL;
}

static int
generation_process_feedback(struct list_head *gl,
					const struct ncm_hdr_coded *hdr)
{
	size_t len;
	int count, i, seq;
	generation_t g;

	len = hdr->hdr.len - sizeof(*hdr);
	if (len == 0)
		return 0;

	count = len/sizeof(*hdr->fb);

	for (i=0; i<count; i++) {
		seq = hdr->lseq + i;
		g = generation_find(gl, seq);
		if (!g)
			continue;

		generation_update_dimensions(g, &hdr->fb[i]);
		generation_update_locks(g, &hdr->fb[i]);

		if (g->gentype == FORWARD) {
			if (generation_is_decoded(g)) {
				timeout_settime(g->task.rtx, 0, NULL);
				rtx_reset(g);
                        }
		} else {
			if (generation_local_flow_decoded(g)) {
				timeout_settime(g->task.rtx, 0, NULL);
				rtx_reset(g);
			}
		}
	}

	return 0;
}

generation_t
generation_decoder_add(struct list_head *gl, const void *payload, size_t len,
				const struct ncm_hdr_coded *hdr)
{
	int ret, maxseq;
	unsigned int delta;
	generation_t g;
	session_t s;

	g = list_first_entry(gl, struct generation, list);
	s = generation_get_session(g);
	delta = delta(hdr->lseq, g->seq, GENERATION_MAX_SEQ);

	if (delta > 128)//FIXME
		delta = 0;

	maxseq = (g->seq + delta) % (GENERATION_MAX_SEQ+1);

	list_for_each_entry(g, gl, list) {
		if (g->seq == maxseq)
			break;
		generation_assume_complete(g);
	}

	(void) generation_advance(gl);

	if (!(g = generation_find(gl, hdr->seq))) {
		if (len > 0) {
			g = list_first_entry(gl, struct generation, list);
			timeout_settime(g->task.ack, TIMEOUT_FLAG_INACTIVE,
				timeout_usec(GENERATION_ACK_MIN_TIMEOUT*1000,
						GENERATION_ACK_INTERVAL*1000));
		}
		return NULL;
	}

	if (len > 0) {
		ret = decoder_add(g, payload, len);
		if (0 > ret)
			DIE("decoder_add() failed: %d", ret);
		g->state.rx.data++;
		if (g->gentype == FORWARD)
			rtx_dec(g);
	}
	else {
		// explicit acknowledgement
		g->state.rx.ack++;
	}

	generation_process_feedback(gl, hdr);

	if (len > 0) {
		if (g->gentype == FORWARD) {
			if (generation_is_decoded(g))
				timeout_settime(g->task.ack,
					TIMEOUT_FLAG_INACTIVE,
					timeout_usec(
					GENERATION_ACK_MIN_TIMEOUT*1000,
					GENERATION_ACK_INTERVAL*1000));
			else
				timeout_settime(g->task.rtx,
					TIMEOUT_FLAG_SHORTEN, rtx_timeout(g));
		} else {
			if (generation_remote_flow_decoded(g))
				timeout_settime(g->task.ack,
					TIMEOUT_FLAG_INACTIVE,
					timeout_usec(
					GENERATION_ACK_MIN_TIMEOUT*1000,
					GENERATION_ACK_INTERVAL*1000));
		}
	}

	if (g->gentype == FORWARD)
		return g;

	do {
		ret = tx_decoded_frame(s);
	} while (ret == 0);

	return g;
}

ssize_t
generation_decoder_get(struct list_head *gl, void *dst, size_t maxlen)
{
	ssize_t len;
	generation_t g;

	g = list_first_entry(gl, struct generation, list);
	len = decoder_get(g, (void *)dst, maxlen);

	if (len > 0)
		return len;

	if (len == EGENNOMORE && generation_advance(g->gl) > 0)
		return generation_decoder_get(g->gl, dst, maxlen);

	return EGENNOMORE;
}

session_t
generation_get_session(generation_t g)
{
	return g->session;
}

int
generation_local_flow_missing(const generation_t g)
{
	return (g->state.local->sdim - g->state.local->ddim);
}

int
generation_index(const generation_t g)
{
	int winsize;
	generation_t first;

	first = list_first_entry(g->gl, struct generation, list);
	winsize = generation_window_size(g->gl);

	return delta(g->seq, first->seq, winsize);
}

static int
cb_rtx(timeout_t t, u32 overrun, void *data)
{
	(void)t;
	(void)overrun;
	generation_t g;
	session_t s;
	g = data;
	s = g->session;
	struct itimerspec *it;
	struct timespec min_timeout;

	if (g->gentype == FORWARD) {
		if (generation_is_decoded(g))
			return 0;
		if (!rlnc_block_rank_decode(g->rb))
			return 0;
	} else {
		if (generation_local_flow_decoded(g))
			return 0;
	}

	if (overrun > 0)
		LOG(LOG_ERR, "rtx overrun = %d", overrun);

	timespecmset(&min_timeout, GENERATION_RTX_MIN_TIMEOUT);
	overrun++;
	do {
		tx_encoded_frame(s, g);
		rtx_inc(g);
		it = rtx_timeout(g);
		if (overrun > 0)
			overrun--;
	} while (timespeccmp(&it->it_value, &min_timeout, <) || overrun > 0);

	timeout_settime(g->task.rtx, 0, it);
	timeout_settime(g->task.ack, 0, NULL);

	return 0;
}

static int
cb_ack(timeout_t t, u32 overrun, void *data)
{
	(void) t;
	(void) overrun;
	generation_t g;
	session_t s;
	g = data;
	s = g->session;

	if (overrun > 0)
		LOG(LOG_ERR, "ack overrun = %d", overrun);

	if (qdelay_packet_cnt() > 10) {
		timeout_settime(g->task.ack, TIMEOUT_FLAG_SHORTEN,
			timeout_usec(0.5*1000,GENERATION_ACK_INTERVAL*1000));
		return 0;
	}

	tx_ack_frame(s, g);
	g->state.tx.ack++;

	timeout_settime(g->task.ack, 0, NULL);

	return 0;
}

int
generation_remaining_space(const struct list_head *gl)
{
	generation_t g;
	int space = 0;

	list_for_each_entry(g, gl, list)
		space += generation_encoder_space(g);

	return space;
}

int
generation_lseq(const struct list_head *gl)
{
	assert(gl->next != gl);
	return list_first_entry(gl, struct generation, list)->seq;
}

int
generation_seq(const generation_t g)
{
	return g->seq;
}


