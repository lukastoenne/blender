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

#include <string.h> /* XXX needed for missing type declarations in BLI ... */

#include "util_path.h"

extern "C" {
#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_ID.h"

#include "BKE_appdir.h"
#include "BKE_global.h"
#include "BKE_main.h"
}

namespace PTC {

#if 0
BLI_INLINE bool path_is_rel(const std::string &path)
{
	return BLI_path_is_rel(path.c_str());
}

BLI_INLINE bool is_dir(const std::string &path)
{
	return BLI_is_dir(path.c_str());
}

BLI_INLINE bool path_is_dirpath(const std::string &path)
{
	/* last char is a slash? */
	return *(BLI_last_slash(path.c_str()) + 1) == '\0';
}

BLI_INLINE std::string path_join_dirfile(const std::string &dir, const std::string &file)
{
	char path[FILE_MAX];
	BLI_join_dirfile(path, sizeof(path), dir.c_str(), file.c_str());
	return std::string(path);
}

BLI_INLINE std::string path_abs(const std::string &path, const std::string &basepath)
{
	char npath[FILE_MAX];
	BLI_strncpy(npath, path.c_str(), sizeof(npath));
	BLI_path_abs(npath, basepath.c_str());
	return std::string(npath);
}

bool ptc_archive_path(CacheLibrary *cachelib, std::string &filepath, Library *lib)
{
	filepath = "";
	
	if (!cachelib)
		return false;
	
	std::string abspath;
	if (path_is_rel(cachelib->filepath)) {
		if (G.relbase_valid || lib) {
			std::string relbase = lib ? lib->filepath: G.main->name;
			abspath = path_abs(cachelib->filepath, relbase);
		}
		else
			return false;
	}
	else {
		abspath = cachelib->filepath;
	}
	
	if (path_is_dirpath(abspath) || is_dir(abspath)) {
		filepath = path_join_dirfile(abspath, cachelib->id.name+2);
	}
	else {
		filepath = abspath;
	}
	
	return true;
}
#endif

} /* namespace PTC */
