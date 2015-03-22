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

#ifndef __CYCLES_ALEMBIC_H__
#define __CYCLES_ALEMBIC_H__

CCL_NAMESPACE_BEGIN

class Scene;

enum AbcArchiveInfoLevel {
	ABC_INFO_NONE = 0,
	ABC_INFO_BASIC,
	ABC_INFO_OBJECTS,
	ABC_INFO_PROPERTIES,
};

void abc_read_ogawa_file(Scene *scene, const char *filepath, AbcArchiveInfoLevel info_level = ABC_INFO_NONE);
void abc_read_hdf5_file(Scene *scene, const char *filepath, AbcArchiveInfoLevel info_level = ABC_INFO_NONE);

CCL_NAMESPACE_END

#endif /* __CYCLES_XML_H__ */
