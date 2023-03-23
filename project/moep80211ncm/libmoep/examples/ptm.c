/*
 * Copyright 2013, 2014		Maurice Leclaire <leclaire@in.tum.de>
 *				Stephan M. Guenther <moepi@moepi.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * See COPYING for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <argp.h>
#include <endian.h>
#include <signal.h>

#include <sys/timerfd.h>

#include <arpa/inet.h>

#include <net/ethernet.h>

#include <netinet/in.h>

#include <moep/system.h>

#include <moep/modules/moep80211.h>
#include <moep/modules/ieee8023.h>

#include "../src/util.h"

#include "../src/modules/radio/radiotap.h"


const char *argp_program_version = "ptm " PACKAGE_VERSION;
const char *argp_program_bug_address = "<" PACKAGE_BUGREPORT ">";

static char args_doc[] = "IF FREQ";

static char doc[] =
"ptm - a packet transfer module for moep80211\n\n"
"  IF                         Use the radio interface with name IF\n"
"  FREQ                       Use the frequency FREQ [in Hz] for the radio\n"
"                             interface; You can use M for MHz.";

enum fix_args {
	FIX_ARG_IF = 0,
	FIX_ARG_FREQ = 1,
	FIX_ARG_CNT
};

static struct argp_option options[] = {
	{"hwaddr", 'a', "ADDR", 0, "Set the hardware address to ADDR"},
	{"beacon", 'b', "INTERVAL", 0, "Transmit beacons and use an interval of INTERVAL milliseconds"},
	{"gi", 'g', "s|l", 0, "Use [s]hort guard interval or [l]ong guard interval"},
	{"ht", 'h', "BANDWIDTH", OPTION_ARG_OPTIONAL, "Enable HT and optionally set the bandwidth to BANDWIDTH"},
	{"ipaddr", 'i', "ADDR", 0, "Set the ip address to ADDR"},
	{"mtu", 'm', "SIZE", 0, "Set the mtu to SIZE"},
	{"rate", 'r', "RATE|MCS", 0, "Set the legacy rate to RATE*500kbit/s or the mcs index to MCS if HT is used"},
	{}
};

static error_t parse_opt(int key, char *arg, struct argp_state *state);

static struct argp argp = {
	options,
	parse_opt,
	args_doc,
	doc
};


struct arguments {
	char *rad;
	u8 *addr;
	struct in_addr ip;
	int mtu;
	int beacon;
	int chan_width;
	u64 freq;
	u64 freq1;
	int rate;
	int gi;
} args;


static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *args = state->input;
	char *endptr = NULL;
	int chan_width;
	long long int freq;

	switch (key) {
	case 'a':
		if (!(args->addr = ieee80211_aton(arg)))
			argp_failure(state, 1, errno, "Invalid hardware address");
		break;
	case 'g':
		if (strlen(arg) != 1)
			argp_failure(state, 1, errno, "Invalid guard interval: %s", arg);
		switch (*arg) {
		case 's':
			args->gi = IEEE80211_RADIOTAP_MCS_SGI;
			break;
		case 'l':
			args->gi = 0;
			break;
		default:
			argp_failure(state, 1, errno, "Invalid guard interval: %s", arg);
		}
		break;
	case 'h':
		if (!arg) {
			args->chan_width = MOEP80211_CHAN_WIDTH_20;
			break;
		}
		chan_width = strtol(arg, &endptr, 0);
		switch (chan_width) {
		case 20:
			args->chan_width = MOEP80211_CHAN_WIDTH_20;
			break;
		case 40:
			args->chan_width = MOEP80211_CHAN_WIDTH_40;
			if (!endptr)
				argp_failure(state, 1, errno, "Invalid HT mode: %s", arg);
			switch (*(endptr++)) {
			case '+':
				args->freq1 += 10;
				break;
			case '-':
				args->freq1 += 10;
				break;
			default:
				argp_failure(state, 1, errno, "Invalid HT mode: %s", arg);
			}
			break;
		default:
			argp_failure(state, 1, errno, "Invalid HT mode: %s", arg);
		}
		if (endptr != NULL && endptr != arg + strlen(arg))
			argp_failure(state, 1, errno, "Invalid HT mode: %s", arg);
		break;
	case 'i':
		if (!(inet_aton(arg, &args->ip)))
			argp_failure(state, 1, errno, "Invalid ip address");
		break;
	case 'm':
		args->mtu = strtol(arg, &endptr, 0);
		if (endptr != NULL && endptr != arg + strlen(arg))
			argp_failure(state, 1, errno, "Invalid mtu: %s", arg);
		if (args->mtu <= 0)
			argp_failure(state, 1, errno, "Invalid mtu: %d", args->mtu);
		break;
	case 'b':
		args->beacon = strtol(arg, &endptr, 0);
		if (endptr != NULL && endptr != arg + strlen(arg))
			argp_failure(state, 1, errno, "Invalid beacon interval: %s", arg);
		if (args->beacon <= 0)
			argp_failure(state, 1, errno, "Invalid beacon interval: %d", args->beacon);
		break;
	case 'r':
		args->rate = strtol(arg, &endptr, 0);
		if (endptr != NULL && endptr != arg + strlen(arg))
			argp_failure(state, 1, errno, "Invalid data rate: %s", arg);
		if (args->rate < 0)
			argp_failure(state, 1, errno, "Invalid data rate: %d", args->rate);
		break;
	case ARGP_KEY_ARG:
		switch (state->arg_num) {
		case FIX_ARG_IF:
			args->rad = arg;
			break;
		case FIX_ARG_FREQ:
			freq = strtoll(arg, &endptr, 0);
			while (endptr != NULL && endptr != arg + strlen(arg)) {
				switch (*endptr) {
				case 'k':
				case 'K':
					freq *= 1000;
					break;
				case 'm':
				case 'M':
					freq *= 1000000;
					break;
				case 'g':
				case 'G':
					freq *= 1000000000;
					break;
				default:
					argp_failure(state, 1, errno, "Invalid frequency: %s", arg);
				}
				endptr++;
			}
			if (freq < 0)
				argp_failure(state, 1, errno, "Invalid frequency: %lld", freq);
			args->freq = freq / 1000000;
			args->freq1 += args->freq;
			break;
		default:
			argp_usage(state);
		}
		break;
	case ARGP_KEY_END:
		if (state->arg_num < FIX_ARG_CNT)
			argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}


#define MOEP_HDR_BEACON	MOEP_HDR_VENDOR_MIN


struct moep_hdr_beacon {
	struct moep_hdr_ext hdr;
} __attribute__((packed));


static moep_dev_t tap;
static moep_dev_t rad;


static int rad_tx(moep_frame_t frame)
{
	struct moep80211_radiotap *radiotap;

	if (!(radiotap = moep_frame_radiotap(frame))) {
		fprintf(stderr, "ptm: error: no radiotap header: %s\n", strerror(errno));
		moep_frame_destroy(frame);
		return -1;
	}
	radiotap->tx_flags = IEEE80211_RADIOTAP_F_TX_NOACK;
	radiotap->hdr.it_present |= BIT(IEEE80211_RADIOTAP_TX_FLAGS);
	if (args.chan_width == MOEP80211_CHAN_WIDTH_20_NOHT) {
		radiotap->rate = args.rate;
		radiotap->hdr.it_present |= BIT(IEEE80211_RADIOTAP_RATE);
	} else {
		radiotap->mcs.mcs = args.rate;
		if (args.chan_width == MOEP80211_CHAN_WIDTH_40)
			radiotap->mcs.flags = IEEE80211_RADIOTAP_MCS_BW_40;
		else
			radiotap->mcs.flags = IEEE80211_RADIOTAP_MCS_BW_20;
		radiotap->mcs.flags |= args.gi;
		radiotap->mcs.known = IEEE80211_RADIOTAP_MCS_HAVE_MCS
				   | IEEE80211_RADIOTAP_MCS_HAVE_BW
				   | IEEE80211_RADIOTAP_MCS_HAVE_GI;
		radiotap->hdr.it_present |= BIT(IEEE80211_RADIOTAP_MCS);
	}

	if (moep_dev_tx(rad, frame)) {
		fprintf(stderr, "ptm: error: failed to send frame: %s\n", strerror(errno));
		moep_frame_destroy(frame);
		return -1;
	}

	moep_frame_destroy(frame);
	return 0;
}

static int taph(moep_dev_t dev, moep_frame_t frame)
{
	struct moep80211_hdr *hdr;
	struct moep_hdr_pctrl *pctrl;
	struct ether_header ether, *etherptr;
	size_t len;

	if (!(etherptr = moep_frame_ieee8023_hdr(frame))) {
		fprintf(stderr, "ptm: error: no ether header: %s\n", strerror(errno));
		moep_frame_destroy(frame);
		return -1;
	}
	memcpy(&ether, etherptr, sizeof(ether));

	moep_dev_frame_convert(rad, frame);

	if (!(hdr = moep_frame_moep80211_hdr(frame))) {
		fprintf(stderr, "ptm: error: no moep80211 header: %s\n", strerror(errno));
		moep_frame_destroy(frame);
		return -1;
	}
	hdr->frame_control = htole16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_DATA);
	memcpy(hdr->ra, ether.ether_dhost, IEEE80211_ALEN);
	memcpy(hdr->ta, ether.ether_shost, IEEE80211_ALEN);

	if (!(pctrl = (struct moep_hdr_pctrl *)moep_frame_add_moep_hdr_ext(frame,
									   MOEP_HDR_PCTRL,
									   sizeof(*pctrl)))) {
		fprintf(stderr, "ptm: error: cannot add pctrl header: %s\n", strerror(errno));
		moep_frame_destroy(frame);
		return -1;
	}
	pctrl->type = htole16(be16toh(ether.ether_type));
	if (!moep_frame_get_payload(frame, &len)) {
		fprintf(stderr, "ptm: error: no payload: %s\n", strerror(errno));
		moep_frame_destroy(frame);
		return -1;
	}
	pctrl->len = htole16(len);

	return rad_tx(frame);
}

static int radh(moep_dev_t dev, moep_frame_t frame)
{
	struct moep80211_hdr *hdr;
	struct moep_hdr_beacon *beacon;
	struct moep_hdr_pctrl *pctrl;
	struct ether_header ether, *etherptr;

	if (!(hdr = moep_frame_moep80211_hdr(frame))) {
		fprintf(stderr, "ptm: error: no moep80211 header: %s\n", strerror(errno));
		moep_frame_destroy(frame);
		return -1;
	}

	if (!memcmp(hdr->ta, args.addr, IEEE80211_ALEN)) {
		moep_frame_destroy(frame);
		return 0;
	}

	if ((beacon = (struct moep_hdr_beacon *)moep_frame_moep_hdr_ext(frame,
									MOEP_HDR_BEACON))) {
		fprintf(stderr, "ptm: received beacon from %s\n", ieee80211_ntoa(hdr->ta));
		moep_frame_destroy(frame);
		return 0;
	}

	if (!(pctrl = (struct moep_hdr_pctrl *)moep_frame_moep_hdr_ext(frame,
								       MOEP_HDR_PCTRL))) {
		fprintf(stderr, "ptm: error: no pctrl header: %s\n", strerror(errno));
		moep_frame_destroy(frame);
		return 0;
	}

	memcpy(ether.ether_dhost, hdr->ra, IEEE80211_ALEN);
	memcpy(ether.ether_shost, hdr->ta, IEEE80211_ALEN);
	ether.ether_type = htobe16(le16toh(pctrl->type));

	if (!moep_frame_adjust_payload_len(frame, le16toh(pctrl->len))) {
		fprintf(stderr, "ptm: error: failed to adjust payload len: %s\n", strerror(errno));
		moep_frame_destroy(frame);
		return -1;
	}

	moep_dev_frame_convert(tap, frame);

	if (!(etherptr = moep_frame_ieee8023_hdr(frame))) {
		fprintf(stderr, "ptm: error: no ether header: %s\n", strerror(errno));
		moep_frame_destroy(frame);
		return -1;
	}
	memcpy(etherptr, &ether, sizeof(ether));

	if (moep_dev_tx(tap, frame)) {
		fprintf(stderr, "ptm: error: failed to send frame: %s\n", strerror(errno));
		moep_frame_destroy(frame);
		return -1;
	}

	moep_frame_destroy(frame);
	return 0;
}

static int send_beacon(int fd, u32 events, void *null)
{
	u64 overruns;
	moep_frame_t frame;
	struct moep80211_hdr *hdr;
	struct moep_hdr_beacon *beacon;

	if (read(fd, &overruns, 8) < 8) {
		fprintf(stderr, "ptm: error: %s\n", strerror(errno));
		return -1;
	}

	if (!(frame = moep_dev_frame_create(rad))) {
		fprintf(stderr, "ptm: cannot create frame: %s\n", strerror(errno));
		return -1;
	}

	if (!(hdr = moep_frame_moep80211_hdr(frame))) {
		fprintf(stderr, "ptm: error: no moep80211 header: %s\n", strerror(errno));
		moep_frame_destroy(frame);
		return -1;
	}
	hdr->frame_control = htole16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_DATA);
	memset(hdr->ra, 0xff, IEEE80211_ALEN);
	memcpy(hdr->ta, args.addr, IEEE80211_ALEN);

	if (!(beacon = (struct moep_hdr_beacon *)moep_frame_add_moep_hdr_ext(frame,
									     MOEP_HDR_BEACON,
									     sizeof(*beacon)))) {
		fprintf(stderr, "ptm: error: cannot create beacon header: %s\n", strerror(errno));
		moep_frame_destroy(frame);
		return -1;
	}

	return rad_tx(frame);
}

static int sigh(struct signalfd_siginfo *siginfo, void *null)
{
	if (siginfo->ssi_signo == SIGTERM || siginfo->ssi_signo == SIGINT) {
		errno = 0;
		return -1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	struct itimerspec interval;
	int bcn_timer;
	moep_callback_t bcn_callback;

	memset(&args, 0, sizeof(args));
	args.mtu = 1500;
	args.beacon = 0;
	args.chan_width = MOEP80211_CHAN_WIDTH_20_NOHT;
	args.freq1 = 0;
	args.rate = 12;
	args.gi = 0;

	argp_parse(&argp, argc, argv, 0, 0, &args);

	if (!(tap = moep_dev_ieee8023_tap_open(args.addr, &args.ip, 24,
					       args.mtu +
					       sizeof(struct ether_header)))) {
		fprintf(stderr, "ptm: error: %s\n", strerror(errno));
		return -1;
	}
	if (!(rad = moep_dev_moep80211_open(args.rad, args.freq,
					    args.chan_width, args.freq1, 0,
					    args.mtu + radiotap_len(-1) +
					    sizeof(struct moep80211_hdr) +
					    sizeof(struct moep_hdr_pctrl)))) {
		fprintf(stderr, "ptm: error: %s\n", strerror(errno));
		moep_dev_close(tap);
		return -1;
	}

	if (!args.addr) {
		if (!(args.addr = malloc(IEEE80211_ALEN))) {
			fprintf(stderr, "ptm: error: failed to allocate memory\n");
			moep_dev_close(rad);
			moep_dev_close(tap);
			return -1;
		}
		if (moep_dev_tap_get_hwaddr(tap, args.addr)) {
			fprintf(stderr, "ptm: error: failed to retrieve hardware address\n");
			free(args.addr);
			moep_dev_close(rad);
			moep_dev_close(tap);
			return -1;
		}
	}

	moep_dev_set_rx_handler(tap, taph);
	moep_dev_set_rx_handler(rad, radh);
	moep_dev_pair(tap, rad);

	bcn_callback = NULL;
	if (args.beacon > 0) {
		if ((bcn_timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)) < 0) {
			fprintf(stderr, "ptm: error: %s\n", strerror(errno));
			free(args.addr);
			moep_dev_close(rad);
			moep_dev_close(tap);
			return -1;
		}

		interval.it_interval.tv_sec = args.beacon / 1000;
		interval.it_interval.tv_nsec = (args.beacon % 1000) * 1000000;
		interval.it_value.tv_sec = interval.it_interval.tv_sec;
		interval.it_value.tv_nsec = interval.it_interval.tv_nsec;
		if (timerfd_settime(bcn_timer, 0, &interval, NULL)) {
			fprintf(stderr, "ptm: error: %s\n", strerror(errno));
			close(bcn_timer);
			free(args.addr);
			moep_dev_close(rad);
			moep_dev_close(tap);
			return -1;
		}

		if (!(bcn_callback = moep_callback_create(bcn_timer, send_beacon, NULL, EPOLLIN))) {
			fprintf(stderr, "ptm: error: %s\n", strerror(errno));
			close(bcn_timer);
			free(args.addr);
			moep_dev_close(rad);
			moep_dev_close(tap);
			return -1;
		}
	}

	moep_run(sigh, NULL);
	printf("%s\n", strerror(errno));

	if (bcn_callback) {
		moep_callback_delete(bcn_callback);
		close(bcn_timer);
	}
	free(args.addr);
	moep_dev_close(rad);
	moep_dev_close(tap);
	return 0;
}
