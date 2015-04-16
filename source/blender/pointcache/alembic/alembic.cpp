/*
 * Copyright 2015, Blender Foundation.
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

#include "PTC_api.h"

#include "ptc_types.h"

#include "abc_reader.h"
#include "abc_writer.h"
#include "abc_group.h"
#include "abc_mesh.h"

namespace PTC {

class AbcFactory : public Factory {
	const std::string &get_default_extension()
	{
		static std::string ext = "abc";
		return ext;
	}
	
	WriterArchive *open_writer_archive(Scene *scene, const std::string &name, ErrorHandler *error_handler)
	{
		return AbcWriterArchive::open(scene, name, error_handler);
	}
	
	ReaderArchive *open_reader_archive(Scene *scene, const std::string &name, ErrorHandler *error_handler)
	{
		return AbcReaderArchive::open(scene, name, error_handler);
	}
	
	Writer *create_writer_duplicache(const std::string &name, Group *group, DupliCache *dupcache, int datatypes, bool do_sim_debug)
	{
		return new AbcDupliCacheWriter(name, group, dupcache, datatypes, do_sim_debug);
	}
	
	Reader *create_reader_duplicache(const std::string &name, Group *group, DupliCache *dupcache,
	                                 bool read_strands_motion, bool read_strands_children, bool read_sim_debug)
	{
		return new AbcDupliCacheReader(name, group, dupcache, read_strands_motion, read_strands_children, read_sim_debug);
	}
	
	Reader *create_reader_duplicache_object(const std::string &name, Object *ob, DupliObjectData *data,
	                                        bool read_strands_motion, bool read_strands_children)
	{
		return new AbcDupliObjectReader(name, ob, data, read_strands_motion, read_strands_children);
	}
};

}

void PTC_alembic_init()
{
	static PTC::AbcFactory abc_factory;
	
	PTC::Factory::alembic = &abc_factory;
}
