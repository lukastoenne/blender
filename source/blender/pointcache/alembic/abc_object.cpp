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

#include "abc_object.h"

extern "C" {
#include "BLI_math.h"

#include "DNA_object_types.h"

#include "BKE_object.h"
}

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

AbcObjectWriter::AbcObjectWriter(const std::string &name, Object *ob) :
    ObjectWriter(ob, name)
{
}

void AbcObjectWriter::init_abc()
{
	if (m_abc_object)
		return;
	
	m_abc_object = abc_archive()->add_id_object<OObject>((ID *)m_ob);
}

#if 0
void AbcObjectWriter::create_refs()
{
	if ((m_ob->transflag & OB_DUPLIGROUP) && m_ob->dup_group) {
		OObject abc_group = abc_archive()->get_id_object((ID *)m_ob->dup_group);
		if (abc_group)
			m_abc_object.addChildInstance(abc_group, "dup_group");
	}
}
#endif

void AbcObjectWriter::write_sample()
{
	if (!m_abc_object)
		return;
	
	// TODO mesh, modifiers, sims ...
}


AbcObjectReader::AbcObjectReader(const std::string &name, Object *ob) :
    ObjectReader(ob, name)
{
}

void AbcObjectReader::init_abc()
{
	if (m_abc_object)
		return;
	m_abc_object = abc_archive()->get_id_object((ID *)m_ob);
}

PTCReadSampleResult AbcObjectReader::read_sample(float frame)
{
	if (!m_abc_object)
		return PTC_READ_SAMPLE_INVALID;
	
	return PTC_READ_SAMPLE_EXACT;
}

} /* namespace PTC */
