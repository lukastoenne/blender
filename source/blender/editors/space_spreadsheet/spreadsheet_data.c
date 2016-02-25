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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/spreadsheet_data.c
 *  \ingroup spspreadsheet
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_space_types.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_idcode.h"

#include "RNA_access.h"

#include "spreadsheet_intern.h"  /* own include */


/* NOTE: all ID types in this list should be handled in spreadsheet_get_context_id! */
bool spreadsheet_is_supported_context_id(int id_type)
{
	switch (id_type) {
		case ID_OB:
		case ID_ME:
			return true;
	}
	return false;
}

/* NOTE: if you add ID types here, also insert them in spreadsheet_is_supported_context_id! */
ID *spreadsheet_get_context_id(const bContext *C, int id_type)
{
	switch (id_type) {
		case ID_OB: {
			return (ID *)CTX_data_active_object(C);
		}
		case ID_ME: {
			Object *ob = CTX_data_active_object(C);
			return (ob && ob->type == OB_MESH) ? (ID *)ob->data : NULL;
		}
	}
	return NULL;
}

ID *spreadsheet_get_id(const bContext *C, SpaceSpreadsheet *ssheet)
{
	ID *id = NULL;
	if (ssheet->flag & SPREADSHEET_USE_PIN_ID) {
		id = ssheet->pin_id;
		if (id && GS(id->name) != ssheet->id_type)
			id = NULL;
	}
	else {
		id = spreadsheet_get_context_id(C, ssheet->id_type);
	}
	
	return id;
}

/* Try to find a valid ID block and pointer/property for the indicated spreadsheet data.
 * Returns true only when a valid ID exists and the data path can be resolved.
 */
bool spreadsheet_get_data(const bContext *C, SpaceSpreadsheet *ssheet,
                          PointerRNA *ptr, PropertyRNA **prop)
{
	ID *id = spreadsheet_get_id(C, ssheet);
	PointerRNA idptr;
	if (!id)
		return false;
	RNA_id_pointer_create(id, &idptr);
	
	if (!RNA_path_resolve_property(&idptr, ssheet->data_path, ptr, prop))
		return false;
	
	/* need a collection property */
	if (RNA_property_type(*prop) != PROP_COLLECTION)
		return false;
	
	return true;
}

int spreadsheet_get_data_fields(SpaceSpreadsheet *UNUSED(ssheet), PointerRNA *ptr, PropertyRNA *prop,
                                SpreadsheetDataField *fields, int max_fields)
{
	StructRNA *srna = RNA_property_pointer_type(ptr, prop);
	const ListBase *field_lb = RNA_struct_type_properties(srna);
	Link *field_link;
	SpreadsheetDataField *field = fields;
	
	/* Note: RNA_struct_iterator_property does not work here:
	 * it requires a struct instance in ptr, but we want to get
	 * type-only properties.
	 */
	
	for (field_link = field_lb->first; field_link; field_link = field_link->next) {
		PropertyRNA *field_prop = (PropertyRNA *)field_link;
		PropertyType field_type = RNA_property_type(field_prop);
		
		if (ELEM(field_type, PROP_COLLECTION))
			continue;
		
		field->prop = field_prop;
		
		++field;
		if (field >= fields + max_fields)
			break;
	}
	
	return field - fields;
}
