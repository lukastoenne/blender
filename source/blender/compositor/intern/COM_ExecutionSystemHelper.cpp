/*
 * Copyright 2011, Blender Foundation.
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_ExecutionSystemHelper.h"

#include "PIL_time.h"

#include "COM_Converter.h"
#include "COM_NodeOperation.h"
#include "COM_ExecutionGroup.h"
#include "COM_NodeBase.h"
#include "COM_WorkScheduler.h"
#include "COM_ReadBufferOperation.h"
#include "COM_WriteBufferOperation.h"
#include "COM_ReadBufferOperation.h"
#include "COM_ViewerOperation.h"
#include "COM_Debug.h"

extern "C" {
#include "BKE_node.h"
}

void ExecutionSystemHelper::addOperation(vector<NodeOperation *>& operations, NodeOperation *operation)
{
	operations.push_back(operation);
}

void ExecutionSystemHelper::addExecutionGroup(vector<ExecutionGroup *>& executionGroups, ExecutionGroup *executionGroup)
{
	executionGroups.push_back(executionGroup);
}

void ExecutionSystemHelper::findOutputNodeOperations(vector<NodeOperation *> *result, vector<NodeOperation *>& operations, bool rendering)
{
	unsigned int index;

	for (index = 0; index < operations.size(); index++) {
		NodeOperation *operation = operations[index];
		if (operation->isOutputOperation(rendering)) {
			result->push_back(operation);
		}
	}
}
