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

#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "BKE_effect.h"
}

#include "BJIT_forcefield.h"
#include "bjit_intern.h"
#include "bjit_nodegraph.h"

using namespace llvm;

static Module *theModule = NULL;

/* ------------------------------------------------------------------------- */
/* specialization of TypeBuilder for external struct types */

namespace llvm {

#if 0
template<bool cross>
class TypeBuilder<EffectorEvalInput, cross> {
public:
	static StructType *get(LLVMContext &context) {
		// If you cache this result, be sure to cache it separately
		// for each LLVMContext.
		return StructType::get(
		            TypeBuilder<vec3_t, cross>::get(context),
		            TypeBuilder<vec3_t, cross>::get(context),
		            NULL);
	}
	
	// You may find this a convenient place to put some constants
	// to help with getelementptr.  They don't have any effect on
	// the operation of TypeBuilder.
	enum Fields {
		FIELD_LOC,
		FIELD_VEL,
		NUM_FIELDS
	};
};

template<bool cross>
class TypeBuilder<EffectorEvalResult, cross> {
public:
	static StructType *get(LLVMContext &context) {
		return StructType::get(
		            TypeBuilder<vec3_t, cross>::get(context),
		            TypeBuilder<vec3_t, cross>::get(context),
		            NULL);
	}
	
	enum Fields {
		FIELD_FORCE,
		FIELD_IMPULSE,
		NUM_FIELDS
	};
};

template <bool cross, typename HeadT, typename... TailT>
struct StructTypeBuilder {
	static StructType *get(LLVMContext &context, SmallVector<Type*, 8> &struct_fields)
	{
		Type *head = TypeBuilder<HeadT, cross>::get(context);
		struct_fields.push_back(head);
		return StructTypeBuilder<cross, TailT...>::get(context, struct_fields);
	}
};

/* terminator */
template <bool cross, typename HeadT>
struct StructTypeBuilder<cross, HeadT> {
	static StructType *get(LLVMContext &context, SmallVector<Type*, 8> &struct_fields)
	{
		Type *head = TypeBuilder<HeadT, cross>::get(context);
		struct_fields.push_back(head);
		return StructType::get(context, struct_fields);
	}
};

template <bool cross, typename... ArgsT>
static StructType *build_struct_type(LLVMContext &context)
{
	SmallVector<Type*, 8> struct_fields;
	return StructTypeBuilder<cross, ArgsT...>::get(context, struct_fields);
}

template<bool cross>
class TypeBuilder<EffectorEvalSettings, cross> {
public:
	static StructType *get(LLVMContext &context) {
		return build_struct_type<cross,
		        mat4_t, mat4_t, types::i<32>, types::i<16>, types::i<16>,
		        types::ieee_float, types::ieee_float, types::ieee_float, types::ieee_float,
		        types::ieee_float, types::ieee_float, types::ieee_float,
		        types::ieee_float, types::ieee_float, types::ieee_float,
		        types::ieee_float
		        >(context);
	}
	
	enum Fields {
		FIELD_TRANSFORM,
		FIELD_ITRANSFORM,
		FIELD_FLAG,
		FIELD_FALLOFF_TYPE,
		FIELD_SHAPE_TYPE,
		FIELD_STRENGTH,
		FIELD_DAMP,
		FIELD_FLOW,
		FIELD_SIZE,
		FIELD_FALLOFF_POWER,
		FIELD_FALLOFF_MIN,
		FIELD_FALLOFF_MAX,
		FIELD_FALLOFF_RAD_POWER,
		FIELD_FALLOFF_RAD_MIN,
		FIELD_FALLOFF_RAD_MAX,
		FIELD_ABSORBTION,
		NUM_FIELDS
	};
};
#endif

} /* namespace llvm */

/* ------------------------------------------------------------------------- */

