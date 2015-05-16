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
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <openvdb/openvdb.h>

extern "C" {
#include "DNA_node_types.h"

#include "BKE_node.h"

#include "BLI_listbase.h"
}

#include "MEM_guardedalloc.h"

#include "openvdb_capi.h"
#include "openvdb_intern.h"
#include "openvdb_util.h"

using namespace openvdb;

int OpenVDB_getVersionHex()
{
    return OPENVDB_LIBRARY_VERSION;
}

void OpenVDB_getNodeSockets(const char *filename, bNodeTree *ntree, bNode *node)
{
	int ret = OPENVDB_NO_ERROR;
	initialize();

	try {
		io::File file(filename);
		file.open();

		GridPtrVecPtr grids = file.getGrids();

		for (size_t i = 0; i < grids->size(); ++i) {
			GridBase::ConstPtr grid = (*grids)[i];
			int type;

			if (grid->valueType() == "float") {
				type = SOCK_FLOAT;
			}
			else if (grid->valueType() == "vec3s") {
				type = SOCK_VECTOR;
			}
			else {
				continue;
			}

			const char *name = grid->getName().c_str();

			nodeAddStaticSocket(ntree, node, SOCK_OUT, type, PROP_NONE, NULL, name);
		}
	}
	catch (...) {
		catch_exception(ret);
	}
}

void OpenVDB_export_fluid(FLUID_3D *fluid, WTURBULENCE *wt, FluidDomainDescr descr, const char *filename, float *shadow)
{
	int ret = OPENVDB_NO_ERROR;

	try {
		internal::OpenVDB_export_fluid(fluid, wt, descr, filename, shadow);
	}
	catch (...) {
		catch_exception(ret);
	}
}

int OpenVDB_import_fluid(FLUID_3D *fluid, WTURBULENCE *wt, FluidDomainDescr *descr, const char *filename, float *shadow)
{
	int ret = OPENVDB_NO_ERROR;

	try {
		internal::OpenVDB_import_fluid(fluid, wt, descr, filename, shadow);
	}
	catch (...) {
		catch_exception(ret);
	}

	return ret;
}


void OpenVDB_update_fluid_transform(const char *filename, FluidDomainDescr descr)
{
	int ret = OPENVDB_NO_ERROR;

	try {
		internal::OpenVDB_update_fluid_transform(filename, descr);
	}
	catch (...) {
		catch_exception(ret);
	}
}
