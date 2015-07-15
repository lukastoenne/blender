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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenjit/intern/forcefield.cpp
 *  \ingroup bjit
 */

#include <cstdarg>

#include "llvm/Analysis/Passes.h"
#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/PassManager.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"

#include "llvm-c/ExecutionEngine.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"

#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "BKE_effect.h"
}

#include "BJIT_forcefield.h"
#include "bjit_intern.h"
#include "bjit_nodegraph.h"

using namespace llvm;

static Module *theModule = NULL;

namespace bjit {

typedef typename SocketTypeImpl<BJIT_TYPE_VEC3>::extern_type vec3_extern_t;
typedef typename SocketTypeImpl<BJIT_TYPE_VEC3>::extern_type_arg vec3_extern_arg_t;

struct EffectorEvalReturn {
	vec3_extern_t force;
	vec3_extern_t impulse;
};

typedef EffectorEvalReturn (*EffectorEvalFunction)(vec3_extern_arg_t loc, vec3_extern_arg_t vel);

static inline const char *get_effector_prefix(short forcefield)
{
	switch (forcefield) {
		case PFIELD_FORCE:      return "force";
		case PFIELD_WIND:       /*return "wind";*/
			
		case PFIELD_NULL:
		case PFIELD_VORTEX:
		case PFIELD_MAGNET:
		case PFIELD_GUIDE:
		case PFIELD_TEXTURE:
		case PFIELD_HARMONIC:
		case PFIELD_CHARGE:
		case PFIELD_LENNARDJ:
		case PFIELD_BOID:
		case PFIELD_TURBULENCE:
		case PFIELD_DRAG:
		case PFIELD_SMOKEFLOW:
			return "";
	}
	/* unknown type, should not happen */
	BLI_assert(false);
	return NULL;
}

static inline std::string get_effector_nodetype(short forcefield)
{
	const char *prefix = get_effector_prefix(forcefield);
	if (prefix && prefix[0] != '\0')
		return "effector_" + std::string(prefix) + "_eval";
	else
		return "";
}

template <>
struct NodeGraphBuilder<EffectorContext> {
	NodeGraph build(EffectorContext *effctx)
	{
		static const float default_force[3] = {0.0f, 0.0f, 0.0f};
		static const float default_impulse[3] = {0.0f, 0.0f, 0.0f};
		
		LLVMContext &context = getGlobalContext();
		NodeGraph graph;
		NodeInstance *node_prev = NULL;
		const NodeSocket *force_prev = NULL;
		const NodeSocket *impulse_prev = NULL;
		
		graph.add_input("loc", BJIT_TYPE_VEC3);
		graph.add_input("vel", BJIT_TYPE_VEC3);
		graph.add_output("force", BJIT_TYPE_VEC3, default_force, context);
		graph.add_output("impulse", BJIT_TYPE_VEC3, default_impulse, context);
		
		for (EffectorCache *eff = (EffectorCache *)effctx->effectors.first; eff; eff = eff->next) {
			if (!eff->ob || !eff->pd)
				continue;
			
			std::string nodetype = get_effector_nodetype(eff->pd->forcefield);
			std::string nodename = eff->ob->id.name;
			NodeInstance *node = graph.add_node(nodetype, nodename);
			if (!node) {
				continue;
			}
			
			node->set_input_extern("loc", graph.get_input(0));
			node->set_input_extern("vel", graph.get_input(1));
			node->set_input_value("transform", eff->ob->obmat, context);
			node->set_input_value("shape", eff->pd->shape, context);
			node->set_input_value("strength", eff->pd->f_strength, context);
			node->set_input_value("power", eff->pd->f_power, context);
			
			const NodeSocket *force = node->type->find_output("force");
			const NodeSocket *impulse = node->type->find_output("impulse");
			
			if (node_prev) {
				std::string combinename = "combine_" + node_prev->name + "_" + node->name;
				NodeInstance *combine = graph.add_node("effector_result_combine", combinename);
				
				graph.add_link(node_prev, force_prev,
				              combine, combine->type->find_input("force1"));
				graph.add_link(node_prev, force,
				              combine, combine->type->find_input("force2"));
				graph.add_link(node_prev, impulse_prev,
				              combine, combine->type->find_input("impulse1"));
				graph.add_link(node_prev, impulse,
				              combine, combine->type->find_input("impulse2"));
				
				node_prev = combine;
				force_prev = combine->type->find_output("force");
				impulse_prev = combine->type->find_output("impulse");
			}
			else {
				node_prev = node;
				force_prev = force;
				impulse_prev = impulse;
			}
		}
		
		if (node_prev) {
			graph.set_output_link("force", node_prev, force_prev->name);
			graph.set_output_link("impulse", node_prev, impulse_prev->name);
		}
		
		return graph;
	}
};

/* ------------------------------------------------------------------------- */

void build_effector_module(void)
{
	static const float vec3_zero[3] = {0.0f, 0.0f, 0.0f};
	static const float mat4_unit[4][4] = {{1.0f, 0.0f, 0.0f, 0.0f},
	                                      {0.0f, 1.0f, 0.0f, 0.0f},
	                                      {0.0f, 0.0f, 1.0f, 0.0f},
	                                      {0.0f, 0.0f, 0.0f, 1.0f}};
	
	LLVMContext &context = getGlobalContext();
	
	theModule = new Module("effectors", context);
	
	bjit_link_module(theModule);
	
	/* node types */
	for (int forcefield = 0; forcefield < NUM_PFIELD_TYPES; ++forcefield) {
		std::string name = get_effector_nodetype(forcefield);
		if (name.empty())
			continue;
		
		NodeType *type = NodeGraph::add_node_type(name);
		BLI_assert(type);
		Function *func = bjit_find_function(theModule, name);
		BLI_assert(func);
		
		type->add_input("loc", BJIT_TYPE_VEC3, vec3_zero, context);
		type->add_input("vel", BJIT_TYPE_VEC3, vec3_zero, context);
		type->add_input("transform", BJIT_TYPE_MAT4, mat4_unit, context);
		type->add_input("shape", BJIT_TYPE_INT, (int)EFF_FIELD_SHAPE_POINT, context);
		type->add_input("strength", BJIT_TYPE_FLOAT, 0.0f, context);
		type->add_input("power", BJIT_TYPE_FLOAT, 0.0f, context);
		type->add_output("force", BJIT_TYPE_VEC3, vec3_zero, context);
		type->add_output("impulse", BJIT_TYPE_VEC3, vec3_zero, context);
	}
	
	{
		NodeType *type = NodeGraph::add_node_type("effector_result_combine");
		BLI_assert(type);
		type->add_input("force1", BJIT_TYPE_VEC3, vec3_zero, context);
		type->add_input("impulse1", BJIT_TYPE_VEC3, vec3_zero, context);
		type->add_input("force2", BJIT_TYPE_VEC3, vec3_zero, context);
		type->add_input("impulse2", BJIT_TYPE_VEC3, vec3_zero, context);
		type->add_output("force", BJIT_TYPE_VEC3, vec3_zero, context);
		type->add_output("impulse", BJIT_TYPE_VEC3, vec3_zero, context);
	}
}

void free_effector_module(void)
{
	bjit_remove_module(theModule);
	
	delete theModule;
	theModule = NULL;
}

} /* namespace bjit */

