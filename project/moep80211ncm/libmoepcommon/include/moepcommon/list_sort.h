#include <string.h>

/*
 * Extracted from include/linux/kernel.h and corresponding headers.
 */

#ifndef MAX_LIST_LENGTH_BITS
#define MAX_LIST_LENGTH_BITS 20
#endif

#ifndef __same_type
#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))
#endif

#ifndef __must_be_array
#define __must_be_array(a)      __same_type((a), &(a)[0])
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))
#endif

static struct list_head *sort_list_merge(void *priv,
	int (*cmp)(void *priv, struct list_head *a, struct list_head *b),
	struct list_head *a, struct list_head *b)
{
	struct list_head head, *tail = &head;

	while (a && b) {
		if ((*cmp)(priv, a, b) <= 0) {
			tail->next = a;
			a = a->next;
		}
		else {
			tail ->next = b;
			b = b->next;
		}
		tail = tail->next;
	}

	tail->next = a?:b;

	return head.next;
}

static void sort_list_merge_and_restore_back_links(void *priv,
	int (*cmp)(void *priv, struct list_head *a, struct list_head *b),
	struct list_head *head, struct list_head *a, struct list_head *b)
{
	struct list_head *tail = head;
	u8 count = 0;

	while (a && b) {
		if ((*cmp)(priv, a, b) <= 0) {
			tail->next = a;
			a->prev = tail;
			a = a->next;
		}
		else {
			tail->next=b;
			b->prev = tail;
			b = b->next;
		}
		tail = tail->next;
	}

	tail->next = a?:b;

	do {
		if (!(++count))
			(*cmp)(priv, tail->next, tail->next);
		tail->next->prev = tail;
		tail = tail->next;
	} while (tail->next);

	tail->next = head;
	head->prev = tail;
}

static void
list_sort(void *priv, struct list_head *head,
	int (*cmp)(void *priv, struct list_head *a, struct list_head *b))
{
	struct list_head *part[MAX_LIST_LENGTH_BITS+1];
	int lev;
	int max_lev = 0;
	struct list_head *list;

	if (list_empty(head))
		return;

	memset(part, 0, sizeof(part));

	head->prev->next = NULL;
	list = head->next;

	while (list) {
		struct list_head *cur = list;
		list = list->next;
		cur->next = NULL;

		for (lev=0; part[lev]; lev++) {
			cur = __sort_list_merge(priv, cmp, part[lev], cur);
			part[lev] = NULL;
		}
		if (lev > max_lev) {
			if ((lev >= ARRAY_SIZE(part)-1))
				lev--;
			max_lev = lev;
		}
		part[lev] = cur;
	}

	for (lev=0; lev<max_lev; lev++) {
		if (part[lev])
			list = __sort_list_merge(priv, cmp, part[lev], list);
	}

	__sort_list_merge_and_restore_back_links(priv, cmp, head,
							part[max_lev], list);
}

