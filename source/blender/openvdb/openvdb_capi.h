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

#ifndef __OPENVDB_CAPI_H__
#define __OPENVDB_CAPI_H__

#ifdef __cplusplus
extern "C" {
#endif

struct bNode;
struct bNodeTree;
struct FLUID_3D;
struct WTURBULENCE;

int OpenVDB_getVersionHex(void);

void OpenVDB_getNodeSockets(const char *filename, struct bNodeTree *ntree, struct bNode *node);


/* This duplicates a few properties from SmokeDomainSettings,
 * but it's more convenient/readable to pass a struct than having a huge set of
 * parameters to a function
 */
typedef struct FluidDomainDescr {
	float obmat[4][4];
	float fluidmat[4][4];
	float fluidmathigh[4][4];
	int shift[3];
	float obj_shift_f[3];
	int fluid_fields;
	float active_color[3];
	int active_fields;
} FluidDomainDescr;

enum {
	OPENVDB_NO_ERROR      = 0,
	OPENVDB_ARITHM_ERROR  = 1,
	OPENVDB_ILLEGAL_ERROR = 2,
	OPENVDB_INDEX_ERROR   = 3,
	OPENVDB_IO_ERROR      = 4,
	OPENVDB_KEY_ERROR     = 5,
	OPENVDB_LOOKUP_ERROR  = 6,
	OPENVDB_IMPL_ERROR    = 7,
	OPENVDB_REF_ERROR     = 8,
	OPENVDB_TYPE_ERROR    = 9,
	OPENVDB_VALUE_ERROR   = 10,
	OPENVDB_UNKNOWN_ERROR = 11,
};

void OpenVDB_export_fluid(struct FLUID_3D *fluid, struct WTURBULENCE *wt, FluidDomainDescr descr, const char *filename, float *shadow);
int OpenVDB_import_fluid(struct FLUID_3D *fluid, struct WTURBULENCE *wt, FluidDomainDescr *descr, const char *filename, float *shadow);

#ifdef __cplusplus
}
#endif

#endif /* __OPENVDB_CAPI_H__ */
