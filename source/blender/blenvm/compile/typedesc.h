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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenvm/intern/bvm_typedesc.h
 *  \ingroup bvm
 */

#ifndef __BVM_TYPEDESC_H__
#define __BVM_TYPEDESC_H__

#include <cassert>
#include <vector>

extern "C" {
#include "RNA_access.h"
}

#include "BVM_types.h"
#include "util_data_ptr.h"
#include "util_math.h"
#include "util_string.h"

namespace blenvm {

struct NodeGraph;
struct NodeValue;

template <BVMType type>
struct BaseTypeTraits;

template <>
struct BaseTypeTraits<BVM_FLOAT> {
	typedef float POD;
	
	enum eStackSize { size = sizeof(POD) };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_FLOAT3> {
	typedef float3 POD;
	
	enum eStackSize { size = sizeof(POD) };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_FLOAT4> {
	typedef float4 POD;
	
	enum eStackSize { size = sizeof(POD) };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_INT> {
	typedef int POD;
	
	enum eStackSize { size = sizeof(POD) };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_MATRIX44> {
	typedef matrix44 POD;
	
	enum eStackSize { size = sizeof(POD) };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_STRING> {
	typedef const char* POD;
	
	enum eStackSize { size = sizeof(POD) };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_RNAPOINTER> {
	typedef PointerRNA POD;
	
	enum eStackSize { size = sizeof(POD) };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_MESH> {
	typedef mesh_ptr POD;
	
	enum eStackSize { size = sizeof(POD) };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_DUPLIS> {
	typedef duplis_ptr POD;
	
	enum eStackSize { size = sizeof(POD) };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

/* ------------------------------------------------------------------------- */

struct StructSpec;

struct TypeDesc {
	TypeDesc(BVMType base_type, BVMBufferType buffer_type = BVM_BUFFER_SINGLE);
	TypeDesc(const TypeDesc &other);
	~TypeDesc();
	
	TypeDesc& operator = (const TypeDesc &other);
	bool operator == (const TypeDesc &other) const;
	
	BVMType base_type() const { return m_base_type; }
	BVMBufferType buffer_type() const { return m_buffer_type; }
	
	bool assignable(const TypeDesc &other) const;
	
	size_t size() const;
	void copy_value(void *to, const void *from) const;
	
	bool is_structure() const { return m_structure != NULL; }
	const StructSpec *structure() const { return m_structure; }
	StructSpec *structure() { return m_structure; }
	StructSpec *make_structure();
	
private:
	BVMType m_base_type;
	BVMBufferType m_buffer_type;
	StructSpec *m_structure;
};

struct StructSpec {
	struct FieldSpec {
		FieldSpec(const string &name, const TypeDesc &typedesc) :
		    name(name),
		    typedesc(typedesc)
		{}
		
		bool operator == (const FieldSpec &other) const
		{
			return name == other.name && typedesc == other.typedesc;
		}
		
		string name;
		TypeDesc typedesc;
	};
	typedef std::vector<FieldSpec> FieldList;
	
	StructSpec();
	StructSpec(const StructSpec &other);
	~StructSpec();
	
	StructSpec& operator = (const StructSpec &other);
	bool operator == (const StructSpec &other) const;
	
	int num_fields() const { return m_fields.size(); }
	const FieldSpec &field(int i) const { return m_fields[i]; }
	int find_field(const string &name) const;
	void add_field(const string &name, const TypeDesc &typedesc);
	
private:
	FieldList m_fields;
};

} /* namespace blenvm */

#endif
