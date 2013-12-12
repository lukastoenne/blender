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

#include <assert.h>

#include "MEM_guardedalloc.h"
#include "memory.h"

#include "DNA_pagedbuffer_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_pagedbuffer.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

/* XXX how to handle alignment nicely? */
/*#define PBUF_ALIGN_STRICT*/

BLI_INLINE div_t div_ceil(int a, int b)
{
	div_t result = div(a, b);
	if (result.rem > 0)
		result.quot += 1;
	return result;
}

static int pbuf_page_size_from_bytes(size_t page_bytes, size_t elem_bytes)
{
	div_t page_size = div(page_bytes, elem_bytes);
	#ifdef PBUF_ALIGN_STRICT
	BLI_assert(page_size.rem == 0);
	#endif
	return page_size.quot;
}

static void pbuf_page_alloc(bPagedBuffer *pbuf, int p)
{
	bPagedBufferPage *page = &pbuf->pages[p];
	if (!page->data) {
		page->data = MEM_mallocN(pbuf->page_bytes, "paged buffer page");
		pbuf->totalloc += pbuf->page_size;
	}
}

static void pbuf_page_free(bPagedBuffer *pbuf, int p)
{
	bPagedBufferPage *page = &pbuf->pages[p];
	if (page->data) {
		MEM_freeN(page->data);
		pbuf->totalloc -= pbuf->page_size;
	}
}

typedef enum {
	PBUF_PAGE_ALLOC_NONE,	/* don't allocate any page data */
	PBUF_PAGE_ALLOC_EXTEND,	/* allocate only new page data */
	PBUF_PAGE_ALLOC_ALL		/* allocate all page data */
} ePagedBufferPageAlloc;

static void pbuf_set_totelem(bPagedBuffer *pbuf, int totelem, ePagedBufferPageAlloc alloc_mode)
{
	div_t div_elem = div_ceil(totelem, pbuf->page_size);
	int totpages = div_elem.quot;
	//	int pagefill = div_elem.rem;
	if (totpages == 0) {
		pbuf->pages = NULL;
		pbuf->totpages = 0;
		pbuf->totelem = 0;
		pbuf->totalloc = 0;
	}
	else {
		bPagedBufferPage *pages = MEM_callocN(sizeof(bPagedBufferPage) * totpages, "paged buffer page array");
		int startp = totelem / pbuf->page_size;
		int p;
		
		if (pbuf->pages) {
			int copyp = min_ii(pbuf->totpages, totpages);
			memcpy(pages, pbuf->pages, sizeof(bPagedBufferPage) * copyp);
			MEM_freeN(pbuf->pages);
		}
		
		pbuf->pages = pages;
		pbuf->totpages = totpages;
		pbuf->totelem = totelem;
		
		/* init data */
		switch (alloc_mode) {
			case PBUF_PAGE_ALLOC_EXTEND:
				for (p = startp; p < totpages; ++p)
					pbuf_page_alloc(pbuf, p);
				break;
			case PBUF_PAGE_ALLOC_ALL:
				for (p = 0; p < totpages; ++p)
					pbuf_page_alloc(pbuf, p);
				break;
			case PBUF_PAGE_ALLOC_NONE:
				/* nothing to do, data pointers are NULL already */
				break;
		}
	}
}


void BLI_pbuf_init(bPagedBuffer *pbuf, size_t page_bytes, size_t elem_bytes)
{
	pbuf->page_bytes = page_bytes;
	pbuf->elem_bytes = elem_bytes;
	pbuf->page_size = pbuf_page_size_from_bytes(page_bytes, elem_bytes);
	
	pbuf->pages = NULL;
	pbuf->totpages = 0;
	pbuf->totelem = 0;
	pbuf->totalloc = 0;
}

void BLI_pbuf_free(bPagedBuffer *pbuf)
{
	if (pbuf->pages) {
		int p;
		for (p = 0; p < pbuf->totpages; ++p)
			pbuf_page_free(pbuf, p);
		MEM_freeN(pbuf->pages);
		pbuf->pages = NULL;
		pbuf->totpages = 0;
	}
	pbuf->totelem = 0;
}

