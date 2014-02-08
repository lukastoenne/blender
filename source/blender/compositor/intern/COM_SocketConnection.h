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

#ifndef _COM_SocketConnection_h
#define _COM_SocketConnection_h

#include "DNA_node_types.h"
#include "COM_Node.h"
#include "COM_Socket.h"
#include "COM_ChannelInfo.h"

/**
 * @brief An SocketConnection is an connection between an InputSocket and an OutputSocket.
 *
 * <pre>
 * +----------+     To InputSocket +----------+
 * | From     |  SocketConnection \| To Node  |
 * | Node     *====================*          |
 * |          |\                   |          |
 * |          | From OutputSocket  +----------+
 * +----------+
 * </pre>
 * @ingroup Model
 * @see InputSocket
 * @see OutputSocket
 */
class SocketConnection {
private:
	/**
	 * @brief Startpoint of the connection
	 */
	OutputSocket *m_fromSocket;
	
	/**
	 * @brief Endpoint of the connection
	 */
	InputSocket *m_toSocket;
	
	/**
	 * @brief has the resize already been done for this connection
	 */
	bool m_ignoreResizeCheck;
public:
	SocketConnection(OutputSocket *from, InputSocket *to);
	~SocketConnection();
	
	OutputSocket *getFromSocket() const { return this->m_fromSocket; }
	InputSocket *getToSocket() const { return this->m_toSocket; }
	
	NodeBase *getFromNode() const { return m_fromSocket ? m_fromSocket->getNode() : NULL; }
	NodeBase *getToNode() const { return m_toSocket ? m_toSocket->getNode() : NULL; }
	
	NodeOperation *getFromOperation() const { return m_fromSocket ? (NodeOperation *)m_fromSocket->getNode() : NULL; }
	NodeOperation *getToOperation() const { return m_toSocket ? (NodeOperation *)m_toSocket->getNode() : NULL; }
	
	/**
	 * @brief set, whether the resize has already been done for this SocketConnection
	 */
	void setIgnoreResizeCheck(bool check) { this->m_ignoreResizeCheck = check; }
	
	/**
	 * @brief has the resize already been done for this SocketConnection
	 */
	bool isIgnoreResizeCheck() const { return this->m_ignoreResizeCheck; }
	
	/**
	 * @brief does this SocketConnection need resolution conversion
	 * @note PreviewOperation's will be ignored
	 * @note Already converted SocketConnection's will be ignored
	 * @return needs conversion [true:false]
	 */
	bool needsResolutionConversion() const;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:SocketConnection")
#endif
};

#endif