namespace bjit {

typedef typename SocketTypeImpl<BJIT_TYPE_VEC3>::extern_type vec3_extern_t;

struct EffectorEvalReturn {
	vec3_extern_t force;
	vec3_extern_t impulse;
};

typedef EffectorEvalReturn (*EffectorEvalFunction)(const vec3_extern_t &loc, const vec3_extern_t &vel);

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
		
//		for (Function::ArgumentListType::iterator it = func->arg_begin(); it != func->arg_end(); ++it) {
//			Argument *arg = *it;
//			ValueName *argname = arg->getValueName();
//			Type *argtype = arg->getType();
			
//			argname->g
//		}
		type->add_input("loc", BJIT_TYPE_VEC3, vec3_zero, context);
		type->add_input("vel", BJIT_TYPE_VEC3, vec3_zero, context);
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


#if 0
static std::string get_effector_prefix(short forcefield)
{
	switch (forcefield) {
		case PFIELD_FORCE:
			return "effector_force";
		case PFIELD_WIND:
			return "effector_wind";
			
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
		
		default: {
			/* unknown type, should not happen */
			BLI_assert(false);
			return "";
		}
	}
	return "";
}

static Constant *make_const_int(LLVMContext &context, int value)
{
	return ConstantInt::get(context, APInt(32, value));
}

static Constant *make_const_short(LLVMContext &context, short value)
{
	return ConstantInt::get(context, APInt(16, value));
}

static Constant *make_const_float(LLVMContext &context, float value)
{
	return ConstantFP::get(context, APFloat(value));
}

static Constant *make_const_vec3(LLVMContext &context, const float vec[3])
{
	return ConstantDataArray::get(context, ArrayRef<float>(vec, 3));
}

static Constant *make_const_vec4(LLVMContext &context, const float vec[4])
{
	return ConstantDataArray::get(context, ArrayRef<float>(vec, 4));
}

static Constant *make_const_mat4(LLVMContext &context, float mat[4][4])
{
	Constant *col1 = make_const_vec4(context, mat[0]);
	Constant *col2 = make_const_vec4(context, mat[1]);
	Constant *col3 = make_const_vec4(context, mat[2]);
	Constant *col4 = make_const_vec4(context, mat[3]);
	return ConstantArray::get(TypeBuilder<mat4_t, true>::get(context), std::vector<Constant*>({col1, col2, col3, col4}));
}

static Value *make_effector_settings(IRBuilder<> &builder, EffectorCache *eff)
{
	LLVMContext &context = builder.getContext();
	typedef TypeBuilder<EffectorEvalSettings, true> TB;
	StructType *settings_t = TB::get(context);
	
	float imat[4][4];
	invert_m4_m4(imat, eff->ob->obmat);
	
	Constant* fields[TB::NUM_FIELDS];
	fields[TB::FIELD_TRANSFORM] = make_const_mat4(context, eff->ob->obmat);
	fields[TB::FIELD_ITRANSFORM] = make_const_mat4(context, imat);
	fields[TB::FIELD_FLAG] = make_const_int(context, eff->pd->flag);
	fields[TB::FIELD_FALLOFF_TYPE] = make_const_short(context, eff->pd->falloff);
	fields[TB::FIELD_SHAPE_TYPE] = make_const_short(context, eff->pd->shape);
	fields[TB::FIELD_STRENGTH] = make_const_float(context, eff->pd->f_strength);
	fields[TB::FIELD_DAMP] = make_const_float(context, eff->pd->f_damp);
	fields[TB::FIELD_FLOW] = make_const_float(context, eff->pd->f_flow);
	fields[TB::FIELD_SIZE] = make_const_float(context, eff->pd->f_size);
	fields[TB::FIELD_FALLOFF_POWER] = make_const_float(context, eff->pd->f_power);
	fields[TB::FIELD_FALLOFF_MIN] = make_const_float(context, eff->pd->mindist);
	fields[TB::FIELD_FALLOFF_MAX] = make_const_float(context, eff->pd->maxdist);
	fields[TB::FIELD_FALLOFF_RAD_POWER] = make_const_float(context, eff->pd->f_power_r);
	fields[TB::FIELD_FALLOFF_RAD_MIN] = make_const_float(context, eff->pd->minrad);
	fields[TB::FIELD_FALLOFF_RAD_MAX] = make_const_float(context, eff->pd->maxrad);
	fields[TB::FIELD_ABSORBTION] = make_const_float(context, eff->pd->absorption);
	Constant *settings = ConstantStruct::get(settings_t, ArrayRef<Constant*>(fields, TB::NUM_FIELDS));
	
	AllocaInst *var = builder.CreateAlloca(settings_t);
	builder.CreateStore(settings, var);
	return var;
}
static Value *make_effector_result(IRBuilder<> &builder)
{
	LLVMContext &context = builder.getContext();
	typedef TypeBuilder<EffectorEvalResult, true> TB;
	StructType *result_t = TB::get(context);
	const static float ZERO[3] = {0.0f, 0.0f, 0.0f};
	
	Constant* fields[TB::NUM_FIELDS];
	fields[TB::FIELD_FORCE] = make_const_vec3(context, ZERO);
	fields[TB::FIELD_IMPULSE] = make_const_vec3(context, ZERO);
	Constant *result = ConstantStruct::get(result_t, ArrayRef<Constant*>(fields, TB::NUM_FIELDS));
	
	AllocaInst *var = builder.CreateAlloca(result_t);
	builder.CreateStore(result, var);
	return var;
}
#endif

void BJIT_build_effector_function(EffectorContext *effctx)
{
	using namespace bjit;
	
	NodeGraphBuilder<EffectorContext> builder;
	
	NodeGraph graph = builder.build(effctx);
	graph.dump();
	
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
		
		EffectorEvalReturn result = fn((const vec3_extern_t &)point->loc, (const vec3_extern_t &)point->vel);
		copy_v3_v3(force, result.force);
		copy_v3_v3(impulse, result.impulse);
	}
}
