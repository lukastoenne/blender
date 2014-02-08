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

#ifndef _COM_InputSocket_h
#define _COM_InputSocket_h

#include <vector>
#include "COM_Socket.h"
#include "COM_SocketReader.h"

using namespace std;
class SocketConnection;
class Node;
class NodeCompiler;
class OutputSocket;
class ChannelInfo;
class NodeOperation;

/**
 * @brief Resize modes of inputsockets
 * How are the input and working resolutions matched
 * @ingroup Model
 */
typedef enum InputSocketResizeMode {
	/** @brief Center the input image to the center of the working area of the node, no resizing occurs */
	COM_SC_CENTER = NS_CR_CENTER,
	/** @brief The bottom left of the input image is the bottom left of the working area of the node, no resizing occurs */
	COM_SC_NO_RESIZE = NS_CR_NONE,
	/** @brief Fit the width of the input image to the width of the working area of the node */
	COM_SC_FIT_WIDTH = NS_CR_FIT_WIDTH,
	/** @brief Fit the height of the input image to the height of the working area of the node */
	COM_SC_FIT_HEIGHT = NS_CR_FIT_HEIGHT,
	/** @brief Fit the width or the height of the input image to the width or height of the working area of the node, image will be larger than the working area */
	COM_SC_FIT = NS_CR_FIT,
	/** @brief Fit the width and the height of the input image to the width and height of the working area of the node, image will be equally larger than the working area */
	COM_SC_STRETCH = NS_CR_STRETCH
} InputSocketResizeMode;

/**
 * @brief InputSocket are sockets that can receive data/input
 * @ingroup Model
 */
class InputSocket : public Socket {
private:
	/**
	 * @brief connection connected to this InputSocket.
	 * An input socket can only have a single connection
	 */
	SocketConnection *m_connection;
	
	/**
	 * @brief resize mode of this socket
	 */
	InputSocketResizeMode m_resizeMode;
	
	
public:
	InputSocket(DataType datatype);
	InputSocket(DataType datatype, InputSocketResizeMode resizeMode);
	InputSocket(InputSocket *from);
	
	void setConnection(SocketConnection *connection);
	SocketConnection *getConnection();
	
	const int isConnected() const;
	int isInputSocket() const;
	
	/**
	 * @brief determine the resolution of this data going through this socket
	 * @param resolution the result of this operation
	 * @param preferredResolution the preferable resolution as no resolution could be determined
	 */
	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);
	
	/**
	 * @brief set the resize mode
	 * @param resizeMode the new resize mode.
	 */
	void setResizeMode(InputSocketResizeMode resizeMode) {
		this->m_resizeMode = resizeMode;
	}
	
	/**
	 * @brief get the resize mode of this socket
	 * @return InputSocketResizeMode
	 */
	InputSocketResizeMode getResizeMode() const {
		return this->m_resizeMode;
	}
	
	const ChannelInfo *getChannelInfo(const int channelnumber);
	
	bool isStatic();
	
	SocketReader *getReader();
	NodeOperation *getOperation() const;
};

#endif
