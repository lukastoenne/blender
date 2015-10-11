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

/** \file bvm_codegen.cc
 *  \ingroup bvm
 */

#include "bvm_codegen.h"
#include "bvm_expression.h"
#include "bvm_nodegraph.h"

namespace bvm {

Expression *codegen_expression(const NodeGraph &graph)
{
	Expression *expr = new Expression();
	
	for (NodeGraph::OutputList::const_iterator it = graph.outputs.begin();
	     it != graph.outputs.end();
	     ++it) {
		const NodeGraphOutput &output = *it;
		
		expr->add_return_value(TypeDesc(output.type), output.name);
	}
	
	for (NodeGraph::NodeInstanceMap::const_iterator it = graph.nodes.begin();
	     it != graph.nodes.end();
	     ++it) {
		const NodeInstance &node = it->second;
		
	}
	
	return expr;
}

} /* namespace bvm */