void BLI_pbuf_copy(bPagedBuffer *to, bPagedBuffer *from)
{
	to->page_bytes = from->page_bytes;
	to->elem_bytes = from->elem_bytes;
	to->page_size = from->page_size;
	
	if (from->pages) {
		int p;
		to->pages = MEM_dupallocN(from->pages);
		for (p = 0; p < from->totpages; ++p)
			if (from->pages[p].data)
				to->pages[p].data = MEM_dupallocN(from->pages[p].data);
	}
	else {
		to->pages = NULL;
	}
	to->totpages = from->totpages;
	to->totelem = from->totelem;
	to->totalloc = from->totalloc;
}


void BLI_pbuf_add_elements(bPagedBuffer *pbuf, int num_elem)
{
	int ntotelem = pbuf->totelem + num_elem;
	pbuf_set_totelem(pbuf, ntotelem, PBUF_PAGE_ALLOC_EXTEND);
}

void *BLI_pbuf_get(bPagedBuffer *pbuf, int index)
{
	if (index < pbuf->totelem) {
		div_t page_div = div(index, pbuf->page_size);
		bPagedBufferPage *page = pbuf->pages + page_div.quot;
		if (page->data)
			return (char *)page->data + pbuf->elem_bytes * page_div.rem;
		else
			return NULL;
	}
	else
		return NULL;
}


void BLI_pbuf_iter_init(bPagedBuffer *pbuf, bPagedBufferIterator *iter)
{
	iter->index = 0;
	iter->page = pbuf->pages;
	iter->page_index = 0;
	while (iter->page->data == NULL && iter->index < pbuf->totelem) {
		iter->index += pbuf->page_size;
		++iter->page;
	}
	iter->data = iter->page->data;
}

void BLI_pbuf_iter_next(bPagedBuffer *pbuf, bPagedBufferIterator *iter)
{
	++iter->index;
	++iter->page_index;
	if (iter->page_index < pbuf->page_size) {
		iter->data = (char *)iter->data + pbuf->elem_bytes;
	}
	else {
		++iter->page;
		iter->page_index = 0;
		while (iter->page->data == NULL && iter->index < pbuf->totelem) {
			iter->index += pbuf->page_size;
			++iter->page;
		}
		iter->data = iter->page->data;
	}
}

bool BLI_pbuf_iter_valid(bPagedBuffer *pbuf, bPagedBufferIterator *iter)
{
	return iter->index < pbuf->totelem;
}


#if 0
/************************************************/
/*				Buffer Management				*/
/************************************************/

static void set_default_data(void *data, int totdata, bPagedBufferLayerInfo *layer)
{
	int i;
	/* set default values */
	for (i=0; i < totdata; ++i)
		memcpy((char*)data + i*layer->stride, layer->default_value, layer->stride);
}

static void *make_page_layer(int page_size, bPagedBufferLayerInfo *layer)
{
	void *data= MEM_callocN(page_size * layer->stride, "bPagedBufferPage layer");
	set_default_data(data, page_size, layer);
	return data;
}

static void init_page(bPagedBuffer *pbuf, bPagedBufferPage *page, int page_size)
{
	bPagedBufferLayerInfo *layer;
	
	page->layers= MEM_callocN(pbuf->totlayers*sizeof(void*), "bPagedBufferPage layers");
	for (layer=pbuf->layers.first; layer; layer=layer->next)
		page->layers[layer->layer] = make_page_layer(page_size, layer);
}

static void free_page(bPagedBuffer *pbuf, bPagedBufferPage *page)
{
	if (page->layers) {
		int i;
		for (i=0; i < pbuf->totlayers; ++i)
			MEM_freeN(page->layers[i]);
		MEM_freeN(page->layers);
		page->layers = NULL;
	}
}

static void free_all_pages(bPagedBuffer *pbuf)
{
	bPagedBufferPage *page;
	int p;
	
	for (p=0, page=pbuf->pages; p < pbuf->totpages; ++p, ++page)
		free_page(pbuf, page);
}


static bPagedBufferLayerInfo *copy_layer_info(bPagedBufferLayerInfo *layer)
{
	bPagedBufferLayerInfo *nlayer = MEM_dupallocN(layer);
	nlayer->next = nlayer->prev = NULL;
	
	nlayer->default_value = MEM_dupallocN(layer->default_value);
	
	/* temporary copy pointer for easy mapping */
	layer->new_layer = nlayer;
	
	return nlayer;
}

static void free_layer_info(bPagedBufferLayerInfo *layer)
{
	MEM_freeN(layer->default_value);
}

