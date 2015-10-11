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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenvm/intern/bvm_api.cc
 *  \ingroup bvm
 */

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"

#include "DNA_node_types.h"

#include "BKE_node.h"

#include "BVM_api.h"
}

#include "bvm_eval.h"
#include "bvm_expression.h"
#include "bvm_function.h"
#include "bvm_module.h"
#include "bvm_nodegraph.h"

BLI_INLINE bvm::Module *_MOD(struct BVMModule *mod)
{ return (bvm::Module *)mod; }

struct BVMModule *BVM_module_create(void)
{ return (struct BVMModule *)(new bvm::Module()); }

void BVM_module_free(BVMModule *mod)
{ delete _MOD(mod); }

struct BVMFunction *BVM_module_create_function(BVMModule *mod, const char *name)
{ return (struct BVMFunction *)_MOD(mod)->create_function(name); }

bool BVM_module_delete_function(BVMModule *mod, const char *name)
{ return _MOD(mod)->remove_function(name); }

/* ------------------------------------------------------------------------- */

BLI_INLINE bvm::Expression *_EXPR(struct BVMExpression *expr)
{ return (bvm::Expression *)expr; }

void BVM_expression_free(struct BVMExpression *expr)
{ delete _EXPR(expr); }

/* ------------------------------------------------------------------------- */

BLI_INLINE bvm::NodeGraph *_GRAPH(struct BVMNodeGraph *graph)
{ return (bvm::NodeGraph *)graph; }
BLI_INLINE bvm::NodeInstance *_NODE(struct BVMNodeInstance *node)
{ return (bvm::NodeInstance *)node; }

struct BVMNodeInstance *BVM_nodegraph_add_node(BVMNodeGraph *graph, const char *type, const char *name)
{ return (struct BVMNodeInstance *)_GRAPH(graph)->add_node(type, name); }

/* ------------------------------------------------------------------------- */

BLI_INLINE bvm::EvalContext *_CTX(struct BVMEvalContext *ctx)
{ return (bvm::EvalContext *)ctx; }

struct BVMEvalContext *BVM_context_create(void)
{ return (BVMEvalContext *)(new bvm::EvalContext()); }

void BVM_context_free(struct BVMEvalContext *ctx)
{ delete _CTX(ctx); }

void BVM_eval_forcefield(struct BVMEvalContext *ctx, struct BVMExpression *expr, float force[3], float impulse[3])
{
	void *results[] = { force, impulse };
	_CTX(ctx)->eval_expression(*_EXPR(expr), results);
}

/* ------------------------------------------------------------------------- */

static void gen_nodetree_nodegraph(bNodeTree *btree, bvm::NodeGraph &graph)
{
	for (bNode *bnode = (bNode*)btree->nodes.first; bnode; bnode = bnode->next) {
		BLI_assert(bnode->typeinfo != NULL);
		if (!nodeIsRegistered(bnode))
			continue;
		
		const char *type = bnode->typeinfo->idname;
		/*NodeInstance *node =*/ graph.add_node(type, bnode->name);
	}
	
	for (bNodeLink *blink = (bNodeLink *)btree->links.first; blink; blink = blink->next) {
		if (!(blink->flag & NODE_LINK_VALID))
			continue;
		
		graph.add_link(blink->fromnode->name, blink->fromsock->name,
		               blink->tonode->name, blink->tosock->name);
	}
}

struct BVMExpression *BVM_gen_nodetree_expression(bNodeTree *btree)
{
	using namespace bvm;
	
	NodeGraph graph;
	
	gen_nodetree_nodegraph(btree, graph);
	
	// XXX TODO call codegen function here to turn nodegraph into an expression
	Expression *expr = NULL;
	
	return (BVMExpression *)expr;
}

#undef _GRAPH

#undef _EXPR
