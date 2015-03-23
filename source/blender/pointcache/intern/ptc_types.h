/*
 * Copyright 2013, Blender Foundation.
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
 */

#ifndef PTC_TYPES_H
#define PTC_TYPES_H

#include "reader.h"
#include "writer.h"

extern "C" {
#include "DNA_dynamicpaint_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_smoke_types.h"
}

namespace PTC {

class ClothWriter : public Writer {
public:
	ClothWriter(Scene *scene, Object *ob, ClothModifierData *clmd, WriterArchive *archive) :
	    Writer(scene, (ID *)ob, archive),
	    m_ob(ob),
	    m_clmd(clmd)
	{}
	
	~ClothWriter()
	{}
	
protected:
	Object *m_ob;
	ClothModifierData *m_clmd;
};

class ClothReader : public Reader {
public:
	ClothReader(Scene *scene, Object *ob, ClothModifierData *clmd, ReaderArchive *archive) :
	    Reader(scene, (ID *)ob, archive),
	    m_ob(ob),
	    m_clmd(clmd)
	{}
	
	~ClothReader()
	{}
	
protected:
	Object *m_ob;
	ClothModifierData *m_clmd;
};


class DynamicPaintWriter : public Writer {
public:
	DynamicPaintWriter(Scene *scene, Object *ob, DynamicPaintSurface *surface, WriterArchive *archive) :
	    Writer(scene, (ID *)ob, archive),
	    m_ob(ob),
	    m_surface(surface)
	{}
	
	~DynamicPaintWriter()
	{}
	
protected:
	Object *m_ob;
	DynamicPaintSurface *m_surface;
};

class DynamicPaintReader : public Reader {
public:
	DynamicPaintReader(Scene *scene, Object *ob, DynamicPaintSurface *surface, ReaderArchive *archive) :
	    Reader(scene, (ID *)ob, archive),
	    m_ob(ob),
	    m_surface(surface)
	{}
	
	~DynamicPaintReader()
	{}
	
protected:
	Object *m_ob;
	DynamicPaintSurface *m_surface;
};


class PointCacheWriter : public Writer {
public:
	PointCacheWriter(Scene *scene, Object *ob, PointCacheModifierData *pcmd, WriterArchive *archive) :
	    Writer(scene, (ID *)ob, archive),
	    m_ob(ob),
	    m_pcmd(pcmd)
	{}
	
	~PointCacheWriter()
	{}
	
protected:
	Object *m_ob;
	PointCacheModifierData *m_pcmd;
};

class PointCacheReader : public Reader {
public:
	PointCacheReader(Scene *scene, Object *ob, PointCacheModifierData *pcmd, ReaderArchive *archive) :
	    Reader(scene, (ID *)ob, archive),
	    m_ob(ob),
	    m_pcmd(pcmd),
	    m_result(0)
	{}
	
	~PointCacheReader()
	{}
	
	virtual DerivedMesh *acquire_result();
	virtual void discard_result();
	
protected:
	Object *m_ob;
	PointCacheModifierData *m_pcmd;
	
	DerivedMesh *m_result;
};


class ParticlesWriter : public Writer {
public:
	ParticlesWriter(Scene *scene, Object *ob, ParticleSystem *psys, WriterArchive *archive) :
	    Writer(scene, (ID *)ob, archive),
	    m_ob(ob),
	    m_psys(psys)
	{}
	
	~ParticlesWriter()
	{}
	
protected:
	Object *m_ob;
	ParticleSystem *m_psys;
};

class ParticlesReader : public Reader {
public:
	ParticlesReader(Scene *scene, Object *ob, ParticleSystem *psys, ReaderArchive *archive) :
	    Reader(scene, (ID *)ob, archive),
	    m_ob(ob),
	    m_psys(psys),
	    m_totpoint(0)
	{}
	
	~ParticlesReader()
	{}
	
	int totpoint() const { return m_totpoint; }
	
protected:
	Object *m_ob;
	ParticleSystem *m_psys;
	
	int m_totpoint;
};


class RigidBodyWriter : public Writer {
public:
	RigidBodyWriter(Scene *scene, RigidBodyWorld *rbw, WriterArchive *archive) :
	    Writer(scene, (ID *)scene, archive),
	    m_rbw(rbw)
	{}
	
	~RigidBodyWriter()
	{}
	
protected:
	RigidBodyWorld *m_rbw;
};

class RigidBodyReader : public Reader {
public:
	RigidBodyReader(Scene *scene, RigidBodyWorld *rbw, ReaderArchive *archive) :
	    Reader(scene, (ID *)scene, archive),
	    m_rbw(rbw)
	{}
	
	~RigidBodyReader()
	{}
	
protected:
	RigidBodyWorld *m_rbw;
};


class SmokeWriter : public Writer {
public:
	SmokeWriter(Scene *scene, Object *ob, SmokeDomainSettings *domain, WriterArchive *archive) :
	    Writer(scene, (ID *)ob, archive),
	    m_ob(ob),
	    m_domain(domain)
	{}
	
	~SmokeWriter()
	{}
	
protected:
	Object *m_ob;
	SmokeDomainSettings *m_domain;
};

class SmokeReader : public Reader {
public:
	SmokeReader(Scene *scene, Object *ob, SmokeDomainSettings *domain, ReaderArchive *archive) :
	    Reader(scene, (ID *)ob, archive),
	    m_ob(ob),
	    m_domain(domain)
	{}
	
	~SmokeReader()
	{}
	
protected:
	Object *m_ob;
	SmokeDomainSettings *m_domain;
};


class SoftBodyWriter : public Writer {
public:
	SoftBodyWriter(Scene *scene, Object *ob, SoftBody *softbody, WriterArchive *archive) :
	    Writer(scene, (ID *)ob, archive),
	    m_ob(ob),
	    m_softbody(softbody)
	{}
	
	~SoftBodyWriter()
	{}
	
protected:
	Object *m_ob;
	SoftBody *m_softbody;
};

class SoftBodyReader : public Reader {
public:
	SoftBodyReader(Scene *scene, Object *ob, SoftBody *softbody, ReaderArchive *archive) :
	    Reader(scene, (ID *)ob, archive),
	    m_ob(ob),
	    m_softbody(softbody)
	{}
	
	~SoftBodyReader()
	{}
	
protected:
	Object *m_ob;
	SoftBody *m_softbody;
};

} /* namespace PTC */

#endif  /* PTC_EXPORT_H */