/* Generic update function for layer indices.
 * Note: This could be optimized slightly for each use case,
 * but not necessary as layer changes are not performance critical.
 *
 * Warning!! This does not create/delete actual page layer data, this must be done separately!
 */
static void update_layer_indices(bPagedBuffer *pbuf)
{
	bPagedBufferLayerInfo *layer;
	int index = 0;
	
	for (layer=pbuf->layers.first; layer; layer=layer->next) {
		/* assign layer index */
		layer->layer = index;
		++index;
	}
	pbuf->totlayers = index;
}

void BLI_pbuf_init(bPagedBuffer *pbuf, int page_size)
{
	pbuf->page_size= page_size;
	pbuf->pages= NULL;
	pbuf->totpages= 0;
	pbuf->totelem= pbuf->totalloc= 0;
	
	pbuf->layers.first = pbuf->layers.last = NULL;
	pbuf->totlayers = 0;
}

void BLI_pbuf_free(bPagedBuffer *pbuf)
{
	bPagedBufferLayerInfo *layer, *layer_next;
	
	if (pbuf->pages) {
		free_all_pages(pbuf);
		MEM_freeN(pbuf->pages);
		pbuf->pages = NULL;
	}
	pbuf->totpages = 0;
	pbuf->totelem = pbuf->totalloc = 0;
	
	for (layer=pbuf->layers.first; layer; layer=layer_next) {
		layer_next = layer->next;
		
		free_layer_info(layer);
		MEM_freeN(layer);
	}
}

void BLI_pbuf_copy(bPagedBuffer *to, bPagedBuffer *from)
{
	bPagedBufferLayerInfo *layer;
	int p, k;
	
	*to = *from;
	to->layers.first = to->layers.last = NULL;
	for (layer=from->layers.first; layer; layer=layer->next) {
		BLI_addtail(&to->layers, copy_layer_info(layer));
	}
	update_layer_indices(to);
	
	if (from->pages) {
		to->pages = MEM_dupallocN(from->pages);
		for (p=0; p < from->totpages; ++p) {
			if (from->pages[p].layers) {
				to->pages[p].layers = MEM_dupallocN(from->pages[p].layers);
				for (k=0; k < from->totlayers; ++k) {
					to->pages[p].layers[k] = MEM_dupallocN(from->pages[p].layers[k]);
				}
			}
		}
	}
	else
		to->pages = NULL;
}

static void update_totalloc(bPagedBuffer *pbuf)
{
	bPagedBufferPage *page;
	int p, start;
	
	pbuf->totalloc = 0;
	for (p=0, start=0, page=pbuf->pages; p < pbuf->totpages; ++p, ++page, start+=pbuf->page_size) {
		if (page->layers)
			pbuf->totalloc += MIN2(pbuf->page_size, pbuf->totelem - start);
	}
}

void BLI_pbuf_set_page_size(bPagedBuffer *pbuf, int new_page_size)
{
	int new_totpages;
	bPagedBufferPage *new_pages= NULL;
	bPagedBufferPage *page;
	bPagedBufferLayerInfo *layer;
	int p;
	
	if (pbuf->page_size == new_page_size)
		return;
	
	/* determine how many new pages are really needed */
	if (pbuf->totelem > 0) {
		int nstart, nend;
		
		new_totpages = 0;
		nend = -1;
		for (p=0; p < pbuf->totpages; ++p) {
			nstart= MAX2( p, nend+1 );
			nend= (int)(MAX2(MIN2((p+1)*pbuf->page_size, pbuf->totelem)-1, p*pbuf->page_size) / new_page_size);
			
			new_totpages += nend-nstart+1;
		}
	}
	else
		new_totpages = 0;
	
	if (new_totpages > 0) {
		bPagedBufferPage *npage;
		int cur, page_start, npage_start, page_end, npage_end, copy;
		
		new_pages = MEM_callocN(new_totpages * sizeof(bPagedBufferPage), "bPagedBufferPage array");
		
		page = pbuf->pages;
		cur = page_start = 0;
		page_end = MIN2(pbuf->page_size, pbuf->totelem);
		/* init the first npage */
		npage = new_pages;
		npage_start = 0;
		npage_end = new_page_size;
		if (page->layers) {
			init_page(pbuf, npage, new_page_size);
		}
		
		while (cur < pbuf->totelem) {
			if (cur >= page_end) {
				free_page(pbuf, page);
				
				++page;
				page_start += pbuf->page_size;
				page_end = MIN2(page_start+pbuf->page_size, pbuf->totelem);
				cur = page_start;
				
				if (page->layers && !npage->layers) {
					init_page(pbuf, npage, new_page_size);
				}
			}
			if (cur >= npage_end) {
				++npage;
				npage_start = (int)(cur / new_page_size) * new_page_size;
				npage_end = npage_start+new_page_size;
				
				if (page->layers) {
					init_page(pbuf, npage, new_page_size);
				}
			}
			
			/* copy data */
			copy = MIN2(page_end, npage_end) - cur;
			for (layer=pbuf->layers.first; layer; layer=layer->next) {
				memcpy((char*)npage->layers[layer->layer] + (cur-npage_start)*layer->stride,
					   (char*)page->layers[layer->layer] + (cur-page_start)*layer->stride,
					   copy*layer->stride);
			}
			
			cur += copy;
		}
		/* free the last page */
		if (page->layers) {
			free_page(pbuf, page);
		}
	}
	
	if (pbuf->pages)
		MEM_freeN(pbuf->pages);
	pbuf->pages = new_pages;
	pbuf->totpages = new_totpages;
	pbuf->page_size = new_page_size;
	update_totalloc(pbuf);
}

