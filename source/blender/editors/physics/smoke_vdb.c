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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file smoke_vdb.c
 *  \ingroup editor_physics
 *  \brief Smoke operators
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_smoke_types.h"

#include "BLI_blenlib.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_report.h"
#include "BKE_smoke.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_physics.h"
#include "ED_screen.h"

#include "physics_intern.h"

static SmokeModifierData *ED_smoke_active(bContext *C)
{
	Object *ob = ED_object_active_context(C);
	ModifierData *md;
	
	if (!ob)
		return NULL;
	
	md = modifiers_findByType(ob, eModifierType_Smoke);
	return (SmokeModifierData *)md;
}

static SmokeDomainVDBSettings *ED_smoke_domain_vdb_active(bContext *C)
{
	SmokeModifierData *smd = ED_smoke_active(C);
	if (smd && smd->type == MOD_SMOKE_TYPE_DOMAIN_VDB)
		return smd->domain_vdb;
	else
		return NULL;
}

static int ED_operator_smoke_domain_vdb(bContext *C)
{
	return ED_smoke_domain_vdb_active(C) != NULL;
}

/* ------------------------------------------------------------------------- */

static int smoke_display_value_adjust_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_active_context(C);
	SmokeDomainVDBSettings *sds = ED_smoke_domain_vdb_active(C);
	
	smoke_vdb_display_range_adjust(sds);
	
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	return OPERATOR_FINISHED;
}

void SMOKE_OT_display_value_adjust(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname = "SMOKE_OT_display_value_adjust";
	ot->name = "Adjust Display Value";
	ot->description = "Automatically set display value min/max from existing data range";

	/* callbacks */
	ot->exec = smoke_display_value_adjust_exec;
	ot->poll = ED_operator_smoke_domain_vdb;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************************************************************* */
