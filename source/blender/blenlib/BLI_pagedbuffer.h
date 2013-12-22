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

struct bPagedBuffer;

void BLI_pbuf_init(struct bPagedBuffer *pbuf, size_t page_bytes, size_t elem_bytes);
void BLI_pbuf_free(struct bPagedBuffer *pbuf);
void BLI_pbuf_copy(struct bPagedBuffer *to, struct bPagedBuffer *from);

void BLI_pbuf_add_elements(struct bPagedBuffer *pbuf, int num_elem);

void *BLI_pbuf_get(struct bPagedBuffer *pbuf, int index);


typedef struct bPagedBufferIterator
{
	struct bPagedBufferPage *page;
	void *data;
	int index, page_index;
} bPagedBufferIterator;

void BLI_pbuf_iter_init(struct bPagedBuffer *pbuf, struct bPagedBufferIterator *iter);
void BLI_pbuf_iter_next(struct bPagedBuffer *pbuf, struct bPagedBufferIterator *iter);
bool BLI_pbuf_iter_valid(struct bPagedBuffer *pbuf, struct bPagedBufferIterator *iter);
void BLI_pbuf_iter_at(struct bPagedBuffer *pbuf, struct bPagedBufferIterator *iter, int index);

#endif /* BLI_PAGEDBUFFER_H */
