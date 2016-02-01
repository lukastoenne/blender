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

/** \file bvm_ast_decl.cc
 *  \ingroup bvm
 */

#include "bvm_ast_context.h"
#include "bvm_ast_decl.h"

namespace bvm {
namespace ast {

void* Decl::operator new (size_t size, const ASTContext &C, DeclContext *DC)
{
	(void)DC;
	return C.allocate(size, "Decl");
}

void Decl::operator delete(void *ptr, const ASTContext &C, DeclContext *DC)
{
	(void)DC;
	C.deallocate(ptr);
}

Decl::Decl(DeclContext *DC, SourceLocation loc) :
    decl_ctx(DC),
    loc(loc)
{
}


VarDecl *VarDecl::create(ASTContext &C, DeclContext *DC, SourceLocation loc)
{
	return new (C, DC) VarDecl(DC, loc);
}

VarDecl::VarDecl(DeclContext *DC, SourceLocation loc) :
    Decl(DC, loc)
{
}


ParmVarDecl *ParmVarDecl::create(ASTContext &C, DeclContext *DC, SourceLocation loc)
{
	return new (C, DC) ParmVarDecl(DC, loc);
}

ParmVarDecl::ParmVarDecl(DeclContext *DC, SourceLocation loc) :
    VarDecl(DC, loc)
{
}


FunctionDecl *FunctionDecl::create(ASTContext &C, DeclContext *DC, SourceLocation loc)
{
	return new (C, DC) FunctionDecl(DC, loc);
}

FunctionDecl::FunctionDecl(DeclContext *DC, SourceLocation loc) :
    Decl(DC, loc)
{
}

} /* namespace ast */
} /* namespace bvm */