/************************************************/
/*					Properties					*/
/************************************************/

static void insert_page_layer(bPagedBuffer *pbuf, bPagedBufferLayerInfo *layer)
{
	bPagedBufferPage *page;
	int index = layer->layer;
	int p;
	
	for (p=0, page=pbuf->pages; p < pbuf->totpages; ++p, ++page) {
		void **old_layers= page->layers;
		/* assumes pbuf->totlayers is already incremented! */
		if (old_layers) {
			page->layers = MEM_callocN(pbuf->totlayers*sizeof(void*), "bPagedBufferPage layers");
			if (index > 0)
				memcpy(page->layers, old_layers,
					   index * sizeof(void*));
			if (index < pbuf->totlayers-1)
				memcpy(page->layers + index+1, old_layers + index,
					   (pbuf->totlayers-index-1) * sizeof(void*));
			
			page->layers[index] = make_page_layer(pbuf->page_size, layer);
			
			MEM_freeN(old_layers);
		}
	}
}

static void remove_page_layer(bPagedBuffer *pbuf, bPagedBufferLayerInfo *layer)
{
	bPagedBufferPage *page;
	int index = layer->layer;
	int p;
	
	for (p=0, page=pbuf->pages; p < pbuf->totpages; ++p, ++page) {
		void **old_layers= page->layers;
		if (old_layers) {
			/* assumes pbuf->totlayers is already decremented! */
			page->layers = MEM_callocN(pbuf->totlayers*sizeof(void*), "bPagedBufferPage layers");
			if (index > 0)
				memcpy(page->layers, old_layers,
					   index * sizeof(void*));
			if (index < pbuf->totlayers)
				memcpy(page->layers + index, old_layers + index+1,
					   (pbuf->totlayers-index) * sizeof(void*));
			
			MEM_freeN(old_layers[index]);
			
			MEM_freeN(old_layers);
		}
	}
}

#if 0	/* UNUSED */
static void remove_all_page_layers(bPagedBuffer *pbuf)
{
	bPagedBufferPage *page;
	int p, i;
	
	for (p=0, page=pbuf->pages; p < pbuf->totpages; ++p, ++page) {
		if (page->layers) {
			for (i=0; i < pbuf->totlayers; ++i)
				MEM_freeN(page->layers[i]);
			MEM_freeN(page->layers);
			page->layers = NULL;
		}
	}
}
#endif

bPagedBufferLayerInfo *BLI_pbuf_layer_add(bPagedBuffer *pbuf, const char *name, int stride, const void *default_value)
{
	bPagedBufferLayerInfo *layer = MEM_callocN(sizeof(bPagedBufferLayerInfo), "paged buffer layer info");
	
	BLI_strncpy(layer->name, name, sizeof(layer->name));
	layer->stride = stride;
	layer->default_value = MEM_mallocN(stride, "paged buffer layer default value");
	memcpy(layer->default_value, default_value, stride);
	
	BLI_addtail(&pbuf->layers, layer);
	update_layer_indices(pbuf);
	insert_page_layer(pbuf, layer);
	
	return layer;
}

