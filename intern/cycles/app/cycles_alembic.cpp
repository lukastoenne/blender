/*
 * Copyright 2015 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>

#include <sstream>
#include <algorithm>
#include <iterator>

#include <Alembic/AbcCoreOgawa/ReadWrite.h>
#ifdef WITH_HDF5
#include <Alembic/AbcCoreHDF5/ReadWrite.h>
#endif
#include <Alembic/Abc/IArchive.h>
#include <Alembic/Abc/IObject.h>
#include <Alembic/Abc/ISampleSelector.h>
#include <Alembic/Abc/ICompoundProperty.h>
#include <Alembic/Abc/IScalarProperty.h>
#include <Alembic/Abc/IArrayProperty.h>
#include <Alembic/Abc/ArchiveInfo.h>

#include "camera.h"
#include "film.h"
#include "graph.h"
#include "integrator.h"
#include "light.h"
#include "mesh.h"
#include "nodes.h"
#include "object.h"
#include "shader.h"
#include "scene.h"

#include "subd_mesh.h"
#include "subd_patch.h"
#include "subd_split.h"

#include "util_debug.h"
#include "util_foreach.h"
#include "util_path.h"
#include "util_transform.h"
#include "util_xml.h"

#include "cycles_alembic.h"

CCL_NAMESPACE_BEGIN

using namespace Alembic;
using namespace Abc;

#define ABC_SAFE_CALL_BEGIN \
	try {

#define ABC_SAFE_CALL_END \
	} \
	catch (Alembic::Util::Exception e) { \
		printf("%s", e.what()); \
	}


/* File */

static const std::string g_sep(";");

static void visitProperties(std::stringstream &ss, ICompoundProperty, std::string &);

template <class PROP>
static void visitSimpleArrayProperty(std::stringstream &ss, PROP iProp, const std::string &iIndent)
{
	std::string ptype = "ArrayProperty ";
	size_t asize = 0;
	
	AbcA::ArraySamplePtr samp;
	index_t maxSamples = iProp.getNumSamples();
	for (index_t i = 0 ; i < maxSamples; ++i) {
		iProp.get(samp, ISampleSelector( i ));
		asize = samp->size();
	};
	
	std::string mdstring = "interpretation=";
	mdstring += iProp.getMetaData().get("interpretation");
	
	std::stringstream dtype;
	dtype << "datatype=";
	dtype << iProp.getDataType();
	
	std::stringstream asizestr;
	asizestr << ";arraysize=";
	asizestr << asize;
	
	mdstring += g_sep;
	
	mdstring += dtype.str();
	
	mdstring += asizestr.str();
	
	ss << iIndent << "  " << ptype << "name=" << iProp.getName()
	   << g_sep << mdstring << g_sep << "numsamps="
	   << iProp.getNumSamples() << std::endl;
}

template <class PROP>
static void visitSimpleScalarProperty(std::stringstream &ss, PROP iProp, const std::string &iIndent)
{
	std::string ptype = "ScalarProperty ";
	size_t asize = 0;
	
	const AbcA::DataType &dt = iProp.getDataType();
	const Alembic::Util ::uint8_t extent = dt.getExtent();
	Alembic::Util::Dimensions dims(extent);
	AbcA::ArraySamplePtr samp = AbcA::AllocateArraySample( dt, dims );
	index_t maxSamples = iProp.getNumSamples();
	for (index_t i = 0 ; i < maxSamples; ++i) {
		iProp.get(const_cast<void*>(samp->getData()), ISampleSelector( i ));
		asize = samp->size();
	};
	
	std::string mdstring = "interpretation=";
	mdstring += iProp.getMetaData().get("interpretation");
	
	std::stringstream dtype;
	dtype << "datatype=";
	dtype << dt;
	
	std::stringstream asizestr;
	asizestr << ";arraysize=";
	asizestr << asize;
	
	mdstring += g_sep;
	
	mdstring += dtype.str();
	
	mdstring += asizestr.str();
	
	ss << iIndent << "  " << ptype << "name=" << iProp.getName()
	   << g_sep << mdstring << g_sep << "numsamps="
	   << iProp.getNumSamples() << std::endl;
}

