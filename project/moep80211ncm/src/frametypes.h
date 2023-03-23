#ifndef __FRAMETYPES_H
#define __FRAMETYPES_H

#include <moep/system.h>
#include <moep/types.h>
#include <moep/ieee80211_addr.h>

#include "generation.h"

enum headertypes {
	NCM_HDR_DATA	= MOEP_HDR_VENDOR_MIN,
	NCM_HDR_CODED,
	NCM_HDR_BCAST,
	NCM_HDR_BEACON,
	NCM_HDR_INVALID	= MOEP_HDR_COUNT-1
};

enum frametypes {
	NCM_DATA = 0,
	NCM_CODED,
	NCM_BEACON,
	NCM_INVALID,
};

struct ncm_hdr_beacon {
	struct moep_hdr_ext hdr;
} __attribute__((packed));

struct ncm_beacon_payload {
	u8 mac[IEEE80211_ALEN];
	u16 p;
	u16 q;
} __attribute__((packed));

/**
  Bware: the coding header is a variable-length header, depending on the galois
  field and generation size.
  */
struct ncm_hdr_coded {
	struct moep_hdr_ext hdr;
	u8 sid[2*IEEE80211_ALEN];
	u8 gf:2;
	u8 window_size:6;
	u16 seq;
	u16 lseq;
	struct generation_feedback fb[0];
} __attribute__((packed));

struct ncm_hdr_bcast {
	struct moep_hdr_ext hdr;
	u32 id;
} __attribute__((packed));

#endif //__FRAMETYPES_H
