#include <netinet/ether.h>

#include "neighbor.h"
#include "linkstate.h"
#include "ralqe.h"

#define LINK_TIMEOUT 60000

struct hwaddr
{
	u8 addr[IEEE80211_ALEN];
};

struct linkstate
{
	struct list_head list;
	struct
	{
		timeout_t timeout;
	} task;
	u8 ta[IEEE80211_ALEN];
	u8 ra[IEEE80211_ALEN];
	ralqe_link_t lq;
};

LIST_HEAD(ll);

static struct linkstate *
find(const u8 *ta, const u8 *ra)
{
	struct linkstate *cur;

	list_for_each_entry(cur, &ll, list)
	{
		if (memcmp(ta, cur->ta, IEEE80211_ALEN))
			continue;

		if (0 == memcmp(ra, cur->ra, IEEE80211_ALEN))
			return cur;
	}

	return NULL;
}

static int
cb_delete(timeout_t t, u32 overrun, void *data)
{
	(void)overrun;
	(void)t;
	struct linkstate *ls = data;

	ls_del(ls->ta, ls->ra);
	return 0;
}

int ls_add(const u8 *ta, const u8 *ra)
{
	struct linkstate *ls = find(ta, ra);

	if (ls)
	{
		errno = EEXIST;
		return -1;
	}

	ls = calloc(1, sizeof(struct linkstate));
	memcpy(ls->ta, ta, IEEE80211_ALEN);
	memcpy(ls->ra, ra, IEEE80211_ALEN);
	ls->lq = ralqe_link_new();
	if (!ls->lq)
		DIE("ralqe_link_new() failed.");

	if (0 > timeout_create(CLOCK_MONOTONIC, &ls->task.timeout, cb_delete, ls))
		DIE("timeout_create() failed: %s", strerror(errno));
	timeout_settime(ls->task.timeout, 0, timeout_msec(LINK_TIMEOUT, 0));

	list_add(&ls->list, &ll);

	LOG(LOG_ERR, "ls: new linkstate %s->%s",
		ether_ntoa((const struct ether_addr *)ls->ta),
		ether_ntoa((const struct ether_addr *)ls->ra));

	return 0;
}

int ls_del(const u8 *ta, const u8 *ra)
{
	struct linkstate *ls;

	if (!(ls = find(ta, ra)))
	{
		errno = ENODEV;
		return -1;
	}

	list_del(&ls->list);

	LOG(LOG_ERR, "ls: deleted linkstate %s->%s",
		ether_ntoa((const struct ether_addr *)ls->ta),
		ether_ntoa((const struct ether_addr *)ls->ra));

	timeout_delete(ls->task.timeout);
	ralqe_link_del(ls->lq);
	free(ls);

	return 0;
}

int ls_update(const u8 *ta, const u8 *ra, int p, int q)
{
	struct linkstate *ls;

	if (!(ls = find(ta, ra)))
	{
		if (0 > ls_add(ta, ra))
			DIE("ls_add() failed where it must not");
		if (!(ls = find(ta, ra)))
			DIE("ls_find)() failed where it must not");
	}

	ralqe_init(ls->lq, p, q);
	timeout_settime(ls->task.timeout, 0, timeout_msec(LINK_TIMEOUT, 0));

	return 0;
}

double
ls_quality(const u8 *ta, const u8 *ra, int *p, int *q)
{
	struct linkstate *ls;
	int _p, _q;

	if (!(ls = find(ta, ra)))
	{
		if (0 > ls_add(ta, ra))
			DIE("ls_add() failed where it must not");
		if (!(ls = find(ta, ra)))
			DIE("ls_find)() failed where it must not");
	}

	_p = _q = 0;
	ralqe_update(ls->lq, &_p, &_q);

	if (p)
		*p = _p;
	if (q)
		*q = _q;

	// PZ
	printf("RALQE:");
	printf("%d", *p);
	printf("%d", *q);

	return (double)_p / (_p + _q);
}
