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
#include <Alembic/AbcGeom/IPolyMesh.h>

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
using namespace AbcGeom;

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

static void visitObject(std::stringstream &ss, IObject iObj, std::string iIndent, AbcArchiveInfoLevel info_level)
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
		
		if (info_level >= ABC_INFO_PROPERTIES) {
			// Get the properties.
			ICompoundProperty props = iObj.getProperties();
			visitProperties(ss, props, iIndent);
		}
		
		// now the child objects
		for (size_t i = 0 ; i < iObj.getNumChildren() ; i++) {
			visitObject(ss, IObject(iObj, iObj.getChildHeader(i).getName()), iIndent, info_level);
		}
	}
}

static std::string abc_archive_info(IArchive &archive, AbcArchiveInfoLevel info_level)
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
	
	if (info_level >= ABC_INFO_OBJECTS)
		visitObject(ss, archive.getTop(), "", info_level);
	
	return ss.str();
}

/* ========================================================================= */

struct AbcReadState {
	Scene *scene;		/* scene pointer */
	float time;
	Transform tfm;		/* current transform state */
	bool smooth;		/* smooth normal state */
	int shader;			/* current shader */
	string base;		/* base path to current file*/
	float dicing_rate;	/* current dicing rate */
	Mesh::DisplacementMethod displacement_method;
};

static ISampleSelector get_sample_selector(const AbcReadState &state)
{
	return ISampleSelector(state.time, ISampleSelector::kFloorIndex);
}

static Mesh *add_mesh(Scene *scene, const Transform& tfm)
{
	/* create mesh */
	Mesh *mesh = new Mesh();
	scene->meshes.push_back(mesh);

	/* create object*/
	Object *object = new Object();
	object->mesh = mesh;
	object->tfm = tfm;
	scene->objects.push_back(object);

	return mesh;
}

static void read_mesh(const AbcReadState &state, IPolyMesh object)
{
	/* add mesh */
	Mesh *mesh = add_mesh(state.scene, state.tfm);
	mesh->used_shaders.push_back(state.shader);

	/* read state */
	int shader = state.shader;
	bool smooth = state.smooth;

	mesh->displacement_method = state.displacement_method;

	ISampleSelector ss = get_sample_selector(state);
	IPolyMeshSchema schema = object.getSchema();

	IPolyMeshSchema::Sample sample;
	schema.get(sample, ss);

	int totverts = sample.getPositions()->size();
	int totfaces = sample.getFaceCounts()->size();
	const V3f *P = sample.getPositions()->get();
	const int32_t *verts = sample.getFaceIndices()->get();
	const int32_t *nverts = sample.getFaceCounts()->get();

	/* create vertices */
	mesh->verts.reserve(totverts);
	for(int i = 0; i < totverts; i++) {
		mesh->verts.push_back(make_float3(P[i].x, P[i].y, P[i].z));
	}
	
	/* create triangles */
	int index_offset = 0;
	
	for(int i = 0; i < totfaces; i++) {
		int n = nverts[i];
		/* XXX TODO only supports tris and quads atm,
			 * need a proper tessellation algorithm in cycles.
			 */
		if (n > 4) {
			printf("%d-sided face found, only triangles and quads are supported currently", n);
			n = 4;
		}
		
		for(int j = 0; j < n-2; j++) {
			int v0 = verts[index_offset];
			int v1 = verts[index_offset + j + 1];
			int v2 = verts[index_offset + j + 2];
			
			assert(v0 < (int)totverts);
			assert(v1 < (int)totverts);
			assert(v2 < (int)totverts);
			
			mesh->add_triangle(v0, v1, v2, shader, smooth);
		}
		
		index_offset += n;
	}

	/* temporary for test compatibility */
	mesh->attributes.remove(ATTR_STD_VERTEX_NORMAL);
}

static void read_object(const AbcReadState &state, IObject object)
{
	for (int i = 0; i < object.getNumChildren(); ++i) {
		IObject child = object.getChild(i);
		const MetaData &metadata = child.getMetaData();
		
		if (IPolyMeshSchema::matches(metadata)) {
			read_mesh(state, IPolyMesh(child, kWrapExisting));
		}
		else {
			read_object(state, child);
		}
	}
}

static void read_archive(Scene *scene, IArchive archive, const char *filepath)
{
	AbcReadState state;
	
	state.scene = scene;
	state.time = 0.0f; // TODO
	state.tfm = transform_identity();
	state.shader = scene->default_surface;
	state.smooth = false;
	state.dicing_rate = 0.1f;
	state.base = path_dirname(filepath);
	
	read_object(state, archive.getTop());
	
	scene->params.bvh_type = SceneParams::BVH_STATIC;
}

void abc_read_ogawa_file(Scene *scene, const char *filepath, AbcArchiveInfoLevel info_level)
{
	IArchive archive;
	ABC_SAFE_CALL_BEGIN
	archive = IArchive(AbcCoreOgawa::ReadArchive(), filepath, ErrorHandler::kThrowPolicy);
	ABC_SAFE_CALL_END
	
	if (archive) {
		if (info_level >= ABC_INFO_BASIC)
			printf("%s", abc_archive_info(archive, info_level).c_str());
		
		read_archive(scene, archive, filepath);
	}
}

void abc_read_hdf5_file(Scene *scene, const char *filepath, AbcArchiveInfoLevel info_level)
{
#ifdef WITH_HDF5
	IArchive archive;
	ABC_SAFE_CALL_BEGIN
	archive = IArchive(AbcCoreHDF5::ReadArchive(), filepath, ErrorHandler::kThrowPolicy);
	ABC_SAFE_CALL_END
	
	if (archive) {
		if (info_level >= ABC_INFO_BASIC)
			printf("%s", abc_archive_info(archive, info_level).c_str());
		
		read_archive(scene, archive, filepath);
	}
#endif
}

CCL_NAMESPACE_END

