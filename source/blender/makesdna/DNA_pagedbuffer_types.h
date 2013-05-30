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

#ifndef DNA_PAGEDBUFFER_H
#define DNA_PAGEDBUFFER_H

/** \file DNA_pagedbuffer.h
 *  \ingroup DNA
 *  \brief bPaged buffers are optimized for dynamic creation and removal
           of elements.
 */

#include "DNA_listBase.h"

typedef struct bPagedBufferPage {
	void *data;				/* layer data */
} bPagedBufferPage;

typedef struct bPagedBufferLayer {
	struct bPagedBufferPage *pages;		/* page list */
	int page_size;						/* elements per page */
	int totpages;						/* number of allocated pages */
	int totalloc;						/* actually allocated elements (dead pages not counted) */
} bPagedBufferLayer;

typedef struct bPagedBuffer {
	struct bPagedBufferLayer *layers;	/* layer list */
	int totlayers;						/* number of layers */
	int totelem;						/* number of elements in the buffer */
} bPagedBuffer;

#endif
