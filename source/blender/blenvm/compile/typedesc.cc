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

/** \file blender/blenvm/intern/typedesc.cc
 *  \ingroup bvm
 */

#include "typedesc.h"
#include "node_value.h"

namespace blenvm {

StructSpec::StructSpec()
{
}

StructSpec::StructSpec(const StructSpec &other) :
	m_fields(other.m_fields)
{
}

StructSpec::~StructSpec()
{
}

StructSpec& StructSpec::operator = (const StructSpec &other)
{
	m_fields = other.m_fields;
	return *this;
}

bool StructSpec::operator == (const StructSpec &other) const
{
	/* vector comparison: checks equality of size and of each element */
	return m_fields == other.m_fields;
}

int StructSpec::find_field(const string &name) const
{
	for (int i = 0; i < m_fields.size(); ++i) {
		if (m_fields[i].name == name)
			return i;
	}
	return -1;
}

void StructSpec::add_field(const string &name, const TypeSpec *typespec)
{
	m_fields.push_back(FieldSpec(name, typespec));
}

/* ------------------------------------------------------------------------- */

TypeSpec::TypeSpec(BVMType base_type, BVMBufferType buffer_type) :
    m_base_type(base_type),
    m_buffer_type(buffer_type),
    m_structure(NULL)
{
}

TypeSpec::TypeSpec(const TypeSpec &other)
{
	m_base_type = other.m_base_type;
	m_buffer_type = other.m_buffer_type;
	
	if (other.m_structure)
		m_structure = new StructSpec(*other.m_structure);
	else
		m_structure = NULL;
}

TypeSpec::~TypeSpec()
{
	if (m_structure)
		delete m_structure;
}

TypeSpec& TypeSpec::operator = (const TypeSpec &other)
{
	m_base_type = other.m_base_type;
	m_buffer_type = other.m_buffer_type;
	
	if (m_structure)
		delete m_structure;
	if (other.m_structure)
		m_structure = new StructSpec(*other.m_structure);
	else
		m_structure = NULL;
	return *this;
}

bool TypeSpec::operator == (const TypeSpec &other) const
{
	if (is_structure() && other.is_structure()) {
		return *m_structure == *other.m_structure;
	}
	else if (!is_structure() && !other.is_structure()) {
		return m_base_type == other.m_base_type &&
		       m_buffer_type == other.m_buffer_type;
	}
	else
		return false;
}


bool TypeSpec::operator < (const TypeSpec &other) const
{
	const StructSpec *sa = m_structure;
	const StructSpec *sb = other.m_structure;
	if (!sa && !sb) {
		/* neither type is a struct, compare base types */
		if (m_base_type == other.m_base_type) {
			return m_buffer_type < other.m_buffer_type;
		}
		else {
			return m_base_type < other.m_base_type;
		}
	}
	else if (sa && sb) {
		/* both are structs, make deep comparison */
		if (sa->num_fields() == sb->num_fields()) {
			int num_fields = sa->num_fields();
			for (int i = 0; i < num_fields; ++i) {
				const StructSpec::FieldSpec &fa = sa->field(i);
				const StructSpec::FieldSpec &fb = sb->field(i);
				if (fa.typespec < fb.typespec) {
					return true;
				}
				else if (fb.typespec < fa.typespec) {
					return false;
				}
				else {
					continue;
				}
			}
			/* if we get here: all fields are equivalent */
			return false;
		}
		else {
			return sa->num_fields() < sb->num_fields();
		}
	}
	else if (!sa && sb) {
		return true;
	}
	else { /* sa && !sb */
		return false;
	}
}

bool TypeSpec::assignable(const TypeSpec &other) const
{
	return *this == other;
}

size_t TypeSpec::size() const
{
	if (m_structure) {
		size_t size = 0;
		for (int i = 0; i < m_structure->num_fields(); ++i)
			size += m_structure->field(i).typespec->size();
		return size;
	}
	else {
		switch (m_buffer_type) {
			case BVM_BUFFER_SINGLE:
				switch (m_base_type) {
					case BVM_FLOAT: return BaseTypeTraits<BVM_FLOAT>::size;
					case BVM_FLOAT3: return BaseTypeTraits<BVM_FLOAT3>::size;
					case BVM_FLOAT4: return BaseTypeTraits<BVM_FLOAT4>::size;
					case BVM_INT: return BaseTypeTraits<BVM_INT>::size;
					case BVM_MATRIX44: return BaseTypeTraits<BVM_MATRIX44>::size;
					case BVM_STRING: return BaseTypeTraits<BVM_STRING>::size;
					case BVM_RNAPOINTER: return BaseTypeTraits<BVM_RNAPOINTER>::size;
					case BVM_MESH: return BaseTypeTraits<BVM_MESH>::size;
					case BVM_DUPLIS: return BaseTypeTraits<BVM_DUPLIS>::size;
				}
				break;
			case BVM_BUFFER_ARRAY:
				switch (m_base_type) {
					case BVM_FLOAT: return sizeof(array<BVM_FLOAT>);
					case BVM_FLOAT3: return sizeof(array<BVM_FLOAT>);
					case BVM_FLOAT4: return sizeof(array<BVM_FLOAT>);
					case BVM_INT: return sizeof(array<BVM_FLOAT>);
					case BVM_MATRIX44: return sizeof(array<BVM_FLOAT>);
					case BVM_STRING: return sizeof(array<BVM_FLOAT>);
					case BVM_RNAPOINTER: return sizeof(array<BVM_FLOAT>);
					case BVM_MESH: return sizeof(array<BVM_FLOAT>);
					case BVM_DUPLIS: return sizeof(array<BVM_FLOAT>);
				}
			case BVM_BUFFER_IMAGE:
				switch (m_base_type) {
					case BVM_FLOAT: return sizeof(image<BVM_FLOAT>);
					case BVM_FLOAT3: return sizeof(image<BVM_FLOAT>);
					case BVM_FLOAT4: return sizeof(image<BVM_FLOAT>);
					case BVM_INT: return sizeof(image<BVM_FLOAT>);
					case BVM_MATRIX44: return sizeof(image<BVM_FLOAT>);
					case BVM_STRING: return sizeof(image<BVM_FLOAT>);
					case BVM_RNAPOINTER: return sizeof(image<BVM_FLOAT>);
					case BVM_MESH: return sizeof(image<BVM_FLOAT>);
					case BVM_DUPLIS: return sizeof(image<BVM_FLOAT>);
				}
		}
	}
	
	return 0;
}

void TypeSpec::copy_value(void *to, const void *from) const
{
	if (m_structure) {
		for (int i = 0; i < m_structure->num_fields(); ++i) {
			m_structure->field(i).typespec->copy_value(to, from);
			
			size_t size = m_structure->field(i).typespec->size();
			to = ((uint8_t *)to) + size;
			from = ((uint8_t *)from) + size;
		}
	}
	else {
		switch (m_buffer_type) {
			case BVM_BUFFER_SINGLE:
#define COPY_TYPE(a, b, type) \
	BaseTypeTraits<type>::copy((BaseTypeTraits<type>::POD*)(a), (BaseTypeTraits<type>::POD const *)(b));
				switch (m_base_type) {
					case BVM_FLOAT: COPY_TYPE(to, from, BVM_FLOAT); break;
					case BVM_FLOAT3: COPY_TYPE(to, from, BVM_FLOAT3); break;
					case BVM_FLOAT4: COPY_TYPE(to, from, BVM_FLOAT4); break;
					case BVM_INT: COPY_TYPE(to, from, BVM_INT); break;
					case BVM_MATRIX44: COPY_TYPE(to, from, BVM_MATRIX44); break;
					case BVM_STRING: COPY_TYPE(to, from, BVM_STRING); break;
					case BVM_RNAPOINTER: COPY_TYPE(to, from, BVM_RNAPOINTER); break;
					case BVM_MESH: COPY_TYPE(to, from, BVM_MESH); break;
					case BVM_DUPLIS: COPY_TYPE(to, from, BVM_DUPLIS); break;
				}
#undef COPY_TYPE
				break;
			case BVM_BUFFER_ARRAY:
#define COPY_TYPE(a, b, type) \
	*(array<type> *)(a) = *(array<type> const *)(b);
				switch (m_base_type) {
					case BVM_FLOAT: COPY_TYPE(to, from, BVM_FLOAT); break;
					case BVM_FLOAT3: COPY_TYPE(to, from, BVM_FLOAT3); break;
					case BVM_FLOAT4: COPY_TYPE(to, from, BVM_FLOAT4); break;
					case BVM_INT: COPY_TYPE(to, from, BVM_INT); break;
					case BVM_MATRIX44: COPY_TYPE(to, from, BVM_MATRIX44); break;
					case BVM_STRING: COPY_TYPE(to, from, BVM_STRING); break;
					case BVM_RNAPOINTER: COPY_TYPE(to, from, BVM_RNAPOINTER); break;
					case BVM_MESH: COPY_TYPE(to, from, BVM_MESH); break;
					case BVM_DUPLIS: COPY_TYPE(to, from, BVM_DUPLIS); break;
				}
#undef COPY_TYPE
				break;
			case BVM_BUFFER_IMAGE:
#define COPY_TYPE(a, b, type) \
	*(image<type> *)(a) = *(image<type> const *)(b);
				switch (m_base_type) {
					case BVM_FLOAT: COPY_TYPE(to, from, BVM_FLOAT); break;
					case BVM_FLOAT3: COPY_TYPE(to, from, BVM_FLOAT3); break;
					case BVM_FLOAT4: COPY_TYPE(to, from, BVM_FLOAT4); break;
					case BVM_INT: COPY_TYPE(to, from, BVM_INT); break;
					case BVM_MATRIX44: COPY_TYPE(to, from, BVM_MATRIX44); break;
					case BVM_STRING: COPY_TYPE(to, from, BVM_STRING); break;
					case BVM_RNAPOINTER: COPY_TYPE(to, from, BVM_RNAPOINTER); break;
					case BVM_MESH: COPY_TYPE(to, from, BVM_MESH); break;
					case BVM_DUPLIS: COPY_TYPE(to, from, BVM_DUPLIS); break;
				}
#undef COPY_TYPE
				break;
		}
	}
}

StructSpec* TypeSpec::make_structure()
{
	assert(m_structure == NULL);
	m_structure = new StructSpec();
	return m_structure;
}


TypeSpec::TypeDefMap TypeSpec::m_typedefs;

const TypeSpec* TypeSpec::get_typedef(const string &name)
{
	TypeDefMap::const_iterator it = m_typedefs.find(name);
	if (it != m_typedefs.end())
		return it->second;
	else
		return NULL;
}

TypeSpec *TypeSpec::add_typedef(const string &name, BVMType base_type, BVMBufferType buffer_type)
{
	BLI_assert (m_typedefs.find(name) == m_typedefs.end());
	
	TypeSpec *ts = new TypeSpec(base_type, buffer_type);
	m_typedefs.insert(TypeDefMap::value_type(name, ts));
	return ts;
}

void TypeSpec::remove_typedef(const string &name)
{
	TypeDefMap::iterator it = m_typedefs.find(name);
	if (it != m_typedefs.end()) {
		delete it->second;
		m_typedefs.erase(it);
	}
}

void TypeSpec::clear_typedefs()
{
	for (TypeDefMap::iterator it = m_typedefs.begin(); it != m_typedefs.end(); ++it) {
		delete it->second;
	}
	m_typedefs.clear();
}

TypeSpec::typedef_iterator TypeSpec::typedef_begin()
{
	return m_typedefs.begin();
}

TypeSpec::typedef_iterator TypeSpec::typedef_end()
{
	return m_typedefs.end();
}

/* ------------------------------------------------------------------------- */

TypeDesc::TypeDesc(const string &name) :
    m_name(name)
{
}

TypeDesc::TypeDesc(const TypeDesc &other)
{
	m_name = other.m_name;
}

TypeDesc::~TypeDesc()
{
}

bool TypeDesc::has_typespec() const
{
	const TypeSpec *ts = TypeSpec::get_typedef(m_name);
	return ts != NULL;
}

const TypeSpec *TypeDesc::get_typespec() const
{
	const TypeSpec *ts = TypeSpec::get_typedef(m_name);
	BLI_assert(ts != NULL);
	return ts;
}

} /* namespace blenvm */
