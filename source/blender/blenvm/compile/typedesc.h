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

/** \file blender/blenvm/intern/typedesc.h
 *  \ingroup bvm
 */

#ifndef __BVM_TYPEDESC_H__
#define __BVM_TYPEDESC_H__

#include <cassert>
#include <map>
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
struct NodeConstant;

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

struct TypeSpec {
	typedef std::map<string, const TypeSpec *> TypeDefMap;
	typedef TypeDefMap::const_iterator typedef_iterator;
	
	TypeSpec(BVMType base_type, BVMBufferType buffer_type = BVM_BUFFER_SINGLE);
	TypeSpec(const TypeSpec &other);
	~TypeSpec();
	
	TypeSpec& operator = (const TypeSpec &other);
	bool operator == (const TypeSpec &other) const;
	
	bool operator < (const TypeSpec &other) const;
	
	BVMType base_type() const { return m_base_type; }
	BVMBufferType buffer_type() const { return m_buffer_type; }
	bool is_aggregate() const;
	
	bool assignable(const TypeSpec &other) const;
	
	size_t size() const;
	void copy_value(void *to, const void *from) const;
	
	bool is_structure() const { return m_structure != NULL; }
	const StructSpec *structure() const { return m_structure; }
	StructSpec *structure() { return m_structure; }
	StructSpec *make_structure();
	
	static const TypeSpec* get_typespec(const string &name);
	static TypeSpec *add_typespec(const string &name, BVMType base_type, BVMBufferType buffer_type = BVM_BUFFER_SINGLE);
	static void remove_typespec(const string &name);
	static void clear_typespecs();
	static typedef_iterator typespec_begin();
	static typedef_iterator typespec_end();
	
private:
	BVMType m_base_type;
	BVMBufferType m_buffer_type;
	StructSpec *m_structure;
	
	static TypeDefMap m_typedefs;
};

struct StructSpec {
	struct FieldSpec {
		FieldSpec(const string &name, const TypeSpec *typespec) :
		    name(name),
		    typespec(typespec)
		{}
		
		bool operator == (const FieldSpec &other) const
		{
			return name == other.name && typespec == other.typespec;
		}
		
		string name;
		const TypeSpec *typespec;
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
	void add_field(const string &name, const TypeSpec *typespec);
	
private:
	FieldList m_fields;
};

/* ------------------------------------------------------------------------- */

struct TypeDesc {
	TypeDesc(const string &name);
	TypeDesc(const TypeDesc &other);
	~TypeDesc();
	
	const string &name() const { return m_name; }
	
	bool has_typespec() const;
	const TypeSpec *get_typespec() const;
	
private:
	string m_name;
};

} /* namespace blenvm */

#endif
