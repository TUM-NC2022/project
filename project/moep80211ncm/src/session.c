#include <errno.h>
#include <math.h>

#include <moep/system.h>
#include <moep/modules/ieee8023.h>
#include <moep/modules/moep80211.h>

#include <moepcommon/list.h>
#include <moepcommon/util.h>
#include <moepcommon/timeout.h>

#include <jsm.h>

#include "global.h"
#include "generation.h"
#include "qdelay.h"
#include "session.h"
#include "ncm.h"
#include "neighbor.h"
#include "linkstate.h"

/**
 * Callbacks for timeouts
 */
int cb_destroy(timeout_t t, u32 overrun, void *data);
int cb_dequeue(struct jsm80211_module *module, void *packet, void *data);

struct session_rtt
{
	int rtt_mean; // RTT mean [usec]
	int rtt_sdev; // RTT sdev [usec]
};

// PZ
struct session_tasks tasks;

static int (*tx_encoded)(struct moep_frame *) = rad_tx;
static int (*tx_decoded)(struct moep_frame *) = tap_tx;

/**
 * Session list. Take care of it.
 */
static LIST_HEAD(sl);

static inline int
compare(struct session *s1, struct session *s2)
{
	return memcmp(s1->sid, s2->sid, sizeof(s1->sid));
}

u8 *
session_find_remote_address(struct session *s)
{
	int ret1, ret2;

	ret1 = memcmp(s->hwaddr.master, ncm_get_local_hwaddr(), IEEE80211_ALEN);
	ret2 = memcmp(s->hwaddr.slave, ncm_get_local_hwaddr(), IEEE80211_ALEN);

	if (ret1 == 0 && ret2 == 0)
		return NULL;
	else if (ret1 == 0)
		return s->hwaddr.slave;
	else
		return s->hwaddr.master;
}

static char *get_log_fn(session_t s)
{
	static char filename[1000];

	snprintf(filename, 1000, "%s%d_%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x.log",
			 SESSION_LOG_FILE_PREFIX,
			 getpid(),
			 s->sid[0], s->sid[1], s->sid[2], s->sid[3],
			 s->sid[4], s->sid[5], s->sid[6], s->sid[7],
			 s->sid[8], s->sid[9], s->sid[10], s->sid[11]);
	return filename;
}

static int
session_type(const u8 *sid)
{
	if (0 == memcmp(sid, ncm_get_local_hwaddr(), IEEE80211_ALEN))
		return MASTER;
	else if (0 == memcmp(sid + IEEE80211_ALEN, ncm_get_local_hwaddr(),
						 IEEE80211_ALEN))
		return SLAVE;
	else
		return FORWARD;
}

static void
session_destroy(struct session *s)
{
	list_del(&s->list);

	jsm80211_cleanup(s->jsm_module);

	generation_list_destroy(&s->gl);
	timeout_delete(s->task.destroy);

	unlink(get_log_fn(s));

	LOG(LOG_INFO, "session destroyed");

	free(s);
}

static void
init_coding_header(const struct session *s, const generation_t g,
				   struct ncm_hdr_coded *hdr)
{
	memcpy(hdr->sid, s->sid, sizeof(hdr->sid));
	hdr->lseq = generation_lseq(&s->gl);
	hdr->seq = generation_seq(g);
	hdr->gf = s->params.gftype;
	hdr->window_size = generation_window_size(&s->gl);
}

static int
serialize_for_encoding(void *buffer, size_t maxlen, struct moep_frame *f)
{
	size_t len;
	u8 *payload;
	struct moep_hdr_pctrl *pctrl;
	struct ether_header *ether;

	if (!(ether = moep_frame_ieee8023_hdr(f)))
		DIE("ether_header not found");

	payload = moep_frame_get_payload(f, &len);

	if (len + sizeof(*pctrl) > maxlen)
		DIE("unable to serialize frame for encoding (frame too long)");

	pctrl = buffer;
	pctrl->hdr.type = MOEP_HDR_PCTRL;
	pctrl->hdr.len = sizeof(*pctrl);
	pctrl->type = htole16(be16toh(ether->ether_type));
	pctrl->len = len;

	memcpy(buffer + sizeof(*pctrl), payload, len);

	return len + sizeof(*pctrl);
}

