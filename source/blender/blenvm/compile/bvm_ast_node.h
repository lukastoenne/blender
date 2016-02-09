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

#ifndef __BVM_AST_NODE_H__
#define __BVM_AST_NODE_H__

/** \file bvm_ast_node.h
 *  \ingroup bvm
 */

#include "bvm_ast_decl.h"
#include "bvm_ast_stmt.h"

#include "bvm_util_arrayref.h"

namespace bvm {
namespace ast {

struct ASTContext;

struct NodeInputValue : public Stmt {
	static NodeInputValue *create(ASTContext &C, SourceLocation loc);
	
protected:
	NodeInputValue(SourceLocation loc);
};

struct Node : public Stmt {
protected:
	Node(SourceLocation loc);
};

struct ExprNode : public Node {
	static ExprNode *create(ASTContext &C, SourceLocation loc);
	
protected:
	ExprNode(SourceLocation loc);
};

struct FunctionNode : public ExprNode {
	static FunctionNode *create(ASTContext &C, SourceLocation loc);
	
protected:
	FunctionNode(SourceLocation loc);
};

/* ========================================================================= */

struct NodeGraph : public DeclContext, public Decl {
	static NodeGraph *create(ASTContext &C, DeclContext *DC, SourceLocation loc);
	
	typedef Node ** node_iterator;
	typedef Node * const * node_const_iterator;
	
	unsigned nodes_size() const { return m_nodes_size; }
	node_const_iterator nodes_begin() const { return node_const_iterator(m_nodes); }
	node_const_iterator nodes_end() const { return node_const_iterator(m_nodes); }
	
	void set_nodes(ArrayRef<Node*> nodes);
	
protected:
	NodeGraph(DeclContext *DC, SourceLocation loc);
	
private:
	Node **m_nodes;
	int m_nodes_size;
};

} /* namespace ast */
} /* namespace bvm */

#endif /* __BVM_AST_NODE_H__ */