static void visitCompoundProperty(std::stringstream &ss, ICompoundProperty iProp, std::string &ioIndent)
{
	std::string oldIndent = ioIndent;
	ioIndent += "  ";
	
	std::string interp = "schema=";
	interp += iProp.getMetaData().get("schema");
	
	ss << ioIndent << "CompoundProperty " << "name=" << iProp.getName()
	   << g_sep << interp << std::endl;
	
	visitProperties(ss, iProp, ioIndent);
	
	ioIndent = oldIndent;
}

static void visitProperties(std::stringstream &ss, ICompoundProperty iParent, std::string &ioIndent )
{
	std::string oldIndent = ioIndent;
	for (size_t i = 0 ; i < iParent.getNumProperties() ; i++) {
		PropertyHeader header = iParent.getPropertyHeader(i);
		
		if (header.isCompound()) {
			visitCompoundProperty(ss, ICompoundProperty(iParent, header.getName()), ioIndent);
		}
		else if (header.isScalar()) {
			visitSimpleScalarProperty(ss, IScalarProperty(iParent, header.getName()), ioIndent);
		}
		else {
			assert(header.isArray());
			visitSimpleArrayProperty(ss, IArrayProperty(iParent, header.getName()), ioIndent);
		}
	}
	
	ioIndent = oldIndent;
}

static void visitObject(std::stringstream &ss, IObject iObj, std::string iIndent)
{
	// Object has a name, a full name, some meta data,
	// and then it has a compound property full of properties.
	std::string path = iObj.getFullName();
	
	if (iObj.isInstanceRoot()) {
		if (path != "/") {
			ss << "Object " << "name=" << path
			   << " [Instance " << iObj.instanceSourcePath() << "]"
			   << std::endl;
		}
	}
	else if (iObj.isInstanceDescendant()) {
		/* skip non-root instances to avoid repetition */
		return;
	}
	else {
		if (path != "/") {
			ss << "Object " << "name=" << path << std::endl;
		}
		
		// Get the properties.
		ICompoundProperty props = iObj.getProperties();
		visitProperties(ss, props, iIndent);
		
		// now the child objects
		for (size_t i = 0 ; i < iObj.getNumChildren() ; i++) {
			visitObject(ss, IObject(iObj, iObj.getChildHeader(i).getName()), iIndent);
		}
	}
}

static std::string abc_archive_info(IArchive &archive)
{
	std::stringstream ss;
	
	ss << "Alembic Archive Info for "
	   << Alembic::AbcCoreAbstract::GetLibraryVersion()
	   << std::endl;;
	
	std::string appName;
	std::string libraryVersionString;
	Alembic::Util::uint32_t libraryVersion;
	std::string whenWritten;
	std::string userDescription;
	GetArchiveInfo(archive,
	               appName,
	               libraryVersionString,
	               libraryVersion,
	               whenWritten,
	               userDescription);
	
	if (appName != "") {
		ss << "  file written by: " << appName << std::endl;
		ss << "  using Alembic : " << libraryVersionString << std::endl;
		ss << "  written on : " << whenWritten << std::endl;
		ss << "  user description : " << userDescription << std::endl;
		ss << std::endl;
	}
	else {
//		ss << argv[1] << std::endl;
		ss << "  (file doesn't have any ArchiveInfo)"
		   << std::endl;
		ss << std::endl;
	}
	
	visitObject(ss, archive.getTop(), "");
	
	return ss.str();
}

void abc_read_ogawa_file(Scene *scene, const char *filepath)
{
	IArchive archive;
	ABC_SAFE_CALL_BEGIN
	archive = IArchive(AbcCoreOgawa::ReadArchive(), filepath, Abc::ErrorHandler::kThrowPolicy);
	ABC_SAFE_CALL_END
	
	if (archive) {
		printf("%s", abc_archive_info(archive).c_str());
	}
}

void abc_read_hdf5_file(Scene *scene, const char *filepath)
{
#ifdef WITH_HDF5
	IArchive archive;
	ABC_SAFE_CALL_BEGIN
	archive = IArchive(AbcCoreHDF5::ReadArchive(), filepath, Abc::ErrorHandler::kThrowPolicy);
	ABC_SAFE_CALL_END
	
	if (archive) {
		printf(abc_archive_info(archive));
	}
#endif
}

CCL_NAMESPACE_END

