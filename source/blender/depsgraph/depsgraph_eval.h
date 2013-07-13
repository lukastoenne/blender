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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Evaluation-Related Defines for Depsgraph
 *
 * The types defined here are for things like:
 * 1) how we represent data state and/or pass this on to the 
 *    evaluation functions at runtime, or 
 * 2) types which can be used for declaring automating 
 *    graph-building process
 */

#ifndef __DEPSGRAPH_EVAL_TYPES_H__
#define __DEPSGRAPH_EVAL_TYPES_H__

/* ****************************************** */
/* Evaluation State */

// XXX: dummy... need to flesh this out
// - stuff here for now is just rough draft of what might go here (based on ThreadedObjectUpdateState)
typedef struct DegState {
	ID *id;
	void *data;
	
	SpinLock lock;
} DegState;

/* ****************************************** */
/* Graph Build Types */

/* Standard 3-step composition... 
 *
 * Examples (may not be actually done) of this include:
 * - Evaluating a single constraint
 */
typedef struct sDegEvalTemplate_Standard {
	char name[128];                       /* debugging identifier for this setup */
	
	DEG_AtomicEvalOperation_Cb init;	  /* initialisation (i.e. "preparation") step */
	DEG_AtomicEvalOperation_Cb exec;      /* evaluation of data */
	DEG_AtomicEvalOperation_Cb cleanup;   /* cleanup (i.e. "freeing") step */
} sDegEvalTemplate_Standard;

/* Standard 3-step where exec() step is actually a sub-graph... */
// XXX...

/* Arbitrarily many steps... */
// XXX...


/* ****************************************** */

#endif // __DEPSGRAPH_EVAL_TYPES_H__
