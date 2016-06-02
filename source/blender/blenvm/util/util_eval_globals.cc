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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file util_eval_globals.cc
 *  \ingroup bvm
 */

#include <cassert>

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "DNA_ID.h"
#include "DNA_object_types.h"

#include "BKE_image.h"

#include "IMB_imbuf_types.h"
}

#include "util_eval_globals.h"

#include "util_hash.h"

namespace blenvm {

EvalGlobals::EvalGlobals()
{
	m_image_pool = BKE_image_pool_new();
}

EvalGlobals::~EvalGlobals()
{
	BKE_image_pool_free(m_image_pool);
}

int EvalGlobals::get_id_key(ID *id)
{
	int hash = BLI_ghashutil_strhash(id->name);
	if (id->lib) {
		hash = hash_combine(hash, BLI_ghashutil_strhash(id->lib->name));
	}
	return hash;
}

void EvalGlobals::add_object(int key, Object *ob)
{
	m_objects[key] = ob;
}

PointerRNA EvalGlobals::lookup_object(int key) const
{
	ObjectMap::const_iterator it = m_objects.find(key);
	if (it != m_objects.end()) {
		PointerRNA ptr;
		RNA_id_pointer_create((ID *)it->second, &ptr);
		return ptr;
	}
	else {
		return PointerRNA_NULL;
	}
}

void EvalGlobals::add_image(int key, Image *ima)
{
	m_images[key] = ima;
}

ImBuf *EvalGlobals::lookup_imbuf(int key, ImageUser *iuser) const
{
	ImageMap::const_iterator ima_it = m_images.find(key);
	Image *ima = (ima_it != m_images.end()) ? ima_it->second : NULL;
	if (!ima)
		return NULL;
	
	/* local changes to the original ImageUser */
//	if (!BKE_image_is_multilayer(ima))
//		iuser->multi_index = BKE_scene_multiview_view_id_get(this->m_rd, this->m_viewName);
	
	ImBuf *ibuf = BKE_image_pool_acquire_ibuf(ima, iuser, m_image_pool);
	if (!ibuf || (!ibuf->rect && !ibuf->rect_float)) {
		BKE_image_pool_release_ibuf(ima, ibuf, m_image_pool);
		return NULL;
	}
	
	return ibuf;
}

} /* namespace blenvm */
