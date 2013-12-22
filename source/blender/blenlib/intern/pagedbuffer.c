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
	if (pbuf->pages) {
		while (iter->page->data == NULL && iter->index < pbuf->totelem) {
			iter->index += pbuf->page_size;
			++iter->page;
		}
		iter->data = iter->page->data;
	}
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

void BLI_pbuf_iter_at(bPagedBuffer *pbuf, bPagedBufferIterator *iter, int index)
{
	div_t page_div = div(index, pbuf->page_size);
	
	iter->index = index;
	iter->page = pbuf->pages + page_div.quot;
	iter->page_index = page_div.rem;
	if (iter->page) {
		iter->data = (char *)iter->page->data + pbuf->elem_bytes * iter->page_index;
	}
}