void BLI_pbuf_layer_remove(bPagedBuffer *pbuf, bPagedBufferLayerInfo *layer)
{
	BLI_remlink(&pbuf->layers, layer);
	update_layer_indices(pbuf);
	remove_page_layer(pbuf, layer);
	
	free_layer_info(layer);
	MEM_freeN(layer);
}

/************************************************/
/*					Data Access					*/
/************************************************/

bPagedBufferIterator BLI_pbuf_set_elements(bPagedBuffer *pbuf, int totelem)
{
	bPagedBufferPage *page;
	bPagedBufferLayerInfo *layer;
	
	int new_totpages= (totelem+pbuf->page_size-1) / pbuf->page_size;	/* rounding up */
	
	/* need new pages array? */
	if (new_totpages != pbuf->totpages) {
		bPagedBufferPage *new_pages;
		int p;
		
		new_pages = MEM_callocN(new_totpages * sizeof(bPagedBufferPage), "bPagedBufferPage array");
		if (pbuf->pages)
			memcpy(new_pages, pbuf->pages, MIN2(new_totpages, pbuf->totpages) * sizeof(bPagedBufferPage));
		
		for (p=0, page=new_pages; p < new_totpages; ++p, ++page) {
			if (!page->layers) {
				init_page(pbuf, page, pbuf->page_size);
			}
		}
		/* free remaining pages (if any) */
		for (p=new_totpages, page=pbuf->pages+new_totpages; p < pbuf->totpages; ++p, ++page) {
			free_page(pbuf, page);
		}
		
		if (pbuf->pages)
			MEM_freeN(pbuf->pages);
		pbuf->pages = new_pages;
		pbuf->totpages = new_totpages;
	}
	
	/* clear overhang data (if any) */
	if (totelem < pbuf->totelem && pbuf->totpages > 0) {
		page = pbuf->pages + (pbuf->totpages-1);
		if (page->layers) {
			for (layer=pbuf->layers.first; layer; layer=layer->next)
				memset((char*)page->layers[layer->layer] + (totelem - (pbuf->totpages-1)*pbuf->page_size),
					   0,
					   (pbuf->totpages*pbuf->page_size - totelem)*layer->stride);
		}
	}
	
	pbuf->totelem = totelem;
	update_totalloc(pbuf);
	
	/* full buffer iterator */
	return BLI_pbuf_iter_init(pbuf);
}

bPagedBufferIterator BLI_pbuf_append_elements(bPagedBuffer *pbuf, int totappend)
{
	int old_totelem = pbuf->totelem;
	int new_totelem= pbuf->totelem + totappend;
	int new_totpages= (new_totelem+pbuf->page_size-1) / pbuf->page_size;	/* rounding up */
	
	/* need new pages array? */
	if (new_totpages > pbuf->totpages) {
		bPagedBufferPage *page, *new_pages;
		int p, startp = pbuf->totelem / pbuf->page_size;
		
		new_pages = MEM_callocN(new_totpages * sizeof(bPagedBufferPage), "bPagedBufferPage array");
		if (pbuf->pages)
			memcpy(new_pages, pbuf->pages, pbuf->totpages * sizeof(bPagedBufferPage));
		
		for (p=startp, page=new_pages+startp; p < new_totpages; ++p, ++page) {
			if (!page->layers) {
				init_page(pbuf, page, pbuf->page_size);
			}
		}
		
		if (pbuf->pages)
			MEM_freeN(pbuf->pages);
		pbuf->pages = new_pages;
		pbuf->totpages = new_totpages;
	}
	
	pbuf->totelem = new_totelem;
	update_totalloc(pbuf);
	
	/* iterator over new elements */
	return BLI_pbuf_iter_init_at(pbuf, old_totelem);
}

void BLI_pbuf_reset(bPagedBuffer *pbuf)
{
	if (pbuf->pages) {
		free_all_pages(pbuf);
		MEM_freeN(pbuf->pages);
		pbuf->pages = NULL;
	}
	pbuf->totpages = 0;
	pbuf->totelem = pbuf->totalloc = 0;
}