int session_sid(u8 *sid, const u8 *hwaddr1, const u8 *hwaddr2)
{
	int ret = memcmp(hwaddr1, hwaddr2, IEEE80211_ALEN);

	if (ret == 0)
	{
		errno = EINVAL;
		return -1;
	}

	if (ret < 0)
	{
		memcpy(sid, hwaddr1, IEEE80211_ALEN);
		memcpy(sid + IEEE80211_ALEN, hwaddr2, IEEE80211_ALEN);
	}
	else
	{
		memcpy(sid, hwaddr2, IEEE80211_ALEN);
		memcpy(sid + IEEE80211_ALEN, hwaddr1, IEEE80211_ALEN);
	}

	return 0;
}

session_t
session_find(const u8 *sid)
{
	struct session *tmp, *cur;

	list_for_each_entry_safe(cur, tmp, &sl, list)
	{
		if (0 == memcmp(sid, cur->sid, sizeof(cur->sid)))
			return cur;
	}

	return NULL;
}

session_t
session_register(const struct params_session *params,
				 const struct params_jsm *jsm, const u8 *sid)
{
	struct session *s;
	int i;

	if (!(s = calloc(1, sizeof(*s))))
		DIE("calloc() failed: %s", strerror(errno));

	s->params = *params;
	memcpy(s->sid, sid, sizeof(s->sid));

	s->gentype = session_type(s->sid);

	if (jsm)
	{
		if (0 > jsm80211_init(&s->jsm_module,
							  (struct jsm80211_parameters *)jsm, cb_dequeue, s))
			DIE("jsm80211_init() failed");
	}

	if (0 > timeout_create(CLOCK_MONOTONIC, &s->task.destroy, cb_destroy, s))
		DIE("timeout_create() failed: %s", strerror(errno));

	timeout_settime(s->task.destroy, 0, timeout_msec(SESSION_TIMEOUT, 0));

	INIT_LIST_HEAD(&s->gl);

	for (i = 0; i < s->params.winsize; i++)
	{
		(void)generation_init(s, &s->gl, s->gentype,
							  params->gftype, params->gensize, 8192, i);
	}

	list_add(&s->list, &sl);

	LOG(LOG_INFO, "new sesion created");

	return s;
}

void session_cleanup()
{
	struct session *tmp, *cur;

	list_for_each_entry_safe(cur, tmp, &sl, list)
	{
		session_destroy(cur);
	}
}

int tx_decoded_frame(struct session *s)
{
	moep_frame_t frame;
	ssize_t len;
	struct moep_hdr_pctrl *pctrl;
	struct ether_header *etherptr;
	u8 *hwaddr_remote;
	u8 buffer[8192];

	len = generation_decoder_get(&s->gl, buffer, sizeof(buffer));

	if (len == EGENNOMORE)
		return -1;

	if (0 >= len)
		DIE("gswin_decoder_get() failed: %d", (int)len);

	hwaddr_remote = session_find_remote_address(s);
	if (!hwaddr_remote)
		DIE("failed to dermine remote hwaddr");

	frame = moep_frame_ieee8023_create();

	etherptr = moep_frame_ieee8023_hdr(frame);
	memcpy(etherptr->ether_shost, hwaddr_remote, IEEE80211_ALEN);
	memcpy(etherptr->ether_dhost, ncm_get_local_hwaddr(), IEEE80211_ALEN);

	pctrl = (void *)buffer;
	etherptr->ether_type = htobe16(le16toh(pctrl->type));

	moep_frame_set_payload(frame, (void *)pctrl + sizeof(*pctrl), pctrl->len);

	if (s->jsm_module)
	{
		if (0 != jsm80211_queue(s->jsm_module, frame))
			DIE("jsm80211_queue() failed");
	}
	else
	{
		tx_decoded(frame);
		moep_frame_destroy(frame);
	}

	return 0;
}

