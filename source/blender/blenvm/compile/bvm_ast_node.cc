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

/** \file bvm_ast_node.cc
 *  \ingroup bvm
 */

#include "bvm_ast_context.h"
#include "bvm_ast_node.h"

namespace bvm {
namespace ast {

Node::Node(SourceLocation loc) :
    Stmt(loc)
{
}


ExprNode *ExprNode::create(ASTContext &C, SourceLocation loc)
{
	return new (C) ExprNode(loc);
}

ExprNode::ExprNode(SourceLocation loc) :
    Node(loc)
{
}


FunctionNode *FunctionNode::create(ASTContext &C, SourceLocation loc)
{
	return new (C) FunctionNode(loc);
}

FunctionNode::FunctionNode(SourceLocation loc) :
    ExprNode(loc)
{
}

/* ========================================================================= */

NodeGraph *NodeGraph::create(ASTContext &C, DeclContext *DC, SourceLocation loc)
{
	return new (C, DC) NodeGraph(DC, loc);
}

NodeGraph::NodeGraph(DeclContext *DC, SourceLocation loc) :
    DeclContext(DC),
    Decl(NULL, loc),
    m_nodes(NULL),
    m_nodes_size(0)
{
}

void NodeGraph::set_nodes(ArrayRef<Node*> nodes)
{
	assert(!m_nodes && "Already has nodes!");
	
	m_nodes = new (get_ast_context()) Node*;
//	m_nodes = new (get_ast_context()) Node*[nodes.size()];
	std::copy(nodes.begin(), nodes.end(), m_nodes);
	m_nodes_size = nodes.size();
}

} /* namespace ast */
} /* namespace bvm */
