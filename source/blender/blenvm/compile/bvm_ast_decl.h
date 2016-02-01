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

#ifndef __BVM_AST_DECL_H__
#define __BVM_AST_DECL_H__

/** \file bvm_ast_decl.h
 *  \ingroup bvm
 */

#include <vector>

#include "bvm_source_location.h"
#include "bvm_util_string.h"

namespace bvm {
namespace ast {

struct ASTContext;
struct Stmt;

struct DeclContext {
};

struct Decl {
	DeclContext *decl_ctx;
	SourceLocation loc;
	
protected:
	void* operator new (size_t size, const ASTContext &C, DeclContext *DC);
	void operator delete(void *ptr, const ASTContext &C, DeclContext *DC);
	
	Decl(DeclContext *DC, SourceLocation loc);
};

struct VarDecl : public Decl {
	static VarDecl *create(ASTContext &C, DeclContext *DC, SourceLocation loc);
	
protected:
	VarDecl(DeclContext *DC, SourceLocation loc);
};

struct ParmVarDecl : public VarDecl {
	static ParmVarDecl *create(ASTContext &C, DeclContext *DC, SourceLocation loc);
	
protected:
	ParmVarDecl(DeclContext *DC, SourceLocation loc);
};

struct FunctionDecl : public Decl {
	typedef std::vector<ParmVarDecl> ParmDeclList;
	
	ParmDeclList parms;
	Stmt *body;
	
	static FunctionDecl *create(ASTContext &C, DeclContext *DC, SourceLocation loc);
	
protected:
	FunctionDecl(DeclContext *DC, SourceLocation loc);
};

} /* namespace ast */
} /* namespace bvm */

#endif /* __BVM_AST_DECL_H__ */
