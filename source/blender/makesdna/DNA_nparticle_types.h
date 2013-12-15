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

#ifndef DNA_NPARTICLE_TYPES_H
#define DNA_NPARTICLE_TYPES_H

/** \file DNA_nparticle_types.h
 *  \ingroup DNA
 */

#include "DNA_pagedbuffer_types.h"

/* Attribute descriptor */
typedef struct NParticleAttributeDescription {
	char name[64];
	int datatype;
	int pad;
} NParticleAttributeDescription;

/* particle attribute types */
enum NParticleAttributeDataType {
	PAR_ATTR_DATATYPE_INTERNAL		= 0,	/* for static attributes with special types */
	PAR_ATTR_DATATYPE_FLOAT			= 1,
	PAR_ATTR_DATATYPE_INT			= 2,
	PAR_ATTR_DATATYPE_BOOL			= 3,
	PAR_ATTR_DATATYPE_VECTOR		= 4,
	PAR_ATTR_DATATYPE_POINT			= 5,
	PAR_ATTR_DATATYPE_NORMAL		= 6,
	PAR_ATTR_DATATYPE_COLOR			= 7,
	PAR_ATTR_DATATYPE_MATRIX		= 8
};

typedef struct NParticleAttributeState {
	bPagedBuffer data;
} NParticleAttributeState;

typedef struct NParticleAttribute {
	struct NParticleAttribute *next, *prev;
	
	NParticleAttributeDescription desc;	/* attribute descriptor */
	NParticleAttributeState *state;
} NParticleAttribute;

typedef struct NParticleSystem {
	ListBase attributes;				/* definition of available attributes */
	struct NParticleAttribute *attribute_id;
} NParticleSystem;

#endif
