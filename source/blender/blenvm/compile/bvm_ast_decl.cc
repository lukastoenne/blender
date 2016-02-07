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

#include <assert.h>

#include "bvm_ast_context.h"
#include "bvm_ast_decl.h"

namespace bvm {
namespace ast {

DeclContext::DeclContext(DeclContext *parent) :
    m_parent(parent)
{
}

DeclContext::~DeclContext()
{
}


void* Decl::operator new (size_t size, const ASTContext &C)
{
	return C.allocate(size);
}

void* Decl::operator new (size_t size, const ASTContext &C, DeclContext *DC)
{
	(void)DC;
	return C.allocate(size);
}

Decl::Decl(DeclContext *DC, SourceLocation loc) :
    decl_ctx(DC),
    loc(loc)
{
}

TranslationUnitDecl *Decl::get_translation_unit_decl() const
{
	DeclContext *DC = decl_ctx;
	while (DC->get_parent())
		DC = DC->get_parent();
	
	assert(dynamic_cast<TranslationUnitDecl*>(DC) && "Decl is not contained in a translation unit");
	return static_cast<TranslationUnitDecl*>(DC);
}

ASTContext &Decl::get_ast_context() const
{
	return get_translation_unit_decl()->getASTContext();
}

/* ========================================================================= */

TranslationUnitDecl *TranslationUnitDecl::create(ASTContext &C)
{
	return new (C) TranslationUnitDecl(C);
}

TranslationUnitDecl::TranslationUnitDecl(ASTContext &C) :
    Decl(NULL, SourceLocation()),
    DeclContext(NULL),
    ctx(C)
{
}

ASTContext &TranslationUnitDecl::getASTContext() const
{
	return ctx;
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