int tx_encoded_frame(struct session *s, generation_t g)
{
	moep_frame_t frame;
	u8 payload[8192];
	struct ncm_hdr_coded *coded;
	struct generation_feedback *fb;
	struct moep80211_hdr *hdr;
	int count;
	size_t len;
	ssize_t ret;

	frame = create_rad_frame();

	count = generation_window_size(&s->gl);
	len = sizeof(*coded) + count * sizeof(*fb);

	coded = (struct ncm_hdr_coded *)
		moep_frame_add_moep_hdr_ext(frame, NCM_HDR_CODED, len);
	init_coding_header(s, g, coded);

	fb = coded->fb;
	if (0 > generation_feedback(g, fb, count * sizeof(*fb)))
		DIE("generation_feedback() failed: %s", strerror(errno));

	ret = generation_encoder_get(g, payload, sizeof(payload));
	if (0 > ret)
		DIE("generation_encoder_get() failed: %d", (int)ret);

	moep_frame_set_payload(frame, payload, ret);

	hdr = moep_frame_moep80211_hdr(frame);
	memset(hdr->ra, 0xff, IEEE80211_ALEN);
	memcpy(hdr->ta, ncm_get_local_hwaddr(), IEEE80211_ALEN);

	tx_encoded(frame);

	moep_frame_destroy(frame);

	return 0;
}

int tx_ack_frame(struct session *s, generation_t g)
{
	moep_frame_t frame;
	struct ncm_hdr_coded *coded;
	struct generation_feedback *fb;
	struct moep80211_hdr *hdr;
	int count;
	size_t len;

	frame = create_rad_frame();

	count = generation_window_size(&s->gl);
	len = sizeof(*coded) + count * sizeof(*fb);

	coded = (struct ncm_hdr_coded *)
		moep_frame_add_moep_hdr_ext(frame, NCM_HDR_CODED, len);
	init_coding_header(s, g, coded);

	fb = coded->fb;
	if (0 > generation_feedback(g, fb, count * sizeof(*fb)))
		DIE("generation_feedback() failed: %s", strerror(errno));

	hdr = moep_frame_moep80211_hdr(frame);
	memset(hdr->ra, 0xff, IEEE80211_ALEN);
	memcpy(hdr->ta, ncm_get_local_hwaddr(), IEEE80211_ALEN);

	tx_encoded(frame);

	moep_frame_destroy(frame);

	return 0;
}

void session_commit_state(struct session *s, const struct generation_state *state)
{
	s->state.count++;

	s->state.rx.data += state->rx.data;
	s->state.tx.data += state->tx.data;
	s->state.rx.ack += state->rx.ack;
	s->state.tx.ack += state->tx.ack;

	s->state.tx.redundant += state->tx.redundant;

	if (s->gentype != FORWARD)
		s->state.rx.excess_data += (state->rx.data - state->remote->sdim);

	return;
}

void session_decoder_add(struct session *s, moep_frame_t frame)
{
	struct ncm_hdr_coded *coded;
	size_t len;
	u8 *payload;

	timeout_settime(s->task.destroy, 0, timeout_msec(SESSION_TIMEOUT, 0));

	coded = (struct ncm_hdr_coded *)
		moep_frame_moep_hdr_ext(frame, NCM_HDR_CODED);
	payload = moep_frame_get_payload(frame, &len);

	if (NULL == generation_decoder_add(&s->gl, payload, len, coded))
	{
		if (len > 0)
			s->state.rx.late_data++;
		else
			s->state.rx.late_ack++;
	}
}

int session_encoder_add(struct session *s, moep_frame_t f)
{
	ssize_t len;
	static u8 buffer[4096];
	generation_t g;

	timeout_settime(s->task.destroy, 0, timeout_msec(SESSION_TIMEOUT, 0));

	len = serialize_for_encoding(buffer, sizeof(buffer), f);
	g = generation_encoder_add(&s->gl, buffer, len);

	if (!g)
	{
		LOG(LOG_WARNING, "session full, frame discarded");
		return -1;
	}

	return 0;
}

