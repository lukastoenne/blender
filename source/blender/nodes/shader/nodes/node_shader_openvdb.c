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

static bNodeSocketTemplate sh_node_openvdb_in[] = {
    {SOCK_VECTOR, 1, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
    {-1, 0, ""}
};

static void node_shader_init_openvdb(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeShaderOpenVDB *vdb = MEM_callocN(sizeof(NodeShaderOpenVDB), "NodeShaderOpenVDB");
	node->storage = vdb;
}

static void node_shader_free_openvdb(bNode *node)
{
	NodeShaderOpenVDB *vdb = node->storage;
	if (vdb) {
		BLI_freelistN(&vdb->grid_info);
		MEM_freeN(vdb);
	}
}

static void node_shader_copy_openvdb(bNodeTree *UNUSED(dest_ntree), bNode *dst_node, bNode *src_node)
{
	dst_node->storage = MEM_dupallocN(src_node->storage);
	if (dst_node->storage) {
		NodeShaderOpenVDB *src_vdb = src_node->storage;
		NodeShaderOpenVDB *dst_vdb = dst_node->storage;

		BLI_duplicatelist(&dst_vdb->grid_info, &src_vdb->grid_info);
	}
}

#ifdef WITH_OPENVDB
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

static void node_openvdb_get_info(void *userdata, const char *name, const char *value_type, bool is_color)
{
	NodeShaderOpenVDB *vdb = userdata;
	OpenVDBGridInfo *info = MEM_callocN(sizeof(OpenVDBGridInfo), "openvdb grid info");

	BLI_strncpy(info->name, name, sizeof(info->name));
	if (STREQ(value_type, "float"))
		info->type = OPENVDB_TYPE_FLOAT;
	else if (STREQ(value_type, "vec3s")) {
		if (is_color)
			info->type = OPENVDB_TYPE_COLOR;
		else
			info->type = OPENVDB_TYPE_VEC3;
	}
	else
		info->type = OPENVDB_TYPE_UNKNOWN;

	info->flag = 0;

	BLI_addtail(&vdb->grid_info, info);
}

static void node_openvdb_get_sockets(Main *bmain, bNodeTree *ntree, bNode *node)
{
	NodeShaderOpenVDB *vdb = node->storage;
	OpenVDBGridInfo *info;
	char *filename;

	if (!vdb) {
		return;
	}

	filename = &vdb->filename[0];

	if (BLI_path_is_rel(filename)) {
		BLI_path_abs(filename, bmain->name);
	}

	BLI_freelistN(&vdb->grid_info);
	OpenVDB_get_grid_info(filename, node_openvdb_get_info, vdb);

	for (info = vdb->grid_info.first; info; info = info->next) {
		switch (info->type) {
			case OPENVDB_TYPE_FLOAT:
				nodeAddStaticSocket(ntree, node, SOCK_OUT, SOCK_FLOAT, PROP_NONE, NULL, info->name);
				break;
			case OPENVDB_TYPE_VEC3:
				nodeAddStaticSocket(ntree, node, SOCK_OUT, SOCK_VECTOR, PROP_NONE, NULL, info->name);
				break;
			case OPENVDB_TYPE_COLOR:
				nodeAddStaticSocket(ntree, node, SOCK_OUT, SOCK_RGBA, PROP_NONE, NULL, info->name);
				break;
		}
	}
}

void ntreeUpdateOpenVDBNode(Main *bmain, bNodeTree *ntree, bNode *node)
{
	bNodeSocket *newsock, *oldsock, *oldsock_next;
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

	for (oldsock = oldsocklist.first; oldsock; oldsock = oldsock_next) {
		oldsock_next = oldsock->next;
		nodeRemoveSocket(ntree, node, oldsock);
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
	node_type_storage(&ntype, "NodeShaderOpenVDB", node_shader_free_openvdb, node_shader_copy_openvdb);

	nodeRegisterType(&ntype);
}