void BLI_pbuf_free_dead_pages(bPagedBuffer *pbuf, bPagedBufferTestFunc removetestfunc, void *userdata)
{
	bPagedBufferIterator pit;
	int p, active;
	
	pit.page_size = pbuf->page_size;
	pit.index_end = pbuf->totelem;
	pit.valid = 1;
	
	pit.index = 0;
	for (p=0, pit.page=pbuf->pages; p < pbuf->totpages; ++p, ++pit.page) {
		if (pit.page->layers) {
			active = MIN2(pbuf->page_size, pbuf->totelem - p*pbuf->page_size);
			for (pit.page_index=0; pit.page_index < pbuf->page_size && pit.index < pbuf->totelem; ++pit.page_index, ++pit.index)
				if (removetestfunc(&pit, userdata))
					--active;
			if (active == 0) {
				free_page(pbuf, pit.page);
			}
		}
	}
	
	update_totalloc(pbuf);
}

void BLI_pbuf_compress(struct bPagedBuffer *pbuf, bPagedBufferTestFunc removetestfunc, void *userdata, bPagedBufferLayerInfo *origindex_layer)
{
	bPagedBufferIterator pit;
	int p;
	int new_totelem, page_size=pbuf->page_size;
	bPagedBufferLayerInfo *layer;
	int origindex_index = origindex_layer->layer;
	int *origindex;
	bPagedBufferPage *page, *npage, *old_pages;
	int old_totpages;
	int i, ni, copy, page_len;
	
	pit.page_size = page_size;
	pit.index_end = pbuf->totelem;
	pit.valid = 1;
	
	new_totelem = 0;
	pit.index = 0;
	for (p=0, pit.page=pbuf->pages; p < pbuf->totpages; ++p, ++pit.page) {
		if (pit.page->layers) {
			origindex = pit.page->layers[origindex_index];
			for (pit.page_index=0; pit.page_index < pit.page_size && pit.index < pit.index_end; ++pit.page_index, ++pit.index, ++origindex) {
				if (removetestfunc(&pit, userdata))
					*origindex = -1;
				else {
					*origindex = pit.index;
					++new_totelem;
				}
			}
		}
		else {
			pit.index += pit.page_size;
		}
	}
	
	/* 
	 * 1. swap the old page array for the reduced one
	 * 2. copy the page data
	 */
	old_pages = pbuf->pages;
	old_totpages = pbuf->totpages;
	pbuf->totpages = (new_totelem + page_size-1) / page_size;
	if (pbuf->totpages > 0)
		pbuf->pages = MEM_callocN(pbuf->totpages*sizeof(bPagedBufferPage), "bPagedBufferPage array");
	else
		pbuf->pages = NULL;
	
	npage = pbuf->pages;
	ni = 0;
	if (npage) {
		init_page(pbuf, npage, page_size);
	}
	for (p=0, page=old_pages; p < old_totpages; ++p, ++page) {
		if (page->layers) {
			page_len = MIN2(page_size, pbuf->totelem - p*page_size);
			
			i=0;
			origindex = page->layers[origindex_index];
			while (i < page_len) {
				/* check if npage is full */
				if (ni >= page_size) {
					++npage;
					ni = 0;
					init_page(pbuf, npage, page_size);
				}
				
				/* skip deleted elements */
				while (i < page_len && *origindex < 0) {
					++i;
					++origindex;
				}

				/* count contiguous element range */
				copy = 0;
				while (i+copy < page_len && *(origindex+copy) >= 0 && ni+copy < page_size)
					++copy;
				if (copy > 0) {
					/* copy data */
					for (layer=pbuf->layers.first; layer; layer=layer->next)
						memcpy((char*)npage->layers[layer->layer] + ni*layer->stride,
							   (char*)page->layers[layer->layer] + i*layer->stride,
							   copy*layer->stride);
					i += copy;
					ni += copy;
					origindex += copy;
				}
			}
			
			/* free the old page */
			free_page(pbuf, page);
		}
	}
	
	if (old_pages)
		MEM_freeN(old_pages);
	
	pbuf->totelem = new_totelem;
	/* after compressing no dead pages exist any more, so totalloc==totelem */
	pbuf->totalloc = pbuf->totelem;
}

/**** Iterator ****/

bPagedBufferIterator BLI_pbuf_iter_init(bPagedBuffer *pbuf)
{
	bPagedBufferIterator it;
	it.page_size = pbuf->page_size;
	it.index_end = pbuf->totelem;

	if (pbuf->pages) {
		int p = 0;
		it.index = 0;
		it.page = pbuf->pages;
		it.page_index = 0;
		/* find the first valid page */
		while (it.index < it.index_end) {
			if (it.page->layers) {
				/* set to page start */
				it.valid = true;
				return it;
			}
			it.index += it.page_size;
			++p;
			++it.page;
		}
	}
	it.valid = false;
	return it;
}

