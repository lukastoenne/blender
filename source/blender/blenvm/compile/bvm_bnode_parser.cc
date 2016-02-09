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

/** \file bvm_bnode_parser.cc
 *  \ingroup bvm
 */

extern "C" {
#include "DNA_node_types.h"

#include "BKE_node.h"

#include "RNA_access.h"
}

#include "bvm_bnode_parser.h"

#include "bvm_ast_node.h"

#include "bvm_util_string.h"

namespace bvm {

using namespace ast;

bNodeTreeParser::bNodeTreeParser(ASTErrorHandler *error_handler) :
    m_error_handler(error_handler)
{
}

bNodeTreeParser::~bNodeTreeParser()
{
}

static void parse_py_nodes(const bNodeTree *ntree, NodeCompiler *compiler)
{
	PointerRNA ptr;
	RNA_id_pointer_create((ID *)ntree, &ptr);
	
	FunctionRNA *func = RNA_struct_find_function(ptr.type, "bvm_compile");
	if (!func)
		return;
	
	ParameterList list;
	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "compiler", &compiler);
	ntree->typeinfo->ext.call(NULL, &ptr, func, &list);
	
	RNA_parameter_list_free(&list);
}

FunctionDecl *bNodeTreeParser::parse(ASTContext &C, const bNodeTree *ntree)
{
	TranslationUnitDecl *TUD = TranslationUnitDecl::create(C);
	
	NodeGraph *graph = NodeGraph::create(C, TUD, SourceLocation());
	
	Nodes nodes;
	NodeCompiler compiler(C, nodes, m_error_handler);
	parse_py_nodes(ntree, &compiler);
	
	
	
	return graph;
}

/* ========================================================================= */

NodeCompiler::NodeCompiler(ast::ASTContext &C, Nodes &nodes, ASTErrorHandler *error_handler) :
    m_ctx(C),
    m_nodes(nodes),
    m_error_handler(error_handler)
{
}

NodeCompiler::~NodeCompiler()
{
}

ast::Node *NodeCompiler::add_node(const string &type)
{
	// TODO
	return NULL;
}

ast::NodeInput *NodeCompiler::get_input(const string &name) const
{
	// TODO
	return NULL;
}

ast::NodeOutput *NodeCompiler::get_output(const string &name) const
{
	// TODO
	return NULL;
}

void NodeCompiler::link(ast::NodeOutput *from, ast::NodeOutput *to)
{
}

void NodeCompiler::report(const string &msg, SourceLocation loc, BVMErrorLevel level)
{
	if (m_error_handler)
		m_error_handler->report(msg, loc, level);
}

} /* namespace bvm */