void BJIT_build_effector_function(EffectorContext *effctx)
{
	using namespace bjit;
	
	NodeGraphBuilder<EffectorContext> builder;
	
	NodeGraph graph = builder.build(effctx);
//	graph.dump();
	
	Function *func = codegen(graph, theModule);
	if (func)
		func->dump();
	
	verifyFunction(*func, &outs());
	bjit_finalize_function(theModule, func, 2);
	
//	printf("=== The Module ===\n");
//	theModule->dump();
//	printf("==================\n");
	
	effctx->eval_func = bjit_compile_function(func);
	effctx->eval_data = func;
}

void BJIT_free_effector_function(EffectorContext *effctx)
{
	Function *func = (Function *)effctx->eval_data;
	
	if (func)
		bjit_free_function(func);
	
	effctx->eval_func = NULL;
	effctx->eval_data = NULL;
}

void BJIT_effector_eval(EffectorContext *effctx, const EffectorWeights *weights,
                        const EffectedPoint *point, float force[3], float impulse[3])
{
	using namespace bjit;
	
	zero_v3(force);
	zero_v3(impulse);
	
	if (effctx->eval_func) {
		EffectorEvalFunction fn = (EffectorEvalFunction)effctx->eval_func;
		
		// XXX will be used as function args
		(void)weights;
		
//		printf(" ================ EVAL ================\n");
//		printf(" == loc = (%.3f, %.3f, %.3f) ==\n", point->loc[0], point->loc[1], point->loc[2]);
//		printf(" == vel = (%.3f, %.3f, %.3f) ==\n", point->vel[0], point->vel[1], point->vel[2]);
		EffectorEvalReturn result = fn(point->loc, point->vel);
		copy_v3_v3(force, result.force);
		copy_v3_v3(impulse, result.impulse);
//		printf(" ======================================\n");
	}
}
