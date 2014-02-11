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

#include "COM_SocketConnection.h"
#include "COM_NodeOperation.h"

SocketConnection::SocketConnection(OutputSocket *from, InputSocket *to) :
    m_fromSocket(from),
    m_toSocket(to)
{
	BLI_assert(from);
	BLI_assert(to);
}

SocketConnection::~SocketConnection()
{
}

bool SocketConnection::needsResolutionConversion() const
{
	if (m_toSocket->getResizeMode() == COM_SC_NO_RESIZE)
		return false;
	
	NodeOperation *fromOperation = this->getFromOperation();
	NodeOperation *toOperation = this->getToOperation();
	const unsigned int fromWidth = fromOperation->getWidth();
	const unsigned int fromHeight = fromOperation->getHeight();
	const unsigned int toWidth = toOperation->getWidth();
	const unsigned int toHeight = toOperation->getHeight();
	
	if (fromWidth == toWidth && fromHeight == toHeight)
		return false;
	
	return true;
}
