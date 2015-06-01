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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../node_shader_util.h"

#include "openvdb_capi.h"

#define MAX_GRIDS 32

static bNodeSocketTemplate sh_node_openvdb_in[] = {
    {SOCK_VECTOR, 1, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
    {-1, 0, ""}
};

static void node_shader_init_openvdb(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeShaderOpenVDB *vdb = MEM_callocN(sizeof(NodeShaderOpenVDB), "NodeShaderOpenVDB");
	node->storage = vdb;
}

static bNodeSocket *node_output_relink(bNode *node, bNodeSocket *oldsock, int oldindex)
{
	bNodeSocket *sock;

	/* first try to find matching socket name */
	for (sock = node->outputs.first; sock; sock = sock->next)
		if (STREQ(sock->name, oldsock->name))
			return sock;

	/* no matching name, simply link to same index */
	return BLI_findlink(&node->outputs, oldindex);
}

#ifdef WITH_OPENVDB
static void node_openvdb_get_sockets(Main *bmain, bNodeTree *ntree, bNode *node)
{
	NodeShaderOpenVDB *vdb = node->storage;
	char *grid_names[MAX_GRIDS], *grid_types[MAX_GRIDS];
	char *filename;
	int i, num_grids;

	if (!vdb) {
		return;
	}

	filename = &vdb->filename[0];

	if (BLI_path_is_rel(filename)) {
		BLI_path_abs(filename, bmain->name);
	}

	OpenVDB_get_grid_names_and_types(filename, grid_names, grid_types, &num_grids);

	for (i = 0; i < num_grids; i++) {
		int type;

		if (STREQ(grid_types[i], "float")) {
			type = SOCK_FLOAT;
		}
		else if (STREQ(grid_types[i], "vec3s")) {
			type = SOCK_VECTOR;
		}
		else {
			continue;
		}

		nodeAddStaticSocket(ntree, node, SOCK_OUT, type, PROP_NONE, NULL, grid_names[i]);
	}
}

void ntreeUpdateOpenVDBNode(Main *bmain, bNodeTree *ntree, bNode *node)
{
	bNodeSocket *newsock, *oldsock;
	ListBase oldsocklist;
	bNodeLink *link;
	int oldindex;

	oldsocklist = node->outputs;
	BLI_listbase_clear(&node->outputs);

	node_openvdb_get_sockets(bmain, ntree, node);

	/* move links to new socket */
	for (oldsock = oldsocklist.first, oldindex = 0; oldsock; oldsock = oldsock->next, ++oldindex) {
		newsock = node_output_relink(node, oldsock, oldindex);

		if (newsock) {
			for (link = ntree->links.first; link; link = link->next) {
				if (link->fromsock == oldsock)
					link->fromsock = newsock;
			}
		}
	}
}
#else
void ntreeUpdateOpenVDBNode(Main *bmain, bNodeTree *ntree, bNode *node)
{
	UNUSED_VARS(bmain, ntree, node);
}
#endif

void register_node_type_sh_openvdb(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_OPENVDB, "OpenVDB Volume", NODE_CLASS_INPUT, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
	node_type_socket_templates(&ntype, sh_node_openvdb_in, NULL);
	node_type_init(&ntype, node_shader_init_openvdb);
	node_type_storage(&ntype, "NodeShaderOpenVDB", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}
