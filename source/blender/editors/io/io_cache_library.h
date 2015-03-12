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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/io/io_cache_library.h
 *  \ingroup editor/io
 */

#ifndef __IO_CACHE_LIBRARY_H__
#define __IO_CACHE_LIBRARY_H__

struct wmOperatorType;

void CACHELIBRARY_OT_new(struct wmOperatorType *ot);
void CACHELIBRARY_OT_delete(struct wmOperatorType *ot);

void CACHELIBRARY_OT_item_enable(struct wmOperatorType *ot);

void CACHELIBRARY_OT_bake(struct wmOperatorType *ot);
void CACHELIBRARY_OT_rebuild_dupligroup(struct wmOperatorType *ot);

void CACHELIBRARY_OT_archive_info(struct wmOperatorType *ot);

#endif
