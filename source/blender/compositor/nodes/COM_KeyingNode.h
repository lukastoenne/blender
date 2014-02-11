/*
 * Copyright 2012, Blender Foundation.
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
 *		Sergey Sharybin
 */

#include "COM_Node.h"

/**
 * @brief KeyingNode
 * @ingroup Node
 */
class KeyingNode : public Node {
protected:
	OutputSocket *setupPreBlur(NodeCompiler *compiler, InputSocket *inputImage, int size) const;
	OutputSocket *setupPostBlur(NodeCompiler *compiler, OutputSocket *postBlurInput, int size) const;
	OutputSocket *setupDilateErode(NodeCompiler *compiler, OutputSocket *dilateErodeInput, int distance) const;
	OutputSocket *setupFeather(NodeCompiler *compiler, const CompositorContext *context, OutputSocket *featherInput,
	                           int falloff, int distance) const;
	OutputSocket *setupDespill(NodeCompiler *compiler, OutputSocket *despillInput, OutputSocket *inputSrceen,
	                           float factor, float colorBalance) const;
	OutputSocket *setupClip(NodeCompiler *compiler, OutputSocket *clipInput, int kernelRadius, float kernelTolerance,
	                        float clipBlack, float clipWhite, bool edgeMatte) const;
public:
	KeyingNode(bNode *editorNode);
	void convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const;

};
