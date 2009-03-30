/**
 * @version		$Id: sllist.h 77 2008-11-22 22:27:35Z nasmussen $
 * @author		Nils Asmussen <nils@script-solution.de>
 * @copyright	2008 Nils Asmussen
 */

#ifndef SLLIST_H_
#define SLLIST_H_

#ifdef IN_KERNEL
#	include "../../kernel/h/common.h"
#else
#	include "../../libc/esc/h/common.h"
#endif

/* our list (the user should not know about the internal structure) */
typedef void* sSLList;

/* a node in the list (public view) */
typedef struct sSLNode sSLNode;
struct sSLNode {
	/* the user should not be able to change them */
	sSLNode *const next;
	void *const data;
};

/**
 * Creates a new list
 *
 * @return the list or NULL if there is not enough mem
 */
sSLList *sll_create(void);

/**
 * Destroyes the given list
 *
 * @param list the list
 * @param freeData wether the data of the nodes should be free'd. Requires that they are allocated
 * 	on the heap
 */
void sll_destroy(sSLList *list,bool freeData);

/**
 * @param list the list
 * @return the number of elements in the list
 */
u32 sll_length(sSLList *list);

/**
 * Returns the first node in the list. That allows you to iterate through it:
 * <code>
 * sSLNode *n;
 * for(n = sll_begin(list); n != NULL; n = n->next) {
 *   do something with n->data
 * }
 * </code>
 *
 * @param list the list
 * @return the first node (NULL if the list is empty)
 */
sSLNode *sll_begin(sSLList *list);

/**
 * Returns the node at given index. See sll_begin().
 * Note that the index HAS TO exist!
 *
 * @param list the list
 * @param index the index
 * @return the node at given index
 */
sSLNode *sll_nodeAt(sSLList *list,u32 index);

/**
 * Determines the index of the given data
 *
 * @param list the list
 * @param data the data to search for
 * @return the index of the first matching node or -1 if not found
 */
s32 sll_indexOf(sSLList *list,const void *data);

/**
 * Determines the node with given data
 *
 * @param list the list
 * @param data the data to search for
 * @return the first matching node or NULL if not found
 */
sSLNode *sll_nodeWith(sSLList *list,const void *data);

/**
 * Searches for the element at given index. First and last one can be found in O(1).
 *
 * @param list the list
 * @param index the index
 * @return the data at the given index
 */
void *sll_get(sSLList *list,u32 index);

/**
 * Searches for the element at given index and sets the data to the given one. First and last
 * one can be found in O(1).
 *
 * @param list the list
 * @param data the new data (NULL is not allowed!)
 * @param index the index
 */
void sll_set(sSLList *list,const void *data,u32 index);

/**
 * Appends the given data to the list. This can be done in O(1).
 *
 * @param list the list
 * @param data the data (NULL is not allowed!)
 * @return true if successfull (otherwise not enough mem)
 */
bool sll_append(sSLList *list,const void *data);

/**
 * Inserts the given data at the given index to the list. This can be done in O(1) for the first
 * and last index.
 *
 * @param list the list
 * @param data the data (NULL is not allowed!)
 * @param index the index
 * @return true if successfull (otherwise not enough mem)
 */
bool sll_insert(sSLList *list,const void *data,u32 index);

/**
 * Removes all elements from the list
 *
 * @param list the list
 */
void sll_removeAll(sSLList *list);

/**
 * Removes the given node from the list. This is a faster alternative to sll_removeIndex()
 * because it is not required to loop through the list.
 *
 * @param list the list
 * @param node the node to remove
 * @param prev the previous node of <node>. NULL if there is no previous
 */
void sll_removeNode(sSLList *list,sSLNode *node,sSLNode *prev);

/**
 * Removes the first found element with given data. If the data is NULL the first element will
 * be removed.
 *
 * @param list the list
 * @param data the data to search for
 */
void sll_removeFirst(sSLList *list,const void *data);

/**
 * Removes the given index from the list
 *
 * @param list the list
 * @param index the index
 */
void sll_removeIndex(sSLList *list,u32 index);

#if DEBUGGING

/**
 * Prints the given list
 *
 * @param list the list
 */
void sll_dbg_print(sSLList *list);

#endif

#endif /* SLLIST_H_ */