bPagedBufferIterator BLI_pbuf_iter_init_at(bPagedBuffer *pbuf, int index)
{
	bPagedBufferIterator it;
	it.page_size = pbuf->page_size;
	it.index_end = pbuf->totelem;

	if (index < pbuf->totelem && pbuf->pages) {
		int p = index / it.page_size;
		it.index = index;
		it.page = pbuf->pages + p;
		it.page_index = index - p*it.page_size;
		if (it.page->layers) {
			it.valid = true;
			return it;
		}
	}
	it.valid = false;
	return it;
}

void BLI_pbuf_iter_next(bPagedBufferIterator *it)
{
	/* Note: no it->valid test here, this must be done before advancing! */
	++it->index;
	++it->page_index;
	if (it->index >= it->index_end) {
		it->valid = false;
		return;
	}
	if (it->page_index >= it->page_size) {
		++it->page;
		it->page_index = 0;
		/* skip dead pages */
		while (!it->page->layers) {
			++it->page;
			it->index += it->page_size;
			if (it->index >= it->index_end) {
				it->valid = false;
				return;
			}
		}
	}
}

void BLI_pbuf_iter_prev(bPagedBufferIterator *it)
{
	/* Note: no it->valid test here, this must be done before advancing! */
	--it->index;
	--it->page_index;
	if (it->index < 0) {
		it->valid = false;
		return;
	}
	if (it->page_index < 0) {
		--it->page;
		it->page_index = it->page_size-1;
		/* skip dead pages */
		while (!it->page->layers) {
			--it->page;
			it->index -= it->page_size;
			if (it->index < 0) {
				it->valid = false;
				return;
			}
		}
	}
}

/* Note about forward/backward operators:
 * Instead of div/mod with page_size to get the final position,
 * we only use add/sub here. This should be faster on average,
 * when increments are small and only few pages
 * are skipped (small delta).
 */

void BLI_pbuf_iter_forward(struct bPagedBufferIterator *it, int delta)
{
	/* NB: no validity testing here for speed! (index < index_end) */
	
	int page_size = it->page_size;
	while (delta >= page_size) {
		++it->page;
		delta -= page_size;
	}
	if (delta > page_size - it->page_index) {
		++it->page;
		it->page_index += delta - page_size;
	}
	else
		it->page_index += delta;
	it->index += delta;
}

void BLI_pbuf_iter_backward(struct bPagedBufferIterator *it, int delta)
{
	/* NB: no validity testing here for speed! (index >= 0) */
	
	int page_size = it->page_size;
	while (delta >= page_size) {
		--it->page;
		delta -= page_size;
	}
	if (delta > it->page_index) {
		--it->page;
		it->page_index += page_size - delta;
	}
	else
		it->page_index += delta;
	it->index -= delta;
}

void BLI_pbuf_iter_forward_to(struct bPagedBufferIterator *it, int index)
{
	/* NB: no validity testing here for speed! (index < index_end) */
	
	int page_size = it->page_size;
	int delta = index - it->index;	/* total index step */
	while (delta >= page_size) {
		++it->page;
		delta -= page_size;
	}
	if (delta > (page_size-1) - it->page_index) {
		++it->page;
		it->page_index += delta - page_size;
	}
	else
		it->page_index += delta;
	it->index = index;
}

void BLI_pbuf_iter_backward_to(struct bPagedBufferIterator *it, int index)
{
	/* NB: no validity testing here for speed! (index >= 0) */
	
	int page_size = it->page_size;
	int delta = it->index - index;	/* total index step */
	while (delta >= page_size) {
		--it->page;
		delta -= page_size;
	}
	if (delta > it->page_index) {
		--it->page;
		it->page_index -= delta - page_size;
	}
	else
		it->page_index -= delta;
	it->index = index;
}

void BLI_pbuf_iter_goto(struct bPagedBufferIterator *it, int index)
{
	if (index > it->index)
		BLI_pbuf_iter_forward_to(it, index);
	else if (index < it->index)
		BLI_pbuf_iter_backward_to(it, index);
}

/**** Data Access ****/

