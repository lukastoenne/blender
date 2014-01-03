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
	int flag;
} NParticleAttributeDescription;

typedef enum eParticleAttributeFlag {
	PAR_ATTR_REQUIRED				= 1,	/* always exists */
	PAR_ATTR_PROTECTED				= 2,	/* descriptor is immutable */
	PAR_ATTR_READONLY				= 4,	/* attribute data is read-only */
	PAR_ATTR_TEMPORARY				= 8		/* temporary runtime attribute (not stored in cache or blend files) */
} eParticleAttributeFlag;

/* particle attribute types */
typedef enum eParticleAttributeDataType {
	PAR_ATTR_DATATYPE_INTERNAL		= 0,	/* for static attributes with special types */
	PAR_ATTR_DATATYPE_FLOAT			= 1,
	PAR_ATTR_DATATYPE_INT			= 2,
	PAR_ATTR_DATATYPE_BOOL			= 3,
	PAR_ATTR_DATATYPE_VECTOR		= 4,
	PAR_ATTR_DATATYPE_POINT			= 5,
	PAR_ATTR_DATATYPE_NORMAL		= 6,
	PAR_ATTR_DATATYPE_COLOR			= 7,
	PAR_ATTR_DATATYPE_MATRIX		= 8,
	PAR_ATTR_DATATYPE_POINTER		= 9
} eParticleAttributeDataType;

typedef struct NParticleAttributeState {
	/* XXX next/prev only needed for storing in ListBase,
	 * can be removed when attribute states get stored
	 * in a hash table instead.
	 */
	struct NParticleAttributeState *next, *prev;
	
	int hashkey;
	int flag;
	
	NParticleAttributeDescription desc;	/* attribute descriptor */
	bPagedBuffer data;
} NParticleAttributeState;

typedef enum eParticleAttributeStateFlag {
	PAR_ATTR_STATE_TEST			= 1		/* generic temporary test flag */
} eParticleAttributeStateFlag;

typedef struct NParticleState {
	/* XXX just a list atm, uses linear search for lookup,
	 * could use a GHash instead for O(1) lookup.
	 */
	ListBase attributes;
	
	void *py_handle;
} NParticleState;

typedef struct NParticleAttribute {
	struct NParticleAttribute *next, *prev;
	
	NParticleAttributeDescription desc;	/* attribute descriptor */
} NParticleAttribute;

typedef struct NParticleSystem {
	ListBase attributes;				/* definition of available attributes */
	
	struct NParticleState *state;
} NParticleSystem;

typedef struct NParticleDisplay {
	struct NParticleDisplay *next, *prev;
	
	int type;
	int pad;
	char attribute[64];
	
	/* dupli settings */
	ListBase dupli_objects;
} NParticleDisplay;

typedef struct NParticleDisplayDupli {
	struct Object *ob;
} NParticleDisplayDupli;

typedef enum eParticleDisplayType {
	PAR_DISPLAY_PARTICLE		= 1,
	PAR_DISPLAY_DUPLI			= 2
} eParticleDisplayType;

#endif /* DNA_NPARTICLE_TYPES_H */