double
session_redundancy(session_t s)
{
	u8 *hwaddr, *master, *slave;
	const u8 relay[] = {0xde, 0xad, 0xbe, 0xef, 0x02, 0x03};
	double ret, rs, rm, ms, sm, x, y;

	if (s->params.rscheme == 2)
	{
		// RELAY SCHEME
		if (session_type(s->sid) == FORWARD)
		{
			master = s->hwaddr.master;
			slave = s->hwaddr.slave;

			rm = nb_ul_quality(master, NULL, NULL);
			rs = nb_ul_quality(slave, NULL, NULL);

			ms = ls_quality(master, slave, NULL, NULL);
			sm = ls_quality(slave, master, NULL, NULL);

			return max((1.0 - ms) / rs, (1.0 - sm) / rm);
		}
		else
		{
			if (!(hwaddr = session_find_remote_address(s)))
			{
				LOG(LOG_WARNING, "uplink_quality(): unable to find remote "
								 "address");
				return 0.0;
			}

			x = nb_ul_quality(hwaddr, NULL, NULL);
			if (nb_exists(relay))
			{
				y = nb_ul_quality(relay, NULL, NULL);
				return 1.0 / (1.0 - (1.0 - x) * (1.0 - y));
			}
			return 1.0 / x;
		}
	}

	// NON-RELAY SCHEME
	if (!(hwaddr = session_find_remote_address(s)))
	{
		LOG(LOG_WARNING, "uplink_quality(): unable to find remote "
						 "address");
		return 0.0;
	}

	if (s->params.rscheme == 0)
		ret = 1.0 / nb_ul_quality(hwaddr, NULL, NULL);
	else
		ret = nb_ul_redundancy(hwaddr);

	return ret;
}

int cb_destroy(timeout_t t, u32 overrun, void *data)
{
	(void)t;
	(void)overrun;
	session_t s = data;
	session_destroy(s);
	return 0;
}

int cb_dequeue(struct jsm80211_module *module, void *packet, void *data)
{
	(void)module;
	(void)data;
	tx_decoded(packet);
	moep_frame_destroy(packet);
	return 0;
}

void session_log_state()
{
	session_t s;
	char *filename;
	FILE *file;
	int p, q;
	double uplink, downlink;

	list_for_each_entry(s, &sl, list)
	{
		uplink = nb_ul_quality(session_find_remote_address(s), &p, &q),
		downlink = nb_dl_quality(session_find_remote_address(s), NULL, NULL),
		filename = get_log_fn(s);
		file = fopen(filename, "w");
		if (!file)
			DIE("cannot open file: %s", filename);
		fprintf(file,
				"session: %s:%s\n"
				"count\t%d\n"
				"tx.data\t%.2f\n"
				"tx.ack\t%.2f\n"
				"rx.data\t%.2f\n"
				"rx.ack\t%.2f\n"
				"rx.excess_data\t%.2f\n"
				"rx.late_data\t%.2f\n"
				"rx.late_ack\t%.f\n"
				"tx.redundant\t%.2f\n"
				"redundancy\t%.2f\n"
				"uplink\t%.2f\n"
				"p = %d, q = %d\n"
				"downlink\t%.2f\n"
				"qdelay\t%.4f\n"
				"\n",
				ether_ntoa((const struct ether_addr *)s),
				ether_ntoa((const struct ether_addr *)s + IEEE80211_ALEN),
				s->state.count,
				(double)s->state.tx.data / (double)s->state.count,
				(double)s->state.tx.ack / (double)s->state.count,
				(double)s->state.rx.data / (double)s->state.count,
				(double)s->state.rx.ack / (double)s->state.count,
				(double)s->state.rx.excess_data / (double)s->state.count,
				(double)s->state.rx.late_data / (double)s->state.count,
				(double)s->state.rx.late_ack / (double)s->state.count,
				(double)s->state.tx.redundant / (double)s->state.count,
				session_redundancy(s),
				uplink,
				p, q,
				downlink,
				qdelay_get());
		fclose(file);

		if (s->jsm_module)
			jsm80211_log_state(s->jsm_module);
	}
}

int session_remaining_space(const session_t s)
{
	return generation_remaining_space(&s->gl);
}

int session_min_remaining_space()
{
	session_t s;
	int ret = GENERATION_SIZE;

	list_for_each_entry(s, &sl, list)
		ret = min(session_remaining_space(s), ret);

	return ret;
}
