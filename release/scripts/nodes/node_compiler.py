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

from bpy.types import BVMEvalGlobals
from collections import OrderedDict
from socket_types import rna_to_bvm_type, convert_sockets

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

class InputWrapper:
    def __init__(self, gnode, ginput):
        self.gnode = gnode
        self.ginput = ginput

    @property
    def typedesc(self):
        return self.ginput.typedesc

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

    @property
    def typedesc(self):
        return self.goutput.typedesc

class NodeWrapper:
    def __init__(self, gnode):
        self.gnode = gnode
        self.inputs = StringDict([ (i.name, InputWrapper(self.gnode, i)) for i in self.gnode.inputs ])
        self.outputs = StringDict([ (o.name, OutputWrapper(self.gnode, o)) for o in self.gnode.outputs ])

# Compiler class for converting nodes
class NodeCompiler:
    def __init__(self, graph):
        self.graph = graph
        self.bnode_stack = []

    def push(self, bnode, input_map, output_map):
        # proxies for inputs/outputs
        bnode_inputs = StringDict()
        for binput in bnode.inputs:
            proxy = self.add_proxy(rna_to_bvm_type(type(binput)))
            bnode_inputs[binput.identifier] = proxy
            input_map[(bnode, binput)] = proxy.inputs[0]

            if hasattr(binput, "default_value"):
                proxy.inputs[0].set_value(binput.default_value)
        
        bnode_outputs = StringDict()
        for boutput in bnode.outputs:
            proxy = self.add_proxy(rna_to_bvm_type(type(boutput)))
            bnode_outputs[boutput.identifier] = proxy
            output_map[(bnode, boutput)] = proxy.outputs[0]

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

    def graph_input(self, name):
        in_node, in_socket = self.graph.get_input(name)
        return OutputWrapper(in_node, in_node.outputs[in_socket])

    def graph_output(self, name):
        out_node, out_socket = self.graph.get_output(name)
        return InputWrapper(out_node, out_node.inputs[out_socket])

    def link(self, from_output, to_input, autoconvert=True):
        if autoconvert:
            from_output = convert_sockets(self, from_output, to_input)
        if from_output is not None:
            to_input.gnode.set_input_link(to_input.ginput, from_output.gnode, from_output.goutput)

    def map_input(self, key, socket):
        bnode, bnode_inputs, bnode_outputs = self.bnode_stack[-1]
        if key not in bnode_inputs:
            raise KeyError("Input %r not found in node %r" % (key, bnode))
        self.link(bnode_inputs[key].outputs[0], socket)

    def map_output(self, key, socket):
        bnode, bnode_inputs, bnode_outputs = self.bnode_stack[-1]
        if key not in bnode_outputs:
            raise KeyError("Output %r not found in node %r" % (key, bnode))
        self.link(socket, bnode_outputs[key].inputs[0])

    def map_input_external(self, key, socket):
        if len(self.bnode_stack) < 2:
            return
        bnode, bnode_inputs, bnode_outputs = self.bnode_stack[-2]
        if key not in bnode_inputs:
            raise KeyError("Input %r not found in node %r" % (key, bnode))
        self.link(bnode_inputs[key].outputs[0], socket)

    def map_output_external(self, key, socket):
        if len(self.bnode_stack) < 2:
            return
        bnode, bnode_inputs, bnode_outputs = self.bnode_stack[-2]
        if key not in bnode_outputs:
            raise KeyError("Output %r not found in node %r" % (key, bnode))
        self.link(socket, bnode_outputs[key].inputs[0])

    @staticmethod
    def get_id_key(id_data):
        return BVMEvalGlobals.get_id_key(id_data) if id_data else 0

###############################################################################

def register():
    pass

def unregister():
    pass
