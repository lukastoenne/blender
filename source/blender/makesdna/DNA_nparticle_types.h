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

#include "BLO_sys_types.h"	/* for uint64_t */

#include "DNA_ID.h"

#include "DNA_node_types.h"
#include "DNA_pagedbuffer_types.h"


/* Standard attribute types */
/* XXX Warning! when adding attributes here, make sure to update
 * the attribute functions in nparticles.c accordingly!
 * Also make sure the PAR_ATTR_TYPE_MAX below is > than this.
 */
enum NodeParAttributeStandard {
	PAR_ATTR_TYPE_UNDEFINED         = -1,
	
	PAR_ATTR_TYPE_CUSTOM            = 0,
	
	PAR_ATTR_TYPE_FLAG              = 10,
	PAR_ATTR_TYPE_ID                = 11,
	PAR_ATTR_TYPE_SOURCE_ID         = 12,
	PAR_ATTR_TYPE_RANDOM_SEED       = 13,
	PAR_ATTR_TYPE_REM_INDEX         = 14,
	
	PAR_ATTR_TYPE_BIRTH_TIME        = 20,
	PAR_ATTR_TYPE_POSITION          = 21,
	PAR_ATTR_TYPE_VELOCITY          = 22,
	PAR_ATTR_TYPE_FORCE             = 23,
	PAR_ATTR_TYPE_MASS              = 24,
	PAR_ATTR_TYPE_ROTATION          = 25,
	PAR_ATTR_TYPE_ANGULAR_VELOCITY  = 26,
	PAR_ATTR_TYPE_TORQUE            = 27,
	PAR_ATTR_TYPE_ANGULAR_MASS      = 28
};


/** Data attribute associated with particles.
 */
typedef struct NParticleAttribute {
	struct NParticleAttribute *next, *prev;
	struct NParticleAttribute *new_attribute;	/* temporary pointer after copy */
	
	short type;
	short datatype;
	short flag;
	short pad;
	char name[64];
	
	/** Buffer layer associated with this attribute. */
	struct bPagedBufferLayerInfo *layer, *layer_write;
} NParticleAttribute;

/* particle attribute flags */
enum NodeParAttributeFlag {
	PAR_ATTR_STATE			= 1	/* attribute is part of the system state */
};

/* particle attribute types */
enum NodeParAttributeDataType {
	PAR_ATTR_DATATYPE_INTERNAL		= 0,	/* for static attributes with special types */
	PAR_ATTR_DATATYPE_FLOAT			= 1,
	PAR_ATTR_DATATYPE_INT			= 2,
	PAR_ATTR_DATATYPE_VECTOR		= 3,
	PAR_ATTR_DATATYPE_POINT			= 4,
	PAR_ATTR_DATATYPE_NORMAL		= 5,
	PAR_ATTR_DATATYPE_COLOR			= 6,
	PAR_ATTR_DATATYPE_QUATERNION	= 7
	/*PAR_ATTR_DATATYPE_FLAG		= 8*/	/* XXX TODO */
};

/* ******** Various specialized typedefs for attributes ******** */
/* data type of the flag attribute layer */
typedef int NParticleFlagLayerType;

/* Random number generator implementation */
#define NPAR_RANDOM_LCG             1
#define NPAR_RANDOM_RNGSTREAMS      2

#define NPAR_RANDOM NPAR_RANDOM_LCG		/* used rng mode */

#if NPAR_RANDOM == NPAR_RANDOM_LCG
typedef struct NParticleRNG {
	uint64_t Cg;
} NParticleRNG;
#elif NPAR_RANDOM == NPAR_RANDOM_RNGSTREAMS
/* MRG32k3a state size */
typedef struct NParticleRNG {
	double Cg[6];
} NParticleRNG;
#endif

typedef struct NParticleVector {
	float x, y, z;
} NParticleVector;

typedef struct NParticleQuaternion {
	float w, x, y, z;
} NParticleQuaternion;

typedef struct NParticleColor {
	float r, g, b, a;
} NParticleColor;

typedef struct NParticleMeshLocation {
	unsigned int v1, v2, v3, v4;	/* vertex indices */
	float weight[4];				/* factor for simplex edges */
	unsigned int face;				/* optional, only for surface location */
	int pad;
} NParticleMeshLocation;

/* **************** */


typedef struct bNodeInstanceMap {
	/* XXX for now just a simple ListBase, might use a hash later on */
	ListBase lb;
} bNodeInstanceMap;

typedef struct ParticleBuffer {
	bPagedBuffer data;
	
	ListBase attributes;
} ParticleBuffer;


typedef struct NParticleSource {
	int id;
	int next_element_id;
	NParticleRNG rng;
	/** Emission counter carry.
	 * Uses a float value here so that small emission rates
	 * can add up smoothly over several timesteps.
	 */
	float emit_carry;
	int pad;
} NParticleSource;

#endif
