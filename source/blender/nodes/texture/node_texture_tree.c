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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/node_texture_tree.c
 *  \ingroup nodes
 */


#include <string.h>

#include "DNA_texture_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_linestyle.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_paint.h"
#include "BKE_texture.h"

#include "node_common.h"
#include "node_exec.h"
#include "node_util.h"
#include "NOD_texture.h"

#include "RNA_access.h"

#include "RE_shader_ext.h"

bNodeTreeExec *ntreeTexBeginExecTree_internal(bNodeExecContext *context, bNodeTree *ntree, bNodeInstanceKey parent_key)
{
	bNodeTreeExec *exec;
	bNode *node;
	
	/* common base initialization */
	exec = ntree_exec_begin(context, ntree, parent_key);
	
	/* allocate the thread stack listbase array */
	exec->threadstack = MEM_callocN(BLENDER_MAX_THREADS * sizeof(ListBase), "thread stack array");
	
	for (node = exec->nodetree->nodes.first; node; node = node->next)
		node->need_exec = 1;
	
	return exec;
}

bNodeTreeExec *ntreeTexBeginExecTree(bNodeTree *ntree)
{
#if 0
	bNodeExecContext context;
	bNodeTreeExec *exec;
	
	/* XXX hack: prevent exec data from being generated twice.
	 * this should be handled by the renderer!
	 */
	if (ntree->execdata)
		return ntree->execdata;
	
	context.previews = ntree->previews;
	
	exec = ntreeTexBeginExecTree_internal(&context, ntree, NODE_INSTANCE_KEY_BASE);
	
	/* XXX this should not be necessary, but is still used for cmp/sha/tex nodes,
	 * which only store the ntree pointer. Should be fixed at some point!
	 */
	ntree->execdata = exec;
	
	return exec;
#else
	(void)ntree;
	return NULL;
#endif
}

#if 0
/* free texture delegates */
static void tex_free_delegates(bNodeTreeExec *exec)
{
	bNodeThreadStack *nts;
	bNodeStack *ns;
	int th, a;
	
	for (th = 0; th < BLENDER_MAX_THREADS; th++)
		for (nts = exec->threadstack[th].first; nts; nts = nts->next)
			for (ns = nts->stack, a = 0; a < exec->stacksize; a++, ns++)
				if (ns->data && !ns->is_copy)
					MEM_freeN(ns->data);
}
#endif

void ntreeTexEndExecTree_internal(bNodeTreeExec *exec)
{
#if 0
	bNodeThreadStack *nts;
	int a;
	
	if (exec->threadstack) {
		tex_free_delegates(exec);
		
		for (a = 0; a < BLENDER_MAX_THREADS; a++) {
			for (nts = exec->threadstack[a].first; nts; nts = nts->next)
				if (nts->stack) MEM_freeN(nts->stack);
			BLI_freelistN(&exec->threadstack[a]);
		}
		
		MEM_freeN(exec->threadstack);
		exec->threadstack = NULL;
	}
	
	ntree_exec_end(exec);
#else
	(void)exec;
#endif
}

void ntreeTexEndExecTree(bNodeTreeExec *exec)
{
#if 0
	if (exec) {
		/* exec may get freed, so assign ntree */
		bNodeTree *ntree = exec->nodetree;
		ntreeTexEndExecTree_internal(exec);
		
		/* XXX clear nodetree backpointer to exec data, same problem as noted in ntreeBeginExecTree */
		ntree->execdata = NULL;
	}
#else
	(void)exec;
#endif
}

#if 0
int ntreeTexExecTree(
        bNodeTree *nodes,
        TexResult *texres,
        float co[3],
        float dxt[3], float dyt[3],
        int osatex,
        const short thread,
        Tex *UNUSED(tex),
        short which_output,
        int cfra,
        int preview,
        ShadeInput *shi,
        MTex *mtex)
{
	TexCallData data;
	float *nor = texres->nor;
	int retval = TEX_INT;
	bNodeThreadStack *nts = NULL;
	bNodeTreeExec *exec = nodes->execdata;

	data.co = co;
	data.dxt = dxt;
	data.dyt = dyt;
	data.osatex = osatex;
	data.target = texres;
	data.do_preview = preview;
	data.do_manage = (shi) ? shi->do_manage : true;
	data.thread = thread;
	data.which_output = which_output;
	data.cfra = cfra;
	data.mtex = mtex;
	data.shi = shi;
	
	/* ensure execdata is only initialized once */
	if (!exec) {
		BLI_lock_thread(LOCK_NODES);
		if (!nodes->execdata)
			ntreeTexBeginExecTree(nodes);
		BLI_unlock_thread(LOCK_NODES);

		exec = nodes->execdata;
	}
	
	nts = ntreeGetThreadStack(exec, thread);
	ntreeExecThreadNodes(exec, nts, &data, thread);
	ntreeReleaseThreadStack(nts);

	if (texres->nor) retval |= TEX_NOR;
	retval |= TEX_RGB;
	/* confusing stuff; the texture output node sets this to NULL to indicate no normal socket was set
	 * however, the texture code checks this for other reasons (namely, a normal is required for material) */
	texres->nor = nor;

	return retval;
}
#endif
