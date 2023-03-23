#ifndef __PARAMS_H_
#define __PARAMS_H_

#include <sys/types.h>

#include <moepgf/moepgf.h>
#include "global.h"

#ifdef IEEE80211_ALEN
#define IEEE80211_ALEN 6
#endif

struct params_jsm {
//	int	enabled;
	double	limit;
	double	alpha;
	double	beta;
	double	quantile;
	double	scale;
};

struct params_session {
	int			gensize;
	int			winsize;
	int			rscheme;
	float			theta;
	enum MOEPGF_TYPE	gftype;
};

struct params_wireless {
	u64	freq0;
	u64	freq1;
	u64	moep_chan_width;
	struct {
		u32	it_present;
		u8	rate;
		struct {
			u8 known;
			u8 flags;
			u8 mcs;
		} mcs;
	} rt;
};

struct params_device {
	char 	*name;
	size_t	mtu;
	moep_dev_t dev;
	int	tx_rdy;
	int	rx_rdy;
};

struct params_simulator {
	int	enabled;
	char	*sim_tap_dev;
	char	*sim_rad_dev;
};

#endif
