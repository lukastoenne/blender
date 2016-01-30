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

/** \file blender/blenvm/intern/bvm_typedesc.cc
 *  \ingroup bvm
 */

#include "bvm_typedesc.h"

namespace bvm {

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

void StructSpec::add_field(const string &name, const TypeDesc &typedesc)
{
	m_fields.push_back(FieldSpec(name, typedesc));
}

/* ------------------------------------------------------------------------- */

TypeDesc::TypeDesc(BVMType base_type, BVMBufferType buffer_type) :
    m_base_type(base_type),
    m_buffer_type(buffer_type),
    m_structure(NULL)
{
}

TypeDesc::TypeDesc(const TypeDesc &other)
{
	m_base_type = other.m_base_type;
	m_buffer_type = other.m_buffer_type;
	
	if (other.m_structure)
		m_structure = new StructSpec(*other.m_structure);
	else
		m_structure = NULL;
}

TypeDesc::~TypeDesc()
{
	if (m_structure)
		delete m_structure;
}

TypeDesc& TypeDesc::operator = (const TypeDesc &other)
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

bool TypeDesc::operator == (const TypeDesc &other) const
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

bool TypeDesc::assignable(const TypeDesc &other) const
{
	return *this == other;
}

size_t TypeDesc::size() const
{
	if (m_structure) {
		size_t size = 0;
		for (int i = 0; i < m_structure->num_fields(); ++i)
			size += m_structure->field(i).typedesc.size();
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

void TypeDesc::copy_value(void *to, const void *from) const
{
	if (m_structure) {
		for (int i = 0; i < m_structure->num_fields(); ++i) {
			m_structure->field(i).typedesc.copy_value(to, from);
			
			size_t size = m_structure->field(i).typedesc.size();
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

StructSpec* TypeDesc::make_structure()
{
	assert(m_structure == NULL);
	m_structure = new StructSpec();
	return m_structure;
}

} /* namespace bvm */