static void binary_search_page(bPagedBufferIterator *pit, bPagedBufferSearchFunction testfunc, void *userdata, int page_start, int start_index, int end_index)
{
	int left= start_index, right= end_index, mid;
	int result;
	while (left <= right) {
		pit->page_index = mid = (left + right) >> 1;
		pit->index = page_start + mid;
		result = testfunc(pit, userdata);
		if (result == 0) {
			pit->valid = true;
			return;
		}
		else if (result < 0) {
			right = mid-1;
		}
		else {	/* result > 0 */
			left = mid+1;
		}
	}
	/* element not found */
	pit->valid = false;
}

bPagedBufferIterator BLI_pbuf_binary_search_element(bPagedBuffer *pbuf, bPagedBufferSearchFunction testfunc, void *userdata, int start_index, int end_index)
{
	bPagedBufferIterator pit;
	int p, mid_p, start_p, end_p;
	int result;
	int page_size= pbuf->page_size;
	int search_start, search_end;
	
	/* optimized binary search for paged buffers:
	 * - first do a binary search on the page ranges by testing first and last elements on pages.
	 * - when the page containing the element is found, do a binary search inside that page.
	 */
	
	pit.page_size = pbuf->page_size;
	pit.index_end = pbuf->totelem;
	
	if (start_index >= end_index) {
		pit.valid = false;
		return pit;
	}
	
	start_p = (int)(start_index / page_size);
	end_p = (int)((end_index + page_size-1) / page_size);	/* rounding up */
	do {
		mid_p = (start_p + end_p) >> 1;
		
		/* find page less-or-equal mid_p that has data */
		for (p=mid_p, pit.page=pbuf->pages+mid_p; p >= start_p; --p, --pit.page)
			if (pit.page->layers)
				break;
		if (p >= start_p) {
			/* check the first element of page p */
			pit.page_index = 0;
			pit.index = p*page_size;
			if (pit.index < start_index) {
				pit.page_index += start_index - pit.index;
				pit.index = start_index;
			}
			search_start = pit.page_index+1;	/* page index search range for actual binary search */
			
			result = testfunc(&pit, userdata);
			if (result == 0) {
				pit.valid = true;
				return pit;
			}
			else if (result < 0) {
				/* continue checking pages left of p */
				end_p = p-1;
				continue;
			}
			/* result > 0, check the last element of page p */
			pit.page_index = page_size-1;
			pit.index = (p+1)*page_size-1;
			if (pit.index >= end_index) {
				pit.page_index -= pit.index - (end_index-1);
				pit.index = end_index-1;
			}
			search_end = pit.page_index-1;	/* page index search range for actual binary search */
			
			result = testfunc(&pit, userdata);
			if (result == 0) {
				pit.valid = true;
				return pit;
			}
			else if (result < 0) {
				/* element must be on page p (excluding first and last element, we already tested those) */
				binary_search_page(&pit, testfunc, userdata, p*page_size, search_start, search_end);
				return pit;
			}
		}
		/* still here? means element must be greater than mid_p's last */
		/* find page greather than mid_p that has data */
		for (p=mid_p+1, pit.page=pbuf->pages+mid_p+1; p < end_p; ++p, ++pit.page)
			if (pit.page->layers)
				break;
		if (p < end_p) {
			/* check the last element of page p */
			pit.page_index = page_size-1;
			pit.index = (p+1)*page_size-1;
			if (pit.index >= end_index) {
				pit.page_index -= pit.index - (end_index-1);
				pit.index = end_index-1;
			}
			search_end = pit.page_index-1;	/* page index search range for actual binary search */
			
			result = testfunc(&pit, userdata);
			if (result == 0) {
				pit.valid = true;
				return pit;
			}
			else if (result > 0) {
				/* continue checking pages right of p */
				start_p = p+1;
				continue;
			}
			/* result < 0, check the first element of page p */
			pit.page_index = 0;
			pit.index = p*page_size;
			if (pit.index < start_index) {
				pit.page_index += start_index - pit.index;
				pit.index = start_index;
			}
			search_start = pit.page_index+1;	/* page index search range for actual binary search */
			
			result = testfunc(&pit, userdata);
			if (result == 0) {
				pit.valid = true;
				return pit;
			}
			else if (result > 0) {
				/* element must be on page p (excluding first and last element, we already tested those) */
				binary_search_page(&pit, testfunc, userdata, p*page_size, search_start, search_end);
				return pit;
			}
		}
		/* if we reach this point, it means that the element is on a dead page */
		break;
	} while (start_p <= end_p);
	
	/* element is on a dead page or out of range */
	pit.valid = false;
	return pit;
}
#endif
