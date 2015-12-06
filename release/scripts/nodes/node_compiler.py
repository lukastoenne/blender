# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>

from collections import OrderedDict

# Utility dict type that works like RNA collections,
# i.e. accepts both string keys and integer indices
class StringDict(OrderedDict):
    def __getitem__(self, key):
        if isinstance(key, int):
            return super().__getitem__(list(super().keys())[key])
        else:
            return super().__getitem__(key)

    def __contains__(self, key):
        if isinstance(key, int):
            return super().__contains__(list(super().keys())[key])
        else:
            return super().__contains__(key)

# Wrapper classes to make constructing node graphs as convenient as possible
# RNA does not allow collections of temporary (node,socket) pairs,
# so we use python wrappers to pass them around as a single object

def convert_sockets(compiler, from_typedesc, to_typedesc):
    from_type = from_typedesc.base_type
    to_type = to_typedesc.base_type

    if to_type == from_type:
        node = compiler.add_node("PASS_%s" % to_type)
        return {node.inputs[0]}, node.outputs[0]

    if to_type == 'FLOAT':
        if from_type == 'INT':
            node = compiler.add_node("INT_TO_FLOAT")
            return {node.inputs[0]}, node.outputs[0]
        elif from_type == 'FLOAT3':
            node = compiler.add_node("SET_FLOAT3")
            return {node.inputs[0], node.inputs[1], node.inputs[2]}, node.outputs[0]
        elif from_type == 'FLOAT4':
            node = compiler.add_node("SET_FLOAT4")
            return {node.inputs[0], node.inputs[1], node.inputs[2], node.inputs[3]}, node.outputs[0]
    
    elif to_type == 'FLOAT3':
        pass
    
    elif to_type == 'FLOAT4':
        pass
    
    elif to_type == 'INT':
        if from_type == 'FLOAT':
            node = compiler.add_node("FLOAT_TO_INT")
            return {node.inputs[0]}, node.outputs[0]
    
    elif to_type == 'MATRIX44':
        pass

    return set(), None

class InputWrapper:
    def __init__(self, gnode, ginput):
        self.gnode = gnode
        self.ginput = ginput

    def set_value(self, value):
        base_type = self.ginput.typedesc.base_type
        if base_type == 'FLOAT':
            self.gnode.set_value_float(self.ginput, value)
        elif base_type == 'FLOAT3':
            self.gnode.set_value_float3(self.ginput, value)
        elif base_type == 'FLOAT4':
            self.gnode.set_value_float4(self.ginput, value)
        elif base_type == 'INT':
            self.gnode.set_value_int(self.ginput, value)
        elif base_type == 'MATRIX44':
            self.gnode.set_value_matrix44(self.ginput, value)

class OutputWrapper:
    def __init__(self, gnode, goutput):
        self.gnode = gnode
        self.goutput = goutput

class NodeWrapper:
    def __init__(self, gnode):
        self.gnode = gnode
        self.inputs = StringDict([ (i.name, InputWrapper(self.gnode, i)) for i in self.gnode.inputs ])
        self.outputs = StringDict([ (o.name, OutputWrapper(self.gnode, o)) for o in self.gnode.outputs ])

# Compiler class for converting nodes
class NodeCompiler:
    def __init__(self, context, graph):
        self.context = context
        self.graph = graph
        self.bnode_stack = []

    def push(self, bnode, bnode_inputs, bnode_outputs):
        self.bnode_stack.append((bnode, bnode_inputs, bnode_outputs))

    def pop(self):
        self.bnode_stack.pop()

    def add_node(self, type, name=""):
        node = self.graph.add_node(type, name)
        if node is None:
            raise Exception("Can not add node of type %r" % type)
        return NodeWrapper(node)

    def add_proxy(self, type):
        return self.add_node("PASS_%s" % type)

    def graph_output(self, name):
        out_node, out_socket = self.graph.get_output(name)
        return InputWrapper(out_node, out_node.inputs[out_socket])

    def link(self, from_output, to_input, autoconvert=True):
        if autoconvert:
            cin, cout = convert_sockets(self, from_output.goutput.typedesc, to_input.ginput.typedesc)
            if cout:
                to_input.gnode.set_input_link(to_input.ginput, cout.gnode, cout.goutput)
            for i in cin:
                i.gnode.set_input_link(i.ginput, from_output.gnode, from_output.goutput)
        else:
            to_input.gnode.set_input_link(to_input.ginput, from_output.gnode, from_output.goutput)

    def map_input(self, key, socket):
        bnode, bnode_inputs, bnode_outputs = self.bnode_stack[-1]
        if key not in bnode_inputs:
            raise KeyError("Input %r not found in node %r" % (key, bnode))
        self.link(bnode_inputs[key], socket)

    def map_output(self, key, socket):
        bnode, bnode_inputs, bnode_outputs = self.bnode_stack[-1]
        if key not in bnode_outputs:
            raise KeyError("Output %r not found in node %r" % (key, bnode))
        self.link(socket, bnode_outputs[key])

###############################################################################

def register():
    pass

def unregister():
    pass
