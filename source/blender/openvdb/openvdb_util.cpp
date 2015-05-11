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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <openvdb/Exceptions.h>

#include "openvdb_capi.h"
#include "openvdb_util.h"

void catch_exception(int &ret)
{
	try {
		throw;
	}
	catch (const openvdb::ArithmeticError &e) {
		std::cerr << e.what() << std::endl;
		ret = OPENVDB_ARITHM_ERROR;
	}
	catch (const openvdb::IllegalValueException &e) {
		std::cerr << e.what() << std::endl;
		ret = OPENVDB_ILLEGAL_ERROR;
	}
	catch (const openvdb::IndexError &e) {
		std::cerr << e.what() << std::endl;
		ret = OPENVDB_INDEX_ERROR;
	}
	catch (const openvdb::IoError &e) {
		std::cerr << e.what() << std::endl;
		ret = OPENVDB_IO_ERROR;
	}
	catch (const openvdb::KeyError &e) {
		std::cerr << e.what() << std::endl;
		ret = OPENVDB_KEY_ERROR;
	}
	catch (const openvdb::LookupError &e) {
		std::cerr << e.what() << std::endl;
		ret = OPENVDB_LOOKUP_ERROR;
	}
	catch (const openvdb::NotImplementedError &e) {
		std::cerr << e.what() << std::endl;
		ret = OPENVDB_IMPL_ERROR;
	}
	catch (const openvdb::ReferenceError &e) {
		std::cerr << e.what() << std::endl;
		ret = OPENVDB_REF_ERROR;
	}
	catch (const openvdb::TypeError &e) {
		std::cerr << e.what() << std::endl;
		ret = OPENVDB_TYPE_ERROR;
	}
	catch (const openvdb::ValueError &e) {
		std::cerr << e.what() << std::endl;
		ret = OPENVDB_VALUE_ERROR;
	}
	catch (...) {
		std::cerr << "Unknown error in OpenVDB library..." << std::endl;
		ret = OPENVDB_UNKNOWN_ERROR;
	}
}
