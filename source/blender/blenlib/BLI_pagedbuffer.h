/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLI_PAGEDBUFFER_H
#define BLI_PAGEDBUFFER_H

/** \file BLI_pbuffer.h
 *  \ingroup bli
 *  \brief Management and access functions for paged buffers.
 */

#include "BLI_utildefines.h"

#include "DNA_pagedbuffer_types.h"


struct bPagedBuffer;
struct bPagedBufferLayerInfo;
struct bPagedBufferIterator;
struct bPagedBufferPage;


typedef struct bPagedBufferIterator
{
	int index;
	short valid;
	
	/* constants */
	int page_size;
	int index_end;

	/* internals */
	struct bPagedBufferPage *page;
	int page_index;
} bPagedBufferIterator;


/* Buffer Management */

void pbufInit(struct bPagedBuffer *pbuf, int page_size);
void pbufFree(struct bPagedBuffer *pbuf);
void pbufCopy(struct bPagedBuffer *to, struct bPagedBuffer *from);

void pbufSetPageSize(struct bPagedBuffer *pbuf, int new_page_size);

/* Layers */

struct bPagedBufferLayerInfo *pbufLayerAdd(struct bPagedBuffer *pbuf, const char *name, int stride, const void *default_value);
void pbufLayerRemove(struct bPagedBuffer *pbuf, struct bPagedBufferLayerInfo *layer);


/* Data Access */
bPagedBufferIterator pbufSetElements(struct bPagedBuffer *pbuf, int totelem);
bPagedBufferIterator pbufAppendElements(struct bPagedBuffer *pbuf, int num_elements);

typedef int (*bPagedBufferTestFunc)(struct bPagedBufferIterator *pit, void *userdata);
/* must return -1 if the searched element has lower index than the iterator, 1 if it has higher index and 0 if it is found. */
typedef int (*bPagedBufferSearchFunction)(struct bPagedBufferIterator *pit, void *userdata);
/* must return 1 if a comes before b */
typedef int (*bPagedBufferCompareFunction)(struct bPagedBufferIterator *a, struct bPagedBufferIterator *b);

void pbufReset(struct bPagedBuffer *pbuf);
void pbufFreeDeadPages(struct bPagedBuffer *pbuf, bPagedBufferTestFunc removetestfunc, void *userdata);
void pbufCompress(struct bPagedBuffer *pbuf, bPagedBufferTestFunc removetestfunc, void *userdata, struct bPagedBufferLayerInfo *origindex_layer);

struct bPagedBufferIterator pbufGetElement(struct bPagedBuffer *pbuf, int index);

/** Find an element using binary search
 * If a (partial) ordering is defined on the elements, this function can be used
 * to find an element using efficient binary search.
 * \param test The binary test function defining the ordering.
 * \param data Custom information to pass to the test function.
 * \param start_index Lower bound for the search space (index >= start_index).
 * \param end_index Upper bound for the search space (index < end_index).
 */
struct bPagedBufferIterator pbufBinarySearchElement(struct bPagedBuffer *pbuf, bPagedBufferSearchFunction testfunc, void *userdata, int start_index, int end_index);

#if 0
/* access functions for point cache */
void pbufCacheMerge(struct bPagedBuffer *pbuf, int start, bPagedBufferCompareFunction cmpfunc);
#endif

/* XXX these could be inlined for performance */
struct bPagedBufferIterator pit_init(struct bPagedBuffer *pbuf);
struct bPagedBufferIterator pit_init_at(struct bPagedBuffer *pbuf, int index);
void pit_next(struct bPagedBufferIterator *it);
void pit_prev(struct bPagedBufferIterator *it);
void pit_forward(struct bPagedBufferIterator *it, int delta);
void pit_backward(struct bPagedBufferIterator *it, int delta);
void pit_forward_to(struct bPagedBufferIterator *it, int index);
void pit_backward_to(struct bPagedBufferIterator *it, int index);
void pit_goto(struct bPagedBufferIterator *it, int index);


/* macro for fast, low-level access to raw data */
#define PBUF_GET_DATA_POINTER(iterator, datalayer, datatype) \
((datatype*)((iterator)->page->layers[(datalayer)->layer]) + (iterator)->page_index)

#define PBUF_GET_GENERIC_DATA_POINTER(iterator, datalayer) \
(void*)((char*)((iterator)->page->layers[(datalayer)->layer]) + (iterator)->page_index * (datalayer)->stride)

/* access functions for common data types */
BLI_INLINE int pit_get_int(struct bPagedBufferIterator *it, struct bPagedBufferLayerInfo *layer)
{
	return *PBUF_GET_DATA_POINTER(it, layer, int);
}
BLI_INLINE void pit_set_int(struct bPagedBufferIterator *it, struct bPagedBufferLayerInfo *layer, int value)
{
	*PBUF_GET_DATA_POINTER(it, layer, int) = value;
}

BLI_INLINE int pit_get_float(struct bPagedBufferIterator *it, struct bPagedBufferLayerInfo *layer)
{
	return *PBUF_GET_DATA_POINTER(it, layer, float);
}
BLI_INLINE void pit_set_float(struct bPagedBufferIterator *it, struct bPagedBufferLayerInfo *layer, float value)
{
	*PBUF_GET_DATA_POINTER(it, layer, float) = value;
}

#endif
