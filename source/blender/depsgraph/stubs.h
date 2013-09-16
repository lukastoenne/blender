/* This file defines some stub function defs used 
 * for various operation callbacks which have not
 * been implemented in Blender yet. It should
 * eventually disappear instead of remaining as
 * part of the code base.
 */

#ifndef __DEPSGRAPH_FN_STUBS_H__
#define __DEPSGRAPH_FN_STUBS_H__

#pragma message("DEPSGRAPH PORTING XXX: There are still some undefined stubs")

void BKE_animsys_eval_driver(void *context, void *item);

void BKE_constraints_evaluate(void *context, void *item);
void BKE_pose_iktree_evaluate(void *context, void *item);
void BKE_pose_splineik_evaluate(void *context, void *item);
void BKE_pose_eval_bone(void *context, void *item);

void BKE_pose_rebuild_op(void *context, void *item);
void BKE_pose_eval_init(void *context, void *item);
void BKE_pose_eval_flush(void *context, void *item);

void BKE_particle_system_eval(void *context, void *item);

void BKE_rigidbody_rebuild_sim(void *context, void *item); // BKE_rigidbody_rebuild_sim
void BKE_rigidbody_eval_simulation(void *context, void *item); // BKE_rigidbody_do_simulation
void BKE_rigidbody_object_sync_transforms(void *context, void *item); // BKE_rigidbody_sync_transforms

void BKE_object_eval_local_transform(void *context, void *item);
void BKE_object_eval_parent(void *context, void *item);

void BKE_mesh_eval_geometry(void *context, void *item);  // wrapper around makeDerivedMesh() - which gets BMesh, etc. data...
void BKE_mball_eval_geometry(void *context, void *item); // BKE_displist_make_mball
void BKE_curve_eval_geometry(void *context, void *item); // BKE_displist_make_curveTypes
void BKE_curve_eval_path(void *context, void *item);
void BKE_lattice_eval_geometry(void *context, void *item); // BKE_lattice_modifiers_calc

/* Priority Queue type */
typedef struct Queue {
	void *items;
	size_t size;
} Queue;

Queue *queue_new(void);
void queue_push(Queue *q, int priority, void *data);
void *queue_pop(Queue *q);
bool queue_is_empty(Queue *q);
void queue_free(Queue *q);

#endif //__DEPSGRAPH_FN_STUBS_H__

