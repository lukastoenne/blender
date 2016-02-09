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

#ifndef __BVM_BNODE_PARSER_H__
#define __BVM_BNODE_PARSER_H__

/** \file bvm_bnode_parser.h
 *  \ingroup bvm
 */

#include <vector>

#include "bvm_source_location.h"

#include "BVM_types.h"
#include "bvm_util_string.h"

struct bNodeTree;
struct bNode;
struct bNodeSocket;

namespace bvm {

namespace ast {
struct ASTContext;
struct FunctionDecl;
struct NodeGraph;
struct Node;
struct NodeInput;
struct NodeOutput;
}

struct ASTErrorHandler {
	virtual ~ASTErrorHandler() {}
	
	virtual void report(const string &msg, SourceLocation loc, BVMErrorLevel level) = 0;
};

struct bNodeTreeParser {
	typedef std::vector<ast::Node*> Nodes;
	
	bNodeTreeParser(ASTErrorHandler *error_handler);
	~bNodeTreeParser();
	
	ast::FunctionDecl *parse(ast::ASTContext &C, const bNodeTree *ntree);
	
private:
	ASTErrorHandler *m_error_handler;
};

struct NodeCompiler {
	typedef std::vector<ast::Node*> Nodes;
	
	NodeCompiler(ast::ASTContext &C, Nodes &Nodes, ASTErrorHandler *error_handler);
	~NodeCompiler();
	
	ast::Node *add_node(const string &type);
	
	ast::NodeInput *get_input(const string &name) const;
	ast::NodeOutput *get_output(const string &name) const;
	
	void link(ast::NodeOutput *from, ast::NodeOutput *to);
	
	void report(const string &msg, SourceLocation loc, BVMErrorLevel level);
	
private:
	ast::ASTContext &m_ctx;
	Nodes &m_nodes;
	ASTErrorHandler *m_error_handler;
};

} /* namespace bvm */

#endif /* __BVM_BNODE_PARSER_H__ */
