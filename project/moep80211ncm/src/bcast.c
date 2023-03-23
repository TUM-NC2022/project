#include <malloc.h>
#include <sys/types.h>
#include "bcast.h"
#include <errno.h>

#include <moepcommon/list.h>
#include <moepcommon/util.h>

struct bcast {
	struct list_head list;
	u32 id;
};

static LIST_HEAD(bl);

int
bcast_add(u32 id)
{
	struct bcast *tmp, *cur;
	int count = 0;

	list_for_each_entry_safe(cur, tmp, &bl, list) {
		if (cur->id == id) {
			return 1;
		}
		else if (count < 32)  {	
			count++;
		}
		else {
			list_del(&cur->list);
			free(cur);
		}
	}

	if (!(tmp = calloc(1, sizeof(*tmp))))
		DIE("calloc() failed: %s", strerror(errno));

	tmp->id = id;
	list_add(&tmp->list, &bl);

	return 0;
}

int
bcast_known(u32 id)
{
	struct bcast *cur;

	list_for_each_entry(cur, &bl, list) {
		if (cur->id == id)
			return 1;
	}

	return 0;
}


