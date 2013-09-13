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

void BKE_particle_system_eval(void *context, void *item);

void BKE_rigidbody_rebuild_sim(void *context, void *item); // BKE_rigidbody_rebuild_sim
void BKE_rigidbody_eval_simulation(void *context, void *item); // BKE_rigidbody_do_simulation
void BKE_rigidbody_object_sync_transforms(void *context, void *item); // BKE_rigidbody_sync_transforms


#endif //__DEPSGRAPH_FN_STUBS_H__

