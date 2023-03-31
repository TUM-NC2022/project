/*
 This file is part of moep80211
 (C) 2011-2013 Stephan M. Guenther (and other contributing authors)

 moep80211 is free software; you can redistribute it and/or modify it under the
 terms of the GNU General Public License as published by the Free Software
 Foundation; either version 3, or (at your option) any later version.

 moep80211 is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with
 moep80211; see the file COPYING.  If not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <argp.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sched.h>

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>

#include <moep/system.h>
#include <moep/types.h>
#include <moep/ieee80211_addr.h>
#include <moep/ieee80211_frametypes.h>
#include <moep/radiotap.h>
#include <moep/modules/ieee8023.h>
#include <moep/modules/moep80211.h>

#include <moepcommon/timeout.h>

#include <jsm.h>

#include "params.h"
#include "daemonize.h"
#include "frametypes.h"
#include <moepcommon/list.h>
#include <moepcommon/util.h>
#include "ncm.h"
#include "session.h"
#include "bcast.h"
#include "qdelay.h"
#include "neighbor.h"
#include "linkstate.h"
// PZ
#include "lqe.h"

#define TASK_NCM_BEACON 0

const char *argp_program_version = "ncm 2.0";
const char *argp_program_bug_address = "<guenther@tum.de>";

static char args_doc[] = "IF FREQ";

static int send_beacon(timeout_t t, u32 overrun, void *data);

/*
 * Argument parsing
 * ---------------------------------------------------------------------------
 */
static char doc[] =
	"ncm - the moep80211 network coding module\n\n"
	"  IF                         Use the radio interface with name IF\n"
	"  FREQ                       Use the frequency FREQ [in MHz] for the radio\n"
	"                             interface\n";

enum fix_args
{
	FIX_ARG_IF = 0,
	FIX_ARG_FREQ = 1,
	FIX_ARG_CNT
};

static struct argp_option options[] = {
	{.name = "hwaddr",
	 .key = 'a',
	 .arg = "ADDR",
	 .flags = 0,
	 .doc = "Set the hardware address to ADDR"},
	{.name = "ipaddr",
	 .key = 'i',
	 .arg = "ADDR",
	 .flags = 0,
	 .doc = "Set the ip address to ADDR"},
	{.name = "mtu",
	 .key = 'm',
	 .arg = "SIZE",
	 .flags = 0,
	 .doc = "Set the mtu to SIZE"},
	{.name = "daemon",
	 .key = 'd',
	 .arg = NULL,
	 .flags = 0,
	 .doc = "Run as daemon"},
	{.name = "rate",
	 .key = 'r',
	 .arg = "RATE | MCS",
	 .flags = 0,
	 .doc = "Set legacy RATE [r*500kbit/s] or MCS index"},
	{.name = "ht",
	 .key = 'h',
	 .arg = "HT",
	 .flags = 0,
	 .doc = "Set HT channel width"},
	{.name = "gi",
	 .key = 'g',
	 .arg = "GI",
	 .flags = 0,
	 .doc = "Set GI"},
	{.name = "jsm",
	 .key = 'j',
	 .arg = "PARAMS",
	 .flags = OPTION_ARG_OPTIONAL,
	 .doc = "Enable jitter suppression module and optionally "
			"specify suppression parameters in the form "
			"\"IPT-CUTOFF,ALPHA,BETA,QUANTILE,MAX-SCALING\""},
	{.name = "simulator",
	 .key = 'S',
	 .arg = NULL,
	 .flags = 0,
	 .doc = "Use simulator mode"},
	{.name = "tap-dev",
	 .key = 'T',
	 .arg = "PATH",
	 .flags = 0,
	 .doc = "Tap device to connect to the simulator"},
	{.name = "rad-dev",
	 .key = 'R',
	 .arg = "PATH",
	 .flags = 0,
	 .doc = "Radio device to connect to the simulator"},
	{.name = "gensize",
	 .key = 'G',
	 .arg = "GENSIZE",
	 .flags = 0,
	 .doc = "Generations size"},
	{.name = "winsize",
	 .key = 'W',
	 .arg = "WINSIZE",
	 .flags = 0,
	 .doc = "Generation sliding window size"},
	{.name = "fieldsize",
	 .key = 'F',
	 .arg = "FIELDSIZE",
	 .flags = 0,
	 .doc = "Size of the Galois field"},
	{.name = "redundancy-scheme",
	 .key = 's',
	 .arg = "SCHEME",
	 .flags = 0,
	 .doc = "redundancy scheme to use"},
	{.name = "confidence-level",
	 .key = 't',
	 .arg = "THETA",
	 .flags = 0,
	 .doc = "confidence level theta"},
	// PZ
	{.name = "link-quality-estimation",
	 .key = 'l',
	 .arg = "PORT",
	 .flags = 0,
	 .doc = "AI Link Quality Estimation"},
	{NULL}};

