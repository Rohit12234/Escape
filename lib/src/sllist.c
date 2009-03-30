/**
 * @version		$Id$
 * @author		Nils Asmussen <nils@script-solution.de>
 * @copyright	2008 Nils Asmussen
 */


#ifdef IN_KERNEL
#	include "../../kernel/h/common.h"
#	include "../../kernel/h/kheap.h"
#	include "../../kernel/h/video.h"
/* for panic (vassert) */
#	include "../../kernel/h/util.h"
#	define sllprintf	vid_printf
#	define free(x)		kheap_free(x)
#	define malloc(x)	kheap_alloc(x)
#else
#	include "../../libc/esc/h/common.h"
#	include "../../libc/esc/h/heap.h"
#	include "../../libc/esc/h/debug.h"
/* for exit (vassert) */
#	include "../../libc/esc/h/proc.h"
#	define sllprintf debugf
#endif

#include "../h/assert.h"
#include "../h/sllist.h"

/* a node in a list */
typedef struct sNode sNode;
struct sNode {
	sNode *next;
	const void *data;
};

/* represents a list */
typedef struct {
	sNode *first;
	sNode *last;
	u32 length;
} sList;

/**
 * Searches for the node at given index
 *
 * @param list the list
 * @param index the index
 * @return the node
 */
static sNode *sll_getNode(sSLList *list,u32 index);

sSLList *sll_create(void) {
	sList *l = malloc(sizeof(sList));
	if(l == NULL)
		return NULL;

	l->first = NULL;
	l->last = NULL;
	l->length = 0;
	return (sSLList*)l;
}

void sll_destroy(sSLList *list,bool freeData) {
	/* free nodes */
	sList *l = (sList*)list;
	sNode *nn,*n = l->first;

	vassert(list != NULL,"list == NULL");

	while(n != NULL) {
		nn = n->next;
		if(freeData)
			free((void*)n->data);
		free(n);
		n = nn;
	}
	/* free list */
	free(list);
}

sSLNode *sll_begin(sSLList *list) {
	return (sSLNode*)sll_getNode(list,0);
}

sSLNode *sll_nodeAt(sSLList *list,u32 index) {
	return (sSLNode*)sll_getNode(list,index);
}

u32 sll_length(sSLList *list) {
	sList *l = (sList*)list;
	if(l == NULL)
		return 0;
	return l->length;
}

s32 sll_indexOf(sSLList *list,const void *data) {
	sList *l = (sList*)list;
	sNode *n = l->first;
	s32 i;
	for(i = 0; n != NULL; i++) {
		if(n->data == data)
			return i;
		n = n->next;
	}
	return -1;
}

sSLNode *sll_nodeWith(sSLList *list,const void *data) {
	sList *l = (sList*)list;
	sNode *n = l->first;
	while(n != NULL) {
		if(n->data == data)
			return (sSLNode*)n;
		n = n->next;
	}
	return NULL;
}

void *sll_get(sSLList *list,u32 index) {
	return (void*)sll_getNode(list,index)->data;
}

void sll_set(sSLList *list,const void *data,u32 index) {
	sNode *n;
	n = sll_getNode(list,index);
	n->data = data;
}

bool sll_append(sSLList *list,const void *data) {
	sList *l = (sList*)list;
	return sll_insert(list,data,l->length);
}

bool sll_insert(sSLList *list,const void *data,u32 index) {
	sList *l = (sList*)list;
	sNode *nn,*n = l->first,*ln = NULL;

	vassert(list != NULL,"list == NULL");

	/* walk to the desired position */
	if(index == l->length) {
		n = NULL;
		ln = l->last;
	}
	else {
		while(index-- > 0) {
			ln = n;
			n = n->next;
		}
	}

	/* allocate node? */
	nn = malloc(sizeof(sNode));
	if(nn == NULL)
		return false;

	/* insert */
	nn->data = data;
	if(ln != NULL)
		ln->next = nn;
	else
		l->first = nn;
	if(n != NULL)
		nn->next = n;
	else {
		l->last = nn;
		nn->next = NULL;
	}
	l->length++;

	return true;
}

void sll_removeAll(sSLList *list) {
	sList *l = (sList*)list;
	sNode *m,*n = (sNode*)l->first;

	vassert(list != NULL,"list == NULL");

	/* free all nodes */
	while(n != NULL) {
		m = n->next;
		free(n);
		n = m;
	}

	/* adjust list-properties */
	l->length = 0;
	l->first = NULL;
	l->last = NULL;
}

void sll_removeNode(sSLList *list,sSLNode *node,sSLNode *prev) {
	sList *l = (sList*)list;
	sNode *n = (sNode*)node,*ln = (sNode*)prev;

	vassert(list != NULL,"list == NULL");
	vassert(node != NULL,"node == NULL");
	vassert(ln == NULL || ln->next == n,"<prev> is not the previous node of <node>!");

	/* remove */
	if(ln != NULL)
		ln->next = n->next;
	else
		l->first = n->next;
	if(n->next == NULL)
		l->last = ln;
	l->length--;

	/* free */
	free(n);
}

void sll_removeFirst(sSLList *list,const void *data) {
	sList *l = (sList*)list;
	sNode *n = l->first,*ln = NULL;

	vassert(list != NULL,"list == NULL");

	if(data != NULL) {
		while(n != NULL) {
			if(n->data == data)
				break;
			ln = n;
			n = n->next;
		}
	}

	/* ignore */
	if(n == NULL)
		return;

	sll_removeNode(list,(sSLNode*)n,(sSLNode*)ln);
}

void sll_removeIndex(sSLList *list,u32 index) {
	sList *l = (sList*)list;
	sNode *n = l->first,*ln = NULL;

	vassert(list != NULL,"list == NULL");

	/* walk to the desired position */
	while(index-- > 0) {
		ln = n;
		n = n->next;
	}

	/* ignore */
	if(n == NULL)
		return;

	sll_removeNode(list,(sSLNode*)n,(sSLNode*)ln);
}

static sNode *sll_getNode(sSLList *list,u32 index) {
	sList *l = (sList*)list;
	sNode *n;

	vassert(list != NULL,"list == NULL");
	vassert(index <= l->length,"The index %d does not exist",index);

	/* is it the last one? */
	if(index == l->length - 1)
		return l->last;

	/* walk to position */
	n = l->first;
	while(index-- > 0)
		n = n->next;
	return n;
}


/* #### TEST/DEBUG FUNCTIONS #### */
#if DEBUGGING

void sll_dbg_print(sSLList *list) {
	sList *l = (sList*)list;
	sNode *n = l->first;

	vassert(list != NULL,"list == NULL");

	sllprintf("Linked list @ 0x%x\n",list);
	while(n != NULL) {
		sllprintf("\t[0x%x] data=0x%x, next=0x%x\n",n,n->data,n->next);
		n = n->next;
	}
}

#endif
