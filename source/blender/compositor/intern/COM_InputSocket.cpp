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

#include "COM_Socket.h"
#include "COM_Node.h"
#include "COM_SocketConnection.h"
#include "COM_ExecutionSystem.h"

InputSocket::InputSocket(DataType datatype) : Socket(datatype)
{
	this->m_connection = NULL;
	this->m_resizeMode = COM_SC_CENTER;
}
InputSocket::InputSocket(DataType datatype, InputSocketResizeMode resizeMode) : Socket(datatype)
{
	this->m_connection = NULL;
	this->m_resizeMode = resizeMode;
}

InputSocket::InputSocket(InputSocket *from) : Socket(from->getDataType())
{
	this->m_connection = NULL;
	this->m_resizeMode = from->getResizeMode();
}

int InputSocket::isInputSocket() const { return true; }
const int InputSocket::isConnected() const { return this->m_connection != NULL; }

void InputSocket::setConnection(SocketConnection *connection)
{
	this->m_connection = connection;
}
SocketConnection *InputSocket::getConnection()
{
	return this->m_connection;
}

void InputSocket::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	if (this->isConnected()) {
		this->m_connection->getFromSocket()->determineResolution(resolution, preferredResolution);
	}
	else {
		return;
	}
}

bool InputSocket::isStatic()
{
	if (isConnected()) {
		NodeBase *node = this->getConnection()->getFromNode();
		if (node) {
			return node->isStatic();
		}
	}
	return true;
}
SocketReader *InputSocket::getReader()
{
	return this->getOperation();
}

NodeOperation *InputSocket::getOperation() const
{
	if (isConnected()) {
		return (NodeOperation *)this->m_connection->getFromSocket()->getNode();
	}
	else {
		return NULL;
	}
}
