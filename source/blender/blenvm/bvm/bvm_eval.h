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

#ifndef __BVM_EVAL_H__
#define __BVM_EVAL_H__

/** \file bvm_eval.h
 *  \ingroup bvm
 */

#include <vector>

#include "bvm_util_map.h"
#include "bvm_util_string.h"
#include "bvm_util_typedesc.h"

struct Object;

namespace bvm {

struct Expression;

#define BVM_STACK_SIZE 255

#if 0
struct EvalResult {
	typedef unordered_map<string, Value> ValueMap;
	
	template <typename T>
	bool get(const string &name, T data) const
	{
		ValueMap::const_iterator it = m_values.find(name);
		if (it == m_values.end())
			return false;
		
		const Value *value = it->second;
		return value->get<T>(data);
	}
	
private:
	ValueMap m_values;
};
#endif

struct EvalGlobals {
	typedef std::vector<Object *> ObjectList;
	
	ObjectList objects;
};

struct EffectorEvalData {
	EffectorEvalData() :
	    position(0.0f, 0.0f, 0.0f),
	    velocity(0.0f, 0.0f, 0.0f)
	{}
	
	/* context */
	PointerRNA object;
	
	/* point */
	float3 position;
	float3 velocity;
};

struct TextureEvalData {
	float3 co;
	float3 dxt, dyt;
	int cfra;
	int osatex;
};

struct EvalData {
	EvalData()
	{}
	
	EffectorEvalData effector;
	TextureEvalData texture;
};

struct EvalContext {
	EvalContext();
	~EvalContext();
	
	void eval_expression(const EvalGlobals *globals, const EvalData *data, const Expression *expr, void **results) const;
	
protected:
	void eval_instructions(const EvalGlobals *globals, const EvalData *data, const Expression *expr, float *stack) const;
};

} /* namespace bvm */

#endif /* __BVM_EVAL_H__ */
