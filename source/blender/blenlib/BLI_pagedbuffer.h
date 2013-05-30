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

#include <stdlib.h>

#include "BLI_utildefines.h"

#include "DNA_pagedbuffer_types.h"






#if 0
/* Buffer Management */

void BLI_pbuf_init(struct bPagedBuffer *pbuf, int page_size);
void BLI_pbuf_free(struct bPagedBuffer *pbuf);
void BLI_pbuf_copy(struct bPagedBuffer *to, struct bPagedBuffer *from);

void BLI_pbuf_set_page_size(struct bPagedBuffer *pbuf, int new_page_size);

/* Layers */

struct bPagedBufferLayerInfo *BLI_pbuf_layer_add(struct bPagedBuffer *pbuf, const char *name, int stride, const void *default_value);
void BLI_pbuf_layer_remove(struct bPagedBuffer *pbuf, struct bPagedBufferLayerInfo *layer);


/* Data Access */
typedef struct bPagedBufferIterator
{
	int index;
	bool valid;
	
	/* constants */
	int page_size;
	int index_end;

	/* internals */
	struct bPagedBufferPage *page;
	int page_index;
} bPagedBufferIterator;

bPagedBufferIterator BLI_pbuf_set_elements(struct bPagedBuffer *pbuf, int totelem);
bPagedBufferIterator BLI_pbuf_append_elements(struct bPagedBuffer *pbuf, int num_elements);

typedef int (*bPagedBufferTestFunc)(struct bPagedBufferIterator *pit, void *userdata);
/* must return -1 if the searched element has lower index than the iterator, 1 if it has higher index and 0 if it is found. */
typedef int (*bPagedBufferSearchFunction)(struct bPagedBufferIterator *pit, void *userdata);
/* must return 1 if a comes before b */
typedef int (*bPagedBufferCompareFunction)(struct bPagedBufferIterator *a, struct bPagedBufferIterator *b);

void BLI_pbuf_reset(struct bPagedBuffer *pbuf);
void BLI_pbuf_free_dead_pages(struct bPagedBuffer *pbuf, bPagedBufferTestFunc removetestfunc, void *userdata);
void BLI_pbuf_compress(struct bPagedBuffer *pbuf, bPagedBufferTestFunc removetestfunc, void *userdata, struct bPagedBufferLayerInfo *origindex_layer);

/* macros for fast, low-level access to raw data */

#define PBUF_DATA_PTR(page_ptr, layer_index, datatype, page_index) \
	((page_ptr)->layers ? (datatype*) ((page_ptr)->layers[(layer_index)]) + (page_index) : NULL)

void *dummyptr;

#define PBUF_GET_DATA(result, page_ptr, layer_index, datatype, page_index, default_value) \
	if ((page_ptr)->layers) \
		result = ( (datatype*) ((page_ptr)->layers[(layer_index)]) )[(page_index)]; \
	else \
		result = (default_value);

#define PBUF_SET_DATA(page_ptr, layer_index, datatype, page_index, value) \
	if ((page_ptr)->layers) \
		( (datatype*) ((page_ptr)->layers[(layer_index)]) )[(page_index)] = value;

#define PBUF_GET_ELEMENT(result, pbuf, layer_index, datatype, elem_index, default_value) \
{ \
	int page_size = (pbuf)->page_size; \
	div_t result_p = div((index), page_size); \
	PBUF_GET_DATA(result, (pbuf)->pages + result_p.quot, (layer_index), datatype, (elem_index) - result_p.rem * page_size, default_value) \
}

#define PBUF_SET_ELEMENT(pbuf, layer_index, datatype, elem_index, value) \
{ \
	int page_size = (pbuf)->page_size; \
	div_t result_p = div((index), page_size); \
	PBUF_SET_DATA((pbuf)->pages + result_p.quot, (layer_index), datatype, (elem_index) - result_p.rem * page_size, value) \
}

/* XXX these could be inlined for performance */
struct bPagedBufferIterator BLI_pbuf_iter_init(struct bPagedBuffer *pbuf);
struct bPagedBufferIterator BLI_pbuf_iter_init_at(struct bPagedBuffer *pbuf, int index);
void BLI_pbuf_iter_next(struct bPagedBufferIterator *it);
void BLI_pbuf_iter_prev(struct bPagedBufferIterator *it);
void BLI_pbuf_iter_forward(struct bPagedBufferIterator *it, int delta);
void BLI_pbuf_iter_backward(struct bPagedBufferIterator *it, int delta);
void BLI_pbuf_iter_forward_to(struct bPagedBufferIterator *it, int index);
void BLI_pbuf_iter_backward_to(struct bPagedBufferIterator *it, int index);
void BLI_pbuf_iter_goto(struct bPagedBufferIterator *it, int index);

#define PBUF_ITER_GET_DATA(result, iter, layer_index, datatype, default_value) \
	PBUF_GET_DATA(result, iter.page, layer_index, datatype, iter.page_index, default_value)

#define PBUF_ITER_SET_DATA(iter, layer_index, datatype, value) \
	PBUF_SET_DATA(iter.page, layer_index, datatype, iter.page_index, value)

/** Find an element using binary search
 * If a (partial) ordering is defined on the elements, this function can be used
 * to find an element using efficient binary search.
 * \param test The binary test function defining the ordering.
 * \param data Custom information to pass to the test function.
 * \param start_index Lower bound for the search space (index >= start_index).
 * \param end_index Upper bound for the search space (index < end_index).
 */
struct bPagedBufferIterator BLI_pbuf_binary_search_element(struct bPagedBuffer *pbuf, bPagedBufferSearchFunction testfunc, void *userdata, int start_index, int end_index);

#if 0
/* access functions for point cache */
void BLI_pbuf_cache_merge(struct bPagedBuffer *pbuf, int start, bPagedBufferCompareFunction cmpfunc);
#endif

#define PBUF_DEF_LAYER_TYPE(ctype) \
	BLI_INLINE ctype BLI_pbuf_get_##ctype(struct bPagedBuffer *pbuf, int layer_index, int index, ctype default_value) { \
		ctype result; \
		PBUF_GET_ELEMENT(result, pbuf, layer_index, ctype, index, default_value) \
		return result; \
	} \
	BLI_INLINE void BLI_pbuf_set_##ctype(struct bPagedBuffer *pbuf, int layer_index, int index, ctype value) { \
		PBUF_SET_ELEMENT(pbuf, layer_index, ctype, index, value) \
	} \
	BLI_INLINE ctype BLI_pbuf_iter_get_##ctype(struct bPagedBufferIterator iter, int layer_index, ctype default_value) { \
		ctype result; \
		PBUF_ITER_GET_DATA(result, iter, layer_index, ctype, default_value) \
		return result; \
	} \
	BLI_INLINE void BLI_pbuf_iter_set_##ctype(struct bPagedBufferIterator iter, int layer_index, ctype value) { \
		PBUF_ITER_SET_DATA(iter, layer_index, ctype, value) \
	}

PBUF_DEF_LAYER_TYPE(float)
PBUF_DEF_LAYER_TYPE(int)
#endif

#endif
