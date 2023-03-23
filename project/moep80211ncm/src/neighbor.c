#include <netinet/ether.h>

#include "neighbor.h"
#include "ralqe.h"

#define NB_COMMIT 1000
#define NB_TIMEOUT 60000

float ralqe_theta = RALQE_THETA;

struct neighbor
{
	struct list_head list;
	struct
	{
		timeout_t timeout;
		timeout_t commit;
	} task;
	u8 hwaddr[IEEE80211_ALEN];
	u16 lseq;
	ralqe_link_t ul;
	ralqe_link_t dl;
	double ulq;
};

LIST_HEAD(nl);

static struct neighbor *
find(const u8 *hwaddr)
{
	struct neighbor *cur;

	list_for_each_entry(cur, &nl, list)
	{
		if (0 == memcmp(hwaddr, cur->hwaddr, IEEE80211_ALEN))
			return cur;
	}

	return NULL;
}

static int
cb_commit(timeout_t t, u32 overrun, void *data)
{
	//(void) overrun;
	//(void) t;
	struct neighbor *nb = data;

	nb->ulq = ralqe_redundancy(nb->ul, ralqe_theta);
	// PZ
	// printf("%f\n", nb->ulq);

	// u8 new;
	// int total, rcvd;

	// if (nb->istat.rcvd == 0 && nb->istat.lost == 0)
	//	nb->istat.lost = NB_COMMIT / DEFAULT_BEACON_INTERVAL;

	// rcvd = nb->istat.rcvd;
	// total = rcvd + nb->istat.lost;
	// new = mult_frac(255,rcvd,total);

	// nb->dlq = WMEWMA_WEIGHT*(float)nb->dlq + (1.0-WMEWMA_WEIGHT)*(float)new;

	// memset(&nb->istat, 0, sizeof(nb->istat));

	// LOG(LOG_ERR, "nb: %s ulq=%u dlq=%u",
	//	ether_ntoa((const struct ether_addr *)nb->hwaddr),
	//	nb->ulq,nb->dlq);

	/*
	 * Timeout rearms itself
	 */
	return 0;
}

static int
cb_delete(timeout_t t, u32 overrun, void *data)
{
	(void)overrun;
	(void)t;
	struct neighbor *nb = data;

	nb_del(nb->hwaddr);
	return 0;
}

int nb_exists(const u8 *hwaddr)
{
	if (!find(hwaddr))
		return 0;
	return 1;
}

int nb_add(const u8 *hwaddr)
{
	struct neighbor *nb = find(hwaddr);

	if (nb)
	{
		errno = EEXIST;
		return -1;
	}

	nb = calloc(1, sizeof(struct neighbor));
	memcpy(nb->hwaddr, hwaddr, IEEE80211_ALEN);
	nb->ul = ralqe_link_new();
	nb->dl = ralqe_link_new();
	if (!nb->ul || !nb->dl)
		DIE("ralqe_link_new() failed.");

	if (0 > timeout_create(CLOCK_MONOTONIC, &nb->task.timeout, cb_delete, nb))
		DIE("timeout_create() failed: %s", strerror(errno));
	timeout_settime(nb->task.timeout, 0, timeout_msec(NB_TIMEOUT, 0));

	if (0 > timeout_create(CLOCK_MONOTONIC, &nb->task.commit, cb_commit, nb))
		DIE("timeout_create() failed: %s", strerror(errno));
	timeout_settime(nb->task.commit, 0, timeout_msec(NB_COMMIT, NB_COMMIT));

	list_add(&nb->list, &nl);

	LOG(LOG_ERR, "nb: new neighbor at %s",
		ether_ntoa((const struct ether_addr *)nb->hwaddr));

	return 0;
}

int nb_del(const u8 *hwaddr)
{
	struct neighbor *nb;

	if (!(nb = find(hwaddr)))
	{
		errno = ENODEV;
		return -1;
	}

	list_del(&nb->list);

	LOG(LOG_ERR, "nb: deleted neighbor %s (timeout)",
		ether_ntoa((const struct ether_addr *)nb->hwaddr));

	timeout_delete(nb->task.timeout);
	timeout_delete(nb->task.commit);
	ralqe_link_del(nb->ul);
	ralqe_link_del(nb->dl);
	free(nb);

	return 0;
}

int nb_update_seq(const u8 *hwaddr, u16 seq)
{
	struct neighbor *nb;
	int p, q;

	if (!(nb = find(hwaddr)))
	{
		if (0 > nb_add(hwaddr))
			DIE("nb_add() failed where it must not");
		if (!(nb = find(hwaddr)))
			DIE("nb_find() failed where it must not");
		nb->lseq = (u16)(seq - 1);
	}

	p = 1;
	q = (u16)(seq - nb->lseq - 1);
	if (q > UINT16_MAX / 2)
	{
		LOG(LOG_WARNING, "received out-of-order frame, skipping update");
		return 0;
	}
	nb->lseq = seq;
	ralqe_update(nb->dl, &p, &q);

	timeout_settime(nb->task.timeout, 0, timeout_msec(NB_TIMEOUT, 0));
	return 0;
}

int nb_update_ul(const u8 *hwaddr, int p, int q)
{
	struct neighbor *nb;

	if (!(nb = find(hwaddr)))
	{
		if (0 > nb_add(hwaddr))
			DIE("nb_add() failed where it must not");
		if (!(nb = find(hwaddr)))
			DIE("nb_find() failed where it must not");
	}

	ralqe_init(nb->ul, p, q);

	return 0;
}

double
nb_ul_redundancy(const u8 *hwaddr)
{
	struct neighbor *nb;

	if (!(nb = find(hwaddr)))
	{
		if (0 > nb_add(hwaddr))
			DIE("nb_add() failed where it must not");
		if (!(nb = find(hwaddr)))
			DIE("nb_find() failed where it must not");
	}

	return nb->ulq;
}

double
nb_ul_quality(const u8 *hwaddr, int *p, int *q)
{
	struct neighbor *nb;
	int _p, _q;

	if (!(nb = find(hwaddr)))
	{
		if (0 > nb_add(hwaddr))
			DIE("nb_add() failed where it must not");
		if (!(nb = find(hwaddr)))
			DIE("nb_find() failed where it must not");
	}

	_p = _q = 0;
	ralqe_update(nb->ul, &_p, &_q);

	if (p)
		*p = _p;
	if (q)
		*q = _q;

	return (double)_p / ((double)_p + (double)_q);
}

double
nb_dl_quality(const u8 *hwaddr, int *p, int *q)
{
	struct neighbor *nb;
	int _p, _q;

	if (!(nb = find(hwaddr)))
	{
		if (0 > nb_add(hwaddr))
			DIE("nb_add() failed where it must not");
		if (!(nb = find(hwaddr)))
			DIE("nb_find() failed where it must not");
	}

	_p = _q = 0;
	ralqe_update(nb->dl, &_p, &_q);

	if (p)
		*p = _p;
	if (q)
		*q = _q;

	return (double)_p / ((double)_p + (double)_q);
}

int nb_fill_dl(nb_dl_filler_t filler, void *data)
{
	struct neighbor *nb;
	int i;
	int p, q;

	i = 0;
	list_for_each_entry(nb, &nl, list)
	{
		p = 0;
		q = 0;
		ralqe_update(nb->dl, &p, &q);
		filler(data, i, nb->hwaddr, p, q);
		i++;
	}

	return i;
}
