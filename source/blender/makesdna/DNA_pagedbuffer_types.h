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


typedef struct bPagedBufferLayerInfo {
	struct bPagedBufferLayerInfo *next, *prev;
	struct bPagedBufferLayerInfo *new_layer;	/* temporary pointer after copy */
	
	char name[32];
	
	int layer;							/* layer index */
	int stride;							/* size in bytes of a single element */
	
	/* default value when creating new elements */
	void *default_value;
} bPagedBufferLayerInfo;

typedef struct bPagedBufferPage {
	void **layers;				/* layer data */
} bPagedBufferPage;

typedef struct bPagedBuffer {
	struct bPagedBufferPage *pages;		/* page list */
	ListBase layers;					/* layer info list */
	int page_size;						/* elements per page */
	int totpages;						/* number of allocated pages */
	int totlayers;						/* number of data layers */
	int totelem;						/* number of added elements */
	int totalloc;						/* actually allocated elements (dead pages not counted) */
	int pad;
} bPagedBuffer;

#define MAX_PBUF_PROP_NAME             32

#endif