static error_t
parse_opt(int key, char *arg, struct argp_state *state);

static struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.args_doc = args_doc,
	.doc = doc};

static struct cfg
{
	int daemon;
	int mtu;
	u8 *hwaddr;
	struct in_addr ip;

	struct params_simulator sim;
	struct params_device tap;
	struct params_device rad;
	struct params_wireless wlan;
	struct params_session session;
	struct params_jsm jsm;

	// PZ
	struct lqe_socket lqe_socket;
} cfg;

static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
	struct cfg *cfg = state->input;
	char *endptr = NULL;
	long long int freq;
	int size;
	// PZ
	struct sockaddr_in address;

	switch (key)
	{
	case 'a':
		if (!(cfg->hwaddr = ieee80211_aton(arg)))
			argp_failure(state, 1, errno,
						 "Invalid hardware address");
		break;
	case 'd':
		cfg->daemon = 1;
		break;
	case 'i':
		if (!inet_aton(arg, &cfg->ip))
			argp_failure(state, 1, errno, "Invalid ip address");
		break;
	case 'm':
		cfg->mtu = strtol(arg, &endptr, 0);
		if (endptr != NULL && endptr != arg + strlen(arg))
			argp_failure(state, 1, errno, "Invalid mtu: %s", arg);
		if (cfg->mtu <= 0)
			argp_failure(state, 1, errno,
						 "Invalid mtu: %d", cfg->mtu);
		cfg->tap.mtu = cfg->mtu + sizeof(struct ether_header);
		cfg->rad.mtu = cfg->mtu + DEFAULT_MTU_OFFSET;
		break;
	case 'r':
		if (cfg->wlan.rt.it_present & BIT(IEEE80211_RADIOTAP_MCS))
		{
			cfg->wlan.rt.mcs.known |=
				IEEE80211_RADIOTAP_MCS_HAVE_MCS;
			cfg->wlan.rt.mcs.mcs = atoi(arg);
		}
		else
		{
			cfg->wlan.rt.it_present |=
				BIT(IEEE80211_RADIOTAP_RATE);
			cfg->wlan.rt.rate = atoi(arg);
		}
		break;
	case 'h':
		if (cfg->wlan.rt.it_present & BIT(IEEE80211_RADIOTAP_RATE))
		{
			cfg->wlan.rt.it_present &=
				~BIT(IEEE80211_RADIOTAP_RATE);
			cfg->wlan.rt.mcs.known |=
				IEEE80211_RADIOTAP_MCS_HAVE_MCS;
			cfg->wlan.rt.mcs.mcs = cfg->wlan.rt.rate;
			cfg->wlan.rt.rate = 0;
		}
		cfg->wlan.rt.it_present |= BIT(IEEE80211_RADIOTAP_MCS);
		cfg->wlan.rt.mcs.known |= IEEE80211_RADIOTAP_MCS_HAVE_BW;
		if (0 == strncasecmp(arg, "ht20", strlen(arg)))
		{
			cfg->wlan.rt.mcs.flags |= IEEE80211_RADIOTAP_MCS_BW_20;
			cfg->wlan.moep_chan_width = MOEP80211_CHAN_WIDTH_20;
			break;
		}

		if (strlen(arg) != strlen("ht40*"))
			argp_failure(state, 1, errno,
						 "Invalid HT bandwidth: %s", arg);

		if (0 == strncasecmp(arg, "ht40+", strlen(arg)))
		{
			cfg->wlan.rt.mcs.flags |= IEEE80211_RADIOTAP_MCS_BW_40;
			cfg->wlan.moep_chan_width = MOEP80211_CHAN_WIDTH_40;
			cfg->wlan.freq1 += 10;
			break;
		}
		else if (0 == strncasecmp(arg, "ht40-", strlen(arg)))
		{
			cfg->wlan.rt.mcs.flags |= IEEE80211_RADIOTAP_MCS_BW_40;
			cfg->wlan.moep_chan_width = MOEP80211_CHAN_WIDTH_40;
			cfg->wlan.freq1 -= 10;
			break;
		}

		argp_failure(state, 1, errno, "Invalid HT bandwidth: %s", arg);
		break;
	case 'g':
		if (cfg->wlan.rt.it_present & BIT(IEEE80211_RADIOTAP_RATE))
		{
			cfg->wlan.rt.it_present &=
				~BIT(IEEE80211_RADIOTAP_RATE);
			cfg->wlan.rt.mcs.known |=
				IEEE80211_RADIOTAP_MCS_HAVE_MCS;
			cfg->wlan.rt.mcs.mcs = cfg->wlan.rt.rate;
			cfg->wlan.rt.rate = 0;
		}
		cfg->wlan.rt.it_present |= BIT(IEEE80211_RADIOTAP_MCS);
		cfg->wlan.rt.mcs.known |= IEEE80211_RADIOTAP_MCS_HAVE_GI;
		if (atoi(arg) == 400)
			cfg->wlan.rt.mcs.flags |= IEEE80211_RADIOTAP_MCS_SGI;
		else if (atoi(arg) != 800)
			argp_failure(state, 1, errno, "Invalid GI: %s", arg);
		break;
	case 'j':
		// FIXME broken due to scfg struct
		if (arg)
		{
			int num = -1;
			sscanf(arg, "%lf,%lf,%lf,%lf,%lf%n",
				   &cfg->jsm.limit,
				   &cfg->jsm.alpha,
				   &cfg->jsm.beta,
				   &cfg->jsm.quantile,
				   &cfg->jsm.scale,
				   &num);
			if (num != (int)strlen(arg))
			{
				argp_error(state, "Invalid suppression "
								  "parameters: %s",
						   arg);
				return (EINVAL);
			}
			if (cfg->jsm.limit < 0.)
			{
				argp_failure(state, 1, 0, "Invalid "
										  "inter-packet time cutoff %lf (must "
										  "be positive)",
							 cfg->jsm.limit);
				return (EINVAL);
			}
			if (cfg->jsm.alpha < 0. || cfg->jsm.alpha > 1.)
			{
				argp_failure(state, 1, 0, "Invalid alpha "
										  "%lf (must be between 0.0 and 1.0)",
							 cfg->jsm.alpha);
				return (EINVAL);
			}
			if (cfg->jsm.beta < 0. || cfg->jsm.beta > 1.)
			{
				argp_failure(state, 1, 0, "Invalid beta %lf "
										  "(must be between 0.0 and 1.0)",
							 cfg->jsm.beta);
				return (EINVAL);
			}
			if (cfg->jsm.quantile < 0. || cfg->jsm.quantile > 1.)
			{
				argp_failure(state, 1, 0, "Invalid quantile "
										  "%lf (must be between 0.0 and 1.0)",
							 cfg->jsm.quantile);
				return (EINVAL);
			}
			if (cfg->jsm.scale < 0.)
			{
				argp_failure(state, 1, 0, "Invalid maximum "
										  "adjustment %lf (must be positive)",
							 cfg->jsm.scale);
				return (EINVAL);
			}
		}
		break;
	case 'S':
		cfg->sim.enabled = 1;
		break;
	case 'T':
		cfg->sim.sim_tap_dev = arg;
		break;
	case 'R':
		cfg->sim.sim_rad_dev = arg;
		break;
	case 'G':
		cfg->session.gensize = atoi(arg);
		if (cfg->session.gensize <= 1 || cfg->session.gensize > 254 || cfg->session.gensize % 2 != 0)
			argp_failure(state, 1, errno, "Invalid gensize: %s",
						 arg);
		break;
	case 'W':
		cfg->session.winsize = atoi(arg);
		if (cfg->session.winsize < 1 ||
			cfg->session.winsize > GENERATION_MAX_WINDOW)
			argp_failure(state, 1, errno, "Invalid winsize: %s",
						 arg);
		break;
	case 'F':
		size = atoi(arg);
		switch (size)
		{
		case 2:
			cfg->session.gftype = MOEPGF2;
			break;
		case 4:
			cfg->session.gftype = MOEPGF4;
			break;
		case 16:
			cfg->session.gftype = MOEPGF16;
			break;
		case 256:
			cfg->session.gftype = MOEPGF256;
			break;
		default:
			argp_failure(state, 1, errno, "Invalid fieldsize: %s",
						 arg);
		}
		break;
	case 's':
		cfg->session.rscheme = atoi(arg);
		break;
	case 't':
		sscanf(arg, "%f", &cfg->session.theta);
		ralqe_theta = cfg->session.theta;
		break;
	// PZ
	case 'l':
		cfg->lqe_socket.port = strtol(arg, &endptr, 0);
		if (endptr != NULL && endptr != arg + strlen(arg))
			argp_failure(state, 1, errno, "Invalid LQE port: %s", arg);

		LOG(LOG_INFO, "socket is starting on port %d!", cfg->lqe_socket.port);

		// Creating socket file descriptor
		if ((cfg->lqe_socket.client_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
		{
			LOG(LOG_INFO, "creation of socket failed");
			exit(EXIT_FAILURE);
		}

		// Set server address and port
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = inet_addr("localhost");
		address.sin_port = htons(cfg->lqe_socket.port);

		// Connect to the server
		if (connect(cfg->lqe_socket.client_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
		{
			LOG(LOG_INFO, "socket connect failed");
			exit(EXIT_FAILURE);
		}

		LOG(LOG_INFO, "socket is connected!");

		break;
	case ARGP_KEY_ARG:
		switch (state->arg_num)
		{
		case FIX_ARG_IF:
			cfg->rad.name = arg;
			break;
		case FIX_ARG_FREQ:
			freq = strtoll(arg, &endptr, 0);
			if (freq < 0)
				argp_failure(state, 1, errno,
							 "Invalid frequency: %lld", freq);
			cfg->wlan.freq0 = freq;
			cfg->wlan.freq1 += freq;
			break;
		default:
			argp_usage(state);
		}
		break;
	case ARGP_KEY_END:
		if (cfg->sim.enabled)
			break;
		if (state->arg_num < FIX_ARG_CNT)
			argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * /Argument parsing
 */

u8 *ncm_get_local_hwaddr()
{
	static u8 retval[IEEE80211_ALEN];
	memcpy(retval, cfg.hwaddr, IEEE80211_ALEN);
	return retval;
}

moep_frame_t create_rad_frame()
{
	return moep_dev_frame_create(cfg.rad.dev);
}

static void beacon_filler(struct ncm_beacon_payload *data, int i,
						  const u8 *hwaddr, int p, int q)
{
	memcpy(data[i].mac, hwaddr, IEEE80211_ALEN);
	data[i].p = p;
	data[i].q = q;
}

static int
send_beacon(timeout_t t, u32 overrun, void *data)
{
	(void)data;
	(void)t;
	(void)overrun;
	session_log_state();

	// PZ
	// Old location of the pushing of data via LQE socket

	moep_frame_t frame;
	struct moep80211_hdr *hdr;
	struct ncm_hdr_beacon *beacon;
	struct ncm_beacon_payload bcnp[255];
	int i;

	if (!(frame = create_rad_frame()))
		return 0;

	if (!(hdr = moep_frame_moep80211_hdr(frame)))
		goto end;

	memset(hdr->ra, 0xff, IEEE80211_ALEN);

	if (!(beacon = (void *)moep_frame_add_moep_hdr_ext(frame,
													   NCM_HDR_BEACON,
													   sizeof(*beacon))))
		goto end;

	i = nb_fill_dl((nb_dl_filler_t)beacon_filler, bcnp);

	if (!moep_frame_set_payload(frame, (u8 *)bcnp, i * sizeof(*bcnp)))
		goto end;

	rad_tx(frame);

end:
	moep_frame_destroy(frame);
	return 0;
}

static int
signal_handler(struct signalfd_siginfo *siginfo, void *null)
{
	if (siginfo->ssi_signo == SIGINT || siginfo->ssi_signo == SIGTERM)
	{
		errno = 0;
		return -1;
	}
	else if (siginfo->ssi_signo == SIGRTMIN &&
			 siginfo->ssi_code == SI_TIMER)
	{
		if (0 > timeout_exec((void *)siginfo->ssi_ptr,
							 siginfo->ssi_overrun))
		{
			LOG(LOG_ERR, "timeout_exec() failed");
		}
	}
	else
	{
		LOG(LOG_WARNING, "signal_handler(): unknown signal %d",
			siginfo->ssi_signo);
	}
	return 0;
}

static u16
txseq()
{
	static u16 seq = 0;
	return seq++;
}

static void
ncm_frame_init_l1hdr(moep_frame_t frame)
{
	struct moep80211_radiotap *rt;

	if (cfg.sim.enabled)
		return;

	rt = moep_frame_radiotap(frame);

	rt->hdr.it_present = cfg.wlan.rt.it_present;
	rt->rate = cfg.wlan.rt.rate;
	rt->mcs.known = cfg.wlan.rt.mcs.known;
	rt->mcs.flags = cfg.wlan.rt.mcs.flags;
	rt->mcs.mcs = cfg.wlan.rt.mcs.mcs;

	rt->hdr.it_present |= BIT(IEEE80211_RADIOTAP_TX_FLAGS);
	rt->tx_flags = IEEE80211_RADIOTAP_F_TX_NOACK;
}

static void
ncm_frame_init_l2hdr(moep_frame_t frame)
{
	struct moep80211_hdr *hdr;

	hdr = moep_frame_moep80211_hdr(frame);
	hdr->frame_control =
		htole16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_DATA);

	memcpy(hdr->ta, cfg.hwaddr, IEEE80211_ALEN);
}

static void
ncm_frame_set_txseq(moep_frame_t frame)
{
	struct moep80211_hdr *hdr;

	hdr = moep_frame_moep80211_hdr(frame);
	hdr->txseq = txseq();
	qdelay_store(hdr->txseq);
}

int ncm_frame_type(moep_frame_t frame)
{
	struct moep_hdr_ext *ext;

	ext = moep_frame_moep_hdr_ext(frame, NCM_HDR_CODED);
	if (ext)
		return NCM_CODED;

	ext = moep_frame_moep_hdr_ext(frame, NCM_HDR_BCAST);
	if (ext)
		return NCM_DATA;

	ext = moep_frame_moep_hdr_ext(frame, NCM_HDR_BEACON);
	if (ext)
		return NCM_BEACON;

	return NCM_INVALID;
}

int rad_tx(moep_frame_t f)
{
	int ret;

	ncm_frame_init_l1hdr(f);
	ncm_frame_init_l2hdr(f);
	ncm_frame_set_txseq(f);

	if (0 > (ret = moep_dev_tx(cfg.rad.dev, f)))
		LOG(LOG_ERR, "moep80211_tx() failed: %s", strerror(errno));

	return ret;
}

void write_csv_data(moep_frame_t f)
{
	static FILE *file = NULL;
	size_t len;
	struct timespec ts;

	if (!file)
		file = fopen("/dev/shm/taptraffic", "w");

	moep_frame_get_payload(f, &len);
	clock_gettime(CLOCK_MONOTONIC, &ts);

	fprintf(file, "%.6f, %lu\n", (float)ts.tv_sec + (float)ts.tv_nsec / 1000000000.0, len);
}

int tap_tx(moep_frame_t f)
{
	int ret;
	ret = moep_dev_tx(cfg.tap.dev, f);
	if (0 > ret)
	{
		LOG(LOG_ERR, "moep80211_tx() failed: %s", strerror(errno));
	}

	//	write_csv_data(f);

	return ret;
}

static int _set_tap_status(void *data, int status)
{
	status = status && session_min_remaining_space();
	return moep_dev_set_rx_status(cfg.tap.dev, status);
}

int set_tap_status()
{
	return _set_tap_status(NULL, moep_dev_get_tx_status(cfg.rad.dev));
}

static void
run()
{
	timeout_t beacon_timeout;

	if (0 > timeout_create(CLOCK_MONOTONIC, &beacon_timeout, send_beacon,
						   NULL))
		DIE("timeout_create() failed: %s", strerror(errno));
	timeout_settime(beacon_timeout, 0,
					timeout_msec(DEFAULT_BEACON_INTERVAL,
								 DEFAULT_BEACON_INTERVAL));

	moep_run(signal_handler, NULL);

	session_cleanup();

	timeout_delete(beacon_timeout);
}

static int
taph(moep_dev_t dev, moep_frame_t frame)
{
	(void)dev;
	u8 sid[2 * IEEE80211_ALEN];
	struct ether_header *etherptr, ether;
	struct moep80211_hdr *hdr;
	struct moep_hdr_pctrl *pctrl;
	struct ncm_hdr_bcast *bcast;
	size_t len;
	session_t s;

	etherptr = moep_frame_ieee8023_hdr(frame);
	memcpy(&ether, etherptr, sizeof(ether));

	if (is_bcast_mac(ether.ether_dhost) || is_mcast_mac(ether.ether_dhost))
	{
		moep_dev_frame_convert(cfg.rad.dev, frame);

		bcast = (struct ncm_hdr_bcast *)
			moep_frame_add_moep_hdr_ext(
				frame, NCM_HDR_BCAST, sizeof(*bcast));

		if (!bcast)
		{
			DIE("moep_frame_add_moep_hdr_ext() failed: %s",
				strerror(errno));
		}

		bcast->id = rand();
		hdr = moep_frame_moep80211_hdr(frame);
		memcpy(hdr->ra, ether.ether_dhost, IEEE80211_ALEN);

		pctrl = (struct moep_hdr_pctrl *)
			moep_frame_add_moep_hdr_ext(frame,
										MOEP_HDR_PCTRL, sizeof(*pctrl));

		pctrl->type = htole16(be16toh(ether.ether_type));
		moep_frame_get_payload(frame, &len);
		pctrl->len = htole16(len);

		rad_tx(frame);
		moep_frame_destroy(frame);
		return 0;
	}

	if (0 > session_sid(sid, ether.ether_shost, ether.ether_dhost))
		DIE("session_sid() failed: %s", strerror(errno));

	if (!(s = session_find(sid)))
		s = session_register(&cfg.session, NULL, sid);

	session_encoder_add(s, frame);

	set_tap_status();

	moep_frame_destroy(frame);
	return 0;
}

static int
radh(moep_dev_t dev, moep_frame_t frame)
{
	(void)dev;

	int type;
	struct moep80211_hdr *hdr;
	struct moep_hdr_pctrl *pctrl;
	struct ncm_hdr_bcast *bcast;
	struct ncm_hdr_coded *coded;
	struct ncm_beacon_payload *bcnp;
	struct moep80211_radiotap *rt;
	struct ether_header *etherptr, ether;
	size_t len;
	session_t s;

	hdr = moep_frame_moep80211_hdr(frame);
	if (0 == memcmp(hdr->ta, cfg.hwaddr, IEEE80211_ALEN))
	{
		qdelay_update(hdr->txseq);
		goto end;
	}

	(void)nb_update_seq(hdr->ta, hdr->txseq);

	(void)moep_frame_get_payload(frame, &len);

	if (!cfg.sim.enabled)
	{
		rt = moep_frame_radiotap(frame);
		if (rt->flags & IEEE80211_RADIOTAP_F_FCS)
		{
			if (len < 4)
			{
				LOG(LOG_ERR, "after clipping assumed FCS, "
							 "payload is negative (payload_len = "
							 "%lu), ignoring frame",
					len);
				goto end;
			}
			len -= 4;
			(void)moep_frame_adjust_payload_len(frame, len);
			rt->flags &= ~IEEE80211_RADIOTAP_F_FCS;
		}
	}

	type = ncm_frame_type(frame);

	switch (type)
	{
	case NCM_DATA:
		bcast = (struct ncm_hdr_bcast *)
			moep_frame_moep_hdr_ext(frame, NCM_HDR_BCAST);

		if (bcast && !bcast_known(bcast->id))
		{
			bcast_add(bcast->id);
			rad_tx(frame);
		}

		pctrl = (struct moep_hdr_pctrl *)
			moep_frame_moep_hdr_ext(frame, MOEP_HDR_PCTRL);

		memcpy(ether.ether_dhost, hdr->ra, IEEE80211_ALEN);
		memcpy(ether.ether_shost, hdr->ta, IEEE80211_ALEN);
		ether.ether_type = htobe16(le16toh(pctrl->type));

		moep_dev_frame_convert(cfg.tap.dev, frame);

		etherptr = moep_frame_ieee8023_hdr(frame);
		memcpy(etherptr, &ether, sizeof(ether));

		tap_tx(frame);
		break;

	case NCM_CODED:
		coded = (struct ncm_hdr_coded *)
			moep_frame_moep_hdr_ext(frame, NCM_HDR_CODED);

		if (!(s = session_find(coded->sid)))
			s = session_register(&cfg.session, NULL, coded->sid);

		// PZ - always pushes the LQE updates on the socket (location makes sense?)
		// This now only gets triggered when actual data is received (e.g. via ping cmd)
		if (cfg.lqe_socket.client_fd != -1 && rt != NULL)
		{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
			lqe_push_data(s, rt, cfg.lqe_socket.client_fd);
#pragma GCC diagnostic pop
		}

		// PZ - write RSSI, noise etc. into the session here?

		// PZ
		// Get RSSI from radiotap header
		// LOG(LOG_INFO, "Signal: %hhd", rt->signal);
		// LOG(LOG_INFO, "Noise: %hhd\n", rt->noise);
		// LOG(LOG_INFO, "Rate: %hhu", rt->rate);
		// LOG(LOG_INFO, "Signal_DB: %hhu", rt->signal_dB);	// close to 0, not sure if it is relevant
		// LOG(LOG_INFO, "Noise_DB: %hhu\n", rt->noise_dB);	// close to 0, not sure if it is relevant
		//  LOG(LOG_INFO, "TX power: %hhd\n", rt->tx_power);	// always 0, not useful (is it because TX = transmission power=)
		// rt->signal;	   // RF signal power at the antenna. This field contains a single signed 8-bit value, which indicates the RF signal power at the antenna, in decibels difference from 1mW.
		// rt->noise;	   // RF noise power at the antenna. This field contains a single signed 8-bit value, which indicates the RF signal power at the antenna, in decibels difference from 1mW.
		// rt->signal_dB; // RF signal power at the antenna, decibel difference from an arbitrary, fixed reference. This field contains a single unsigned 8-bit value.
		// rt->noise_dB;  // RF noise power at the antenna, decibel difference from an arbitrary, fixed reference. This field contains a single unsigned 8-bit value.

		session_decoder_add(s, frame);
		break;

	case NCM_BEACON:
		bcnp = (void *)moep_frame_get_payload(frame, &len);

		while (len / sizeof(*bcnp))
		{
			if (!memcmp(bcnp->mac, cfg.hwaddr, IEEE80211_ALEN))
			{
				nb_update_ul(hdr->ta, bcnp->p, bcnp->q);
			}
			else
			{
				ls_update(bcnp->mac, hdr->ta, bcnp->p, bcnp->q);
			}

			bcnp++;
			len -= sizeof(*bcnp);
		}

		break;

	default:
		LOG(LOG_ERR, "invalid frame type received");
		goto end;
		break;
	}

end:
	moep_frame_destroy(frame);

	set_tap_status();

	return 0;
}

void cfg_init()
{
	memset(&cfg, 0, sizeof(cfg));

	cfg.daemon = 0;
	cfg.mtu = DEFAULT_MTU;

	cfg.sim.enabled = 0;
	cfg.sim.sim_tap_dev = NULL;
	cfg.sim.sim_rad_dev = NULL;

	cfg.jsm.limit = 0.1;
	cfg.jsm.alpha = 0.0005;
	cfg.jsm.beta = 0.0005;
	cfg.jsm.quantile = 0.999;
	cfg.jsm.scale = 1.1;

	cfg.session.gensize = GENERATION_SIZE;
	cfg.session.winsize = GENERATION_WINDOW;
	cfg.session.gftype = MOEPGF;

	cfg.tap.name = "tap0";
	cfg.tap.mtu = cfg.mtu + sizeof(struct ether_header);

	cfg.rad.name = "wlan0";
	cfg.rad.mtu = cfg.mtu + DEFAULT_MTU_OFFSET;

	cfg.wlan.freq0 = 2412;
	cfg.wlan.freq1 = 0;
	cfg.wlan.moep_chan_width = MOEP80211_CHAN_WIDTH_20;
	cfg.wlan.rt.it_present = BIT(IEEE80211_RADIOTAP_MCS) | BIT(IEEE80211_RADIOTAP_TX_FLAGS);
	cfg.wlan.rt.mcs.known = IEEE80211_RADIOTAP_MCS_HAVE_MCS | IEEE80211_RADIOTAP_MCS_HAVE_BW;
	cfg.wlan.rt.mcs.mcs = 0;
	cfg.wlan.rt.mcs.flags = IEEE80211_RADIOTAP_MCS_BW_20;

	// PZ
	cfg.lqe_socket.client_fd = -1;
	cfg.lqe_socket.port = -1;
}

static int
check_timer_resoluton()
{
	struct timespec ts;
	u64 res;

	clock_getres(CLOCK_MONOTONIC, &ts);
	res = ts.tv_sec * 1000 * 1000 + ts.tv_nsec / 1000;

	if (!res)
	{
		LOG(LOG_INFO, "timer resultion is %lu nsec [OK]", ts.tv_nsec);
		return 0;
	}

	LOG(LOG_WARNING, "timer resultion is %ld usec which may "
					 "cause problem - fix your timers",
		res);
	return -1;
}

/*
 * Set realtime scheduler. This is probably superfluous in most cases but may
 * be neccessary for jitter suppresion or other tasks requiring reliable
 * sub-millisecond timer resultions.
 */
static inline int
set_realtime_scheduler()
{
	struct sched_param param = {1};

	if (0 > sched_setscheduler(getpid(), SCHED_FIFO, &param))
		DIE("sched_setscheduler() failed: %s", strerror(errno));
}

int main(int argc, char **argv)
{
	int ret;

	LOG(LOG_ERR, "hdr len = %d", NCM_HDRLEN_CODED_TOTAL);

	LOG(LOG_INFO, "ncm starting...");

	(void)check_timer_resoluton();
	cfg_init();

	argp_parse(&argp, argc, argv, 0, 0, &cfg);

	if (cfg.daemon)
	{
		daemonize();
	}
	else
	{
		openlog("moep80211ncm", LOG_PID | LOG_PERROR, LOG_USER);
		setlogmask(LOG_UPTO(LOG_DEBUG));
	}

	if (!(cfg.tap.dev = moep_dev_ieee8023_tap_open(cfg.hwaddr,
												   &cfg.ip, 24, cfg.tap.mtu)))
	{
		LOG(LOG_ERR, "moep80211_tap_open() failed: %s",
			strerror(errno));
		return -1;
	}
	if (cfg.sim.enabled)
	{
		if (!(cfg.rad.dev = moep_dev_moep80211_unix_open(
				  cfg.sim.sim_rad_dev, cfg.rad.mtu)))
		{
			DIE("cannot open radio dev: %s", strerror(errno));
		}
	}
	else
	{
		if (!(cfg.rad.dev = moep_dev_moep80211_open(cfg.rad.name,
													cfg.wlan.freq0,
													cfg.wlan.moep_chan_width,
													cfg.wlan.freq1, 0,
													cfg.rad.mtu)))
		{
			LOG(LOG_ERR, "moep80211_rad_open() failed: %s",
				strerror(errno));
			moep_dev_close(cfg.tap.dev);
			return -1;
		}
	}
	if (!cfg.hwaddr)
	{
		cfg.hwaddr = malloc(IEEE80211_ALEN);
		if (moep_dev_tap_get_hwaddr(cfg.tap.dev, cfg.hwaddr))
		{
			LOG(LOG_ERR,
				"moep80211_tap_get_hwaddr() failed: %s",
				strerror(errno));
		}
		LOG(LOG_ERR, "got hwaddr: %s",
			ether_ntoa((const struct ether_addr *)cfg.hwaddr));
	}

	moep_dev_set_rx_handler(cfg.tap.dev, taph);
	moep_dev_set_rx_handler(cfg.rad.dev, radh);

	moep_dev_set_tx_status_cb(cfg.tap.dev,
							  (dev_status_cb)moep_dev_set_rx_status,
							  cfg.rad.dev);
	moep_dev_set_tx_status_cb(cfg.rad.dev,
							  _set_tap_status,
							  NULL);

	run();

	moep_dev_close(cfg.rad.dev);
	moep_dev_close(cfg.tap.dev);

	// PZ
	if (cfg.lqe_socket.client_fd != -1)
	{
		close(cfg.lqe_socket.client_fd);
	}

	return ret;
}
