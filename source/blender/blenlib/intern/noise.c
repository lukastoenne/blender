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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenlib/intern/noise.c
 *  \ingroup bli
 */

#ifdef __SSE2__
#define SIMPLEXNOISE_USE_SSE2
#endif

#include <math.h>

#ifdef SIMPLEXNOISE_USE_SSE2
#  include <emmintrin.h>
#endif

#include "BLI_math.h"
#include "BLI_noise.h"
#include "BLI_utildefines.h"

/* local */
static float noise3_perlin(float vec[3]);
//static float turbulence_perlin(const float point[3], float lofreq, float hifreq);
//static float turbulencep(float noisesize, float x, float y, float z, int nr);

/* UNUSED */
// #define HASHVEC(x, y, z) hashvectf + 3 * hash[(hash[(hash[(z) & 255] + (y)) & 255] + (x)) & 255]

/* needed for voronoi */
#define HASHPNT(x, y, z) hashpntf + 3 * hash[(hash[(hash[(z) & 255] + (y)) & 255] + (x)) & 255]
static const float hashpntf[768] = {
	0.536902, 0.020915, 0.501445, 0.216316, 0.517036, 0.822466, 0.965315,
	0.377313, 0.678764, 0.744545, 0.097731, 0.396357, 0.247202, 0.520897,
	0.613396, 0.542124, 0.146813, 0.255489, 0.810868, 0.638641, 0.980742,
	0.292316, 0.357948, 0.114382, 0.861377, 0.629634, 0.722530, 0.714103,
	0.048549, 0.075668, 0.564920, 0.162026, 0.054466, 0.411738, 0.156897,
	0.887657, 0.599368, 0.074249, 0.170277, 0.225799, 0.393154, 0.301348,
	0.057434, 0.293849, 0.442745, 0.150002, 0.398732, 0.184582, 0.915200,
	0.630984, 0.974040, 0.117228, 0.795520, 0.763238, 0.158982, 0.616211,
	0.250825, 0.906539, 0.316874, 0.676205, 0.234720, 0.667673, 0.792225,
	0.273671, 0.119363, 0.199131, 0.856716, 0.828554, 0.900718, 0.705960,
	0.635923, 0.989433, 0.027261, 0.283507, 0.113426, 0.388115, 0.900176,
	0.637741, 0.438802, 0.715490, 0.043692, 0.202640, 0.378325, 0.450325,
	0.471832, 0.147803, 0.906899, 0.524178, 0.784981, 0.051483, 0.893369,
	0.596895, 0.275635, 0.391483, 0.844673, 0.103061, 0.257322, 0.708390,
	0.504091, 0.199517, 0.660339, 0.376071, 0.038880, 0.531293, 0.216116,
	0.138672, 0.907737, 0.807994, 0.659582, 0.915264, 0.449075, 0.627128,
	0.480173, 0.380942, 0.018843, 0.211808, 0.569701, 0.082294, 0.689488,
	0.573060, 0.593859, 0.216080, 0.373159, 0.108117, 0.595539, 0.021768,
	0.380297, 0.948125, 0.377833, 0.319699, 0.315249, 0.972805, 0.792270,
	0.445396, 0.845323, 0.372186, 0.096147, 0.689405, 0.423958, 0.055675,
	0.117940, 0.328456, 0.605808, 0.631768, 0.372170, 0.213723, 0.032700,
	0.447257, 0.440661, 0.728488, 0.299853, 0.148599, 0.649212, 0.498381,
	0.049921, 0.496112, 0.607142, 0.562595, 0.990246, 0.739659, 0.108633,
	0.978156, 0.209814, 0.258436, 0.876021, 0.309260, 0.600673, 0.713597,
	0.576967, 0.641402, 0.853930, 0.029173, 0.418111, 0.581593, 0.008394,
	0.589904, 0.661574, 0.979326, 0.275724, 0.111109, 0.440472, 0.120839,
	0.521602, 0.648308, 0.284575, 0.204501, 0.153286, 0.822444, 0.300786,
	0.303906, 0.364717, 0.209038, 0.916831, 0.900245, 0.600685, 0.890002,
	0.581660, 0.431154, 0.705569, 0.551250, 0.417075, 0.403749, 0.696652,
	0.292652, 0.911372, 0.690922, 0.323718, 0.036773, 0.258976, 0.274265,
	0.225076, 0.628965, 0.351644, 0.065158, 0.080340, 0.467271, 0.130643,
	0.385914, 0.919315, 0.253821, 0.966163, 0.017439, 0.392610, 0.478792,
	0.978185, 0.072691, 0.982009, 0.097987, 0.731533, 0.401233, 0.107570,
	0.349587, 0.479122, 0.700598, 0.481751, 0.788429, 0.706864, 0.120086,
	0.562691, 0.981797, 0.001223, 0.192120, 0.451543, 0.173092, 0.108960,
	0.549594, 0.587892, 0.657534, 0.396365, 0.125153, 0.666420, 0.385823,
	0.890916, 0.436729, 0.128114, 0.369598, 0.759096, 0.044677, 0.904752,
	0.088052, 0.621148, 0.005047, 0.452331, 0.162032, 0.494238, 0.523349,
	0.741829, 0.698450, 0.452316, 0.563487, 0.819776, 0.492160, 0.004210,
	0.647158, 0.551475, 0.362995, 0.177937, 0.814722, 0.727729, 0.867126,
	0.997157, 0.108149, 0.085726, 0.796024, 0.665075, 0.362462, 0.323124,
	0.043718, 0.042357, 0.315030, 0.328954, 0.870845, 0.683186, 0.467922,
	0.514894, 0.809971, 0.631979, 0.176571, 0.366320, 0.850621, 0.505555,
	0.749551, 0.750830, 0.401714, 0.481216, 0.438393, 0.508832, 0.867971,
	0.654581, 0.058204, 0.566454, 0.084124, 0.548539, 0.902690, 0.779571,
	0.562058, 0.048082, 0.863109, 0.079290, 0.713559, 0.783496, 0.265266,
	0.672089, 0.786939, 0.143048, 0.086196, 0.876129, 0.408708, 0.229312,
	0.629995, 0.206665, 0.207308, 0.710079, 0.341704, 0.264921, 0.028748,
	0.629222, 0.470173, 0.726228, 0.125243, 0.328249, 0.794187, 0.741340,
	0.489895, 0.189396, 0.724654, 0.092841, 0.039809, 0.860126, 0.247701,
	0.655331, 0.964121, 0.672536, 0.044522, 0.690567, 0.837238, 0.631520,
	0.953734, 0.352484, 0.289026, 0.034152, 0.852575, 0.098454, 0.795529,
	0.452181, 0.826159, 0.186993, 0.820725, 0.440328, 0.922137, 0.704592,
	0.915437, 0.738183, 0.733461, 0.193798, 0.929213, 0.161390, 0.318547,
	0.888751, 0.430968, 0.740837, 0.193544, 0.872253, 0.563074, 0.274598,
	0.347805, 0.666176, 0.449831, 0.800991, 0.588727, 0.052296, 0.714761,
	0.420620, 0.570325, 0.057550, 0.210888, 0.407312, 0.662848, 0.924382,
	0.895958, 0.775198, 0.688605, 0.025721, 0.301913, 0.791408, 0.500602,
	0.831984, 0.828509, 0.642093, 0.494174, 0.525880, 0.446365, 0.440063,
	0.763114, 0.630358, 0.223943, 0.333806, 0.906033, 0.498306, 0.241278,
	0.427640, 0.772683, 0.198082, 0.225379, 0.503894, 0.436599, 0.016503,
	0.803725, 0.189878, 0.291095, 0.499114, 0.151573, 0.079031, 0.904618,
	0.708535, 0.273900, 0.067419, 0.317124, 0.936499, 0.716511, 0.543845,
	0.939909, 0.826574, 0.715090, 0.154864, 0.750150, 0.845808, 0.648108,
	0.556564, 0.644757, 0.140873, 0.799167, 0.632989, 0.444245, 0.471978,
	0.435910, 0.359793, 0.216241, 0.007633, 0.337236, 0.857863, 0.380247,
	0.092517, 0.799973, 0.919000, 0.296798, 0.096989, 0.854831, 0.165369,
	0.568475, 0.216855, 0.020457, 0.835511, 0.538039, 0.999742, 0.620226,
	0.244053, 0.060399, 0.323007, 0.294874, 0.988899, 0.384919, 0.735655,
	0.773428, 0.549776, 0.292882, 0.660611, 0.593507, 0.621118, 0.175269,
	0.682119, 0.794493, 0.868197, 0.632150, 0.807823, 0.509656, 0.482035,
	0.001780, 0.259126, 0.358002, 0.280263, 0.192985, 0.290367, 0.208111,
	0.917633, 0.114422, 0.925491, 0.981110, 0.255570, 0.974862, 0.016629,
	0.552599, 0.575741, 0.612978, 0.615965, 0.803615, 0.772334, 0.089745,
	0.838812, 0.634542, 0.113709, 0.755832, 0.577589, 0.667489, 0.529834,
	0.325660, 0.817597, 0.316557, 0.335093, 0.737363, 0.260951, 0.737073,
	0.049540, 0.735541, 0.988891, 0.299116, 0.147695, 0.417271, 0.940811,
	0.524160, 0.857968, 0.176403, 0.244835, 0.485759, 0.033353, 0.280319,
	0.750688, 0.755809, 0.924208, 0.095956, 0.962504, 0.275584, 0.173715,
	0.942716, 0.706721, 0.078464, 0.576716, 0.804667, 0.559249, 0.900611,
	0.646904, 0.432111, 0.927885, 0.383277, 0.269973, 0.114244, 0.574867,
	0.150703, 0.241855, 0.272871, 0.199950, 0.079719, 0.868566, 0.962833,
	0.789122, 0.320025, 0.905554, 0.234876, 0.991356, 0.061913, 0.732911,
	0.785960, 0.874074, 0.069035, 0.658632, 0.309901, 0.023676, 0.791603,
	0.764661, 0.661278, 0.319583, 0.829650, 0.117091, 0.903124, 0.982098,
	0.161631, 0.193576, 0.670428, 0.857390, 0.003760, 0.572578, 0.222162,
	0.114551, 0.420118, 0.530404, 0.470682, 0.525527, 0.764281, 0.040596,
	0.443275, 0.501124, 0.816161, 0.417467, 0.332172, 0.447565, 0.614591,
	0.559246, 0.805295, 0.226342, 0.155065, 0.714630, 0.160925, 0.760001,
	0.453456, 0.093869, 0.406092, 0.264801, 0.720370, 0.743388, 0.373269,
	0.403098, 0.911923, 0.897249, 0.147038, 0.753037, 0.516093, 0.739257,
	0.175018, 0.045768, 0.735857, 0.801330, 0.927708, 0.240977, 0.591870,
	0.921831, 0.540733, 0.149100, 0.423152, 0.806876, 0.397081, 0.061100,
	0.811630, 0.044899, 0.460915, 0.961202, 0.822098, 0.971524, 0.867608,
	0.773604, 0.226616, 0.686286, 0.926972, 0.411613, 0.267873, 0.081937,
	0.226124, 0.295664, 0.374594, 0.533240, 0.237876, 0.669629, 0.599083,
	0.513081, 0.878719, 0.201577, 0.721296, 0.495038, 0.079760, 0.965959,
	0.233090, 0.052496, 0.714748, 0.887844, 0.308724, 0.972885, 0.723337,
	0.453089, 0.914474, 0.704063, 0.823198, 0.834769, 0.906561, 0.919600,
	0.100601, 0.307564, 0.901977, 0.468879, 0.265376, 0.885188, 0.683875,
	0.868623, 0.081032, 0.466835, 0.199087, 0.663437, 0.812241, 0.311337,
	0.821361, 0.356628, 0.898054, 0.160781, 0.222539, 0.714889, 0.490287,
	0.984915, 0.951755, 0.964097, 0.641795, 0.815472, 0.852732, 0.862074,
	0.051108, 0.440139, 0.323207, 0.517171, 0.562984, 0.115295, 0.743103,
	0.977914, 0.337596, 0.440694, 0.535879, 0.959427, 0.351427, 0.704361,
	0.010826, 0.131162, 0.577080, 0.349572, 0.774892, 0.425796, 0.072697,
	0.500001, 0.267322, 0.909654, 0.206176, 0.223987, 0.937698, 0.323423,
	0.117501, 0.490308, 0.474372, 0.689943, 0.168671, 0.719417, 0.188928,
	0.330464, 0.265273, 0.446271, 0.171933, 0.176133, 0.474616, 0.140182,
	0.114246, 0.905043, 0.713870, 0.555261, 0.951333
};

const unsigned char hash[512] = {
	0xA2, 0xA0, 0x19, 0x3B, 0xF8, 0xEB, 0xAA, 0xEE, 0xF3, 0x1C, 0x67, 0x28, 0x1D, 0xED, 0x0,  0xDE, 0x95, 0x2E, 0xDC,
	0x3F, 0x3A, 0x82, 0x35, 0x4D, 0x6C, 0xBA, 0x36, 0xD0, 0xF6, 0xC,  0x79, 0x32, 0xD1, 0x59, 0xF4, 0x8,  0x8B, 0x63,
	0x89, 0x2F, 0xB8, 0xB4, 0x97, 0x83, 0xF2, 0x8F, 0x18, 0xC7, 0x51, 0x14, 0x65, 0x87, 0x48, 0x20, 0x42, 0xA8, 0x80,
	0xB5, 0x40, 0x13, 0xB2, 0x22, 0x7E, 0x57, 0xBC, 0x7F, 0x6B, 0x9D, 0x86, 0x4C, 0xC8, 0xDB, 0x7C, 0xD5, 0x25, 0x4E,
	0x5A, 0x55, 0x74, 0x50, 0xCD, 0xB3, 0x7A, 0xBB, 0xC3, 0xCB, 0xB6, 0xE2, 0xE4, 0xEC, 0xFD, 0x98, 0xB,  0x96, 0xD3,
	0x9E, 0x5C, 0xA1, 0x64, 0xF1, 0x81, 0x61, 0xE1, 0xC4, 0x24, 0x72, 0x49, 0x8C, 0x90, 0x4B, 0x84, 0x34, 0x38, 0xAB,
	0x78, 0xCA, 0x1F, 0x1,  0xD7, 0x93, 0x11, 0xC1, 0x58, 0xA9, 0x31, 0xF9, 0x44, 0x6D, 0xBF, 0x33, 0x9C, 0x5F, 0x9,
	0x94, 0xA3, 0x85, 0x6,  0xC6, 0x9A, 0x1E, 0x7B, 0x46, 0x15, 0x30, 0x27, 0x2B, 0x1B, 0x71, 0x3C, 0x5B, 0xD6, 0x6F,
	0x62, 0xAC, 0x4F, 0xC2, 0xC0, 0xE,  0xB1, 0x23, 0xA7, 0xDF, 0x47, 0xB0, 0x77, 0x69, 0x5,  0xE9, 0xE6, 0xE7, 0x76,
	0x73, 0xF,  0xFE, 0x6E, 0x9B, 0x56, 0xEF, 0x12, 0xA5, 0x37, 0xFC, 0xAE, 0xD9, 0x3,  0x8E, 0xDD, 0x10, 0xB9, 0xCE,
	0xC9, 0x8D, 0xDA, 0x2A, 0xBD, 0x68, 0x17, 0x9F, 0xBE, 0xD4, 0xA,  0xCC, 0xD2, 0xE8, 0x43, 0x3D, 0x70, 0xB7, 0x2,
	0x7D, 0x99, 0xD8, 0xD,  0x60, 0x8A, 0x4,  0x2C, 0x3E, 0x92, 0xE5, 0xAF, 0x53, 0x7,  0xE0, 0x29, 0xA6, 0xC5, 0xE3,
	0xF5, 0xF7, 0x4A, 0x41, 0x26, 0x6A, 0x16, 0x5E, 0x52, 0x2D, 0x21, 0xAD, 0xF0, 0x91, 0xFF, 0xEA, 0x54, 0xFA, 0x66,
	0x1A, 0x45, 0x39, 0xCF, 0x75, 0xA4, 0x88, 0xFB, 0x5D, 0xA2, 0xA0, 0x19, 0x3B, 0xF8, 0xEB, 0xAA, 0xEE, 0xF3, 0x1C,
	0x67, 0x28, 0x1D, 0xED, 0x0,  0xDE, 0x95, 0x2E, 0xDC, 0x3F, 0x3A, 0x82, 0x35, 0x4D, 0x6C, 0xBA, 0x36, 0xD0, 0xF6,
	0xC,  0x79, 0x32, 0xD1, 0x59, 0xF4, 0x8,  0x8B, 0x63, 0x89, 0x2F, 0xB8, 0xB4, 0x97, 0x83, 0xF2, 0x8F, 0x18, 0xC7,
	0x51, 0x14, 0x65, 0x87, 0x48, 0x20, 0x42, 0xA8, 0x80, 0xB5, 0x40, 0x13, 0xB2, 0x22, 0x7E, 0x57, 0xBC, 0x7F, 0x6B,
	0x9D, 0x86, 0x4C, 0xC8, 0xDB, 0x7C, 0xD5, 0x25, 0x4E, 0x5A, 0x55, 0x74, 0x50, 0xCD, 0xB3, 0x7A, 0xBB, 0xC3, 0xCB,
	0xB6, 0xE2, 0xE4, 0xEC, 0xFD, 0x98, 0xB,  0x96, 0xD3, 0x9E, 0x5C, 0xA1, 0x64, 0xF1, 0x81, 0x61, 0xE1, 0xC4, 0x24,
	0x72, 0x49, 0x8C, 0x90, 0x4B, 0x84, 0x34, 0x38, 0xAB, 0x78, 0xCA, 0x1F, 0x1,  0xD7, 0x93, 0x11, 0xC1, 0x58, 0xA9,
	0x31, 0xF9, 0x44, 0x6D, 0xBF, 0x33, 0x9C, 0x5F, 0x9,  0x94, 0xA3, 0x85, 0x6,  0xC6, 0x9A, 0x1E, 0x7B, 0x46, 0x15,
	0x30, 0x27, 0x2B, 0x1B, 0x71, 0x3C, 0x5B, 0xD6, 0x6F, 0x62, 0xAC, 0x4F, 0xC2, 0xC0, 0xE,  0xB1, 0x23, 0xA7, 0xDF,
	0x47, 0xB0, 0x77, 0x69, 0x5,  0xE9, 0xE6, 0xE7, 0x76, 0x73, 0xF,  0xFE, 0x6E, 0x9B, 0x56, 0xEF, 0x12, 0xA5, 0x37,
	0xFC, 0xAE, 0xD9, 0x3,  0x8E, 0xDD, 0x10, 0xB9, 0xCE, 0xC9, 0x8D, 0xDA, 0x2A, 0xBD, 0x68, 0x17, 0x9F, 0xBE, 0xD4,
	0xA,  0xCC, 0xD2, 0xE8, 0x43, 0x3D, 0x70, 0xB7, 0x2,  0x7D, 0x99, 0xD8, 0xD,  0x60, 0x8A, 0x4,  0x2C, 0x3E, 0x92,
	0xE5, 0xAF, 0x53, 0x7,  0xE0, 0x29, 0xA6, 0xC5, 0xE3, 0xF5, 0xF7, 0x4A, 0x41, 0x26, 0x6A, 0x16, 0x5E, 0x52, 0x2D,
	0x21, 0xAD, 0xF0, 0x91, 0xFF, 0xEA, 0x54, 0xFA, 0x66, 0x1A, 0x45, 0x39, 0xCF, 0x75, 0xA4, 0x88, 0xFB, 0x5D,
};


const float hashvectf[768] = {
	0.33783, 0.715698, -0.611206, -0.944031, -0.326599, -0.045624, -0.101074, -0.416443, -0.903503, 0.799286, 0.49411,
	-0.341949, -0.854645, 0.518036, 0.033936, 0.42514, -0.437866, -0.792114, -0.358948, 0.597046, 0.717377, -0.985413,
	0.144714, 0.089294, -0.601776, -0.33728, -0.723907, -0.449921, 0.594513, 0.666382, 0.208313, -0.10791, 0.972076,
	0.575317, 0.060425, 0.815643, 0.293365, -0.875702, -0.383453, 0.293762, 0.465759, 0.834686, -0.846008, -0.233398,
	-0.47934, -0.115814, 0.143036, -0.98291, 0.204681, -0.949036, -0.239532, 0.946716, -0.263947, 0.184326, -0.235596,
	0.573822, 0.784332, 0.203705, -0.372253, -0.905487, 0.756989, -0.651031, 0.055298, 0.497803, 0.814697, -0.297363,
	-0.16214, 0.063995, -0.98468, -0.329254, 0.834381, 0.441925, 0.703827, -0.527039, -0.476227, 0.956421, 0.266113,
	0.119781, 0.480133, 0.482849, 0.7323, -0.18631, 0.961212, -0.203125, -0.748474, -0.656921, -0.090393, -0.085052,
	-0.165253, 0.982544, -0.76947, 0.628174, -0.115234, 0.383148, 0.537659, 0.751068, 0.616486, -0.668488, -0.415924,
	-0.259979, -0.630005, 0.73175, 0.570953, -0.087952, 0.816223, -0.458008, 0.023254, 0.888611, -0.196167, 0.976563,
	-0.088287, -0.263885, -0.69812, -0.665527, 0.437134, -0.892273, -0.112793, -0.621674, -0.230438, 0.748566, 0.232422,
	0.900574, -0.367249, 0.22229, -0.796143, 0.562744, -0.665497, -0.73764, 0.11377, 0.670135, 0.704803, 0.232605,
	0.895599, 0.429749, -0.114655, -0.11557, -0.474243, 0.872742, 0.621826, 0.604004, -0.498444, -0.832214, 0.012756,
	0.55426, -0.702484, 0.705994, -0.089661, -0.692017, 0.649292, 0.315399, -0.175995, -0.977997, 0.111877, 0.096954,
	-0.04953, 0.994019, 0.635284, -0.606689, -0.477783, -0.261261, -0.607422, -0.750153, 0.983276, 0.165436, 0.075958,
	-0.29837, 0.404083, -0.864655, -0.638672, 0.507721, 0.578156, 0.388214, 0.412079, 0.824249, 0.556183, -0.208832,
	0.804352, 0.778442, 0.562012, 0.27951, -0.616577, 0.781921, -0.091522, 0.196289, 0.051056, 0.979187, -0.121216,
	0.207153, -0.970734, -0.173401, -0.384735, 0.906555, 0.161499, -0.723236, -0.671387, 0.178497, -0.006226, -0.983887,
	-0.126038, 0.15799, 0.97934, 0.830475, -0.024811, 0.556458, -0.510132, -0.76944, 0.384247, 0.81424, 0.200104,
	-0.544891, -0.112549, -0.393311, -0.912445, 0.56189, 0.152222, -0.813049, 0.198914, -0.254517, -0.946381, -0.41217,
	0.690979, -0.593811, -0.407257, 0.324524, 0.853668, -0.690186, 0.366119, -0.624115, -0.428345, 0.844147, -0.322296,
	-0.21228, -0.297546, -0.930756, -0.273071, 0.516113, 0.811798, 0.928314, 0.371643, 0.007233, 0.785828, -0.479218,
	-0.390778, -0.704895, 0.058929, 0.706818, 0.173248, 0.203583, 0.963562, 0.422211, -0.904297, -0.062469, -0.363312,
	-0.182465, 0.913605, 0.254028, -0.552307, -0.793945, -0.28891, -0.765747, -0.574554, 0.058319, 0.291382, 0.954803,
	0.946136, -0.303925, 0.111267, -0.078156, 0.443695, -0.892731, 0.182098, 0.89389, 0.409515, -0.680298, -0.213318,
	0.701141, 0.062469, 0.848389, -0.525635, -0.72879, -0.641846, 0.238342, -0.88089, 0.427673, 0.202637, -0.532501,
	-0.21405, 0.818878, 0.948975, -0.305084, 0.07962, 0.925446, 0.374664, 0.055817, 0.820923, 0.565491, 0.079102,
	0.25882, 0.099792, -0.960724, -0.294617, 0.910522, 0.289978, 0.137115, 0.320038, -0.937408, -0.908386, 0.345276,
	-0.235718, -0.936218, 0.138763, 0.322754, 0.366577, 0.925934, -0.090637, 0.309296, -0.686829, -0.657684, 0.66983,
	0.024445, 0.742065, -0.917999, -0.059113, -0.392059, 0.365509, 0.462158, -0.807922, 0.083374, 0.996399, -0.014801,
	0.593842, 0.253143, -0.763672, 0.974976, -0.165466, 0.148285, 0.918976, 0.137299, 0.369537, 0.294952, 0.694977,
	0.655731, 0.943085, 0.152618, -0.295319, 0.58783, -0.598236, 0.544495, 0.203796, 0.678223, 0.705994, -0.478821,
	-0.661011, 0.577667, 0.719055, -0.1698, -0.673828, -0.132172, -0.965332, 0.225006, -0.981873, -0.14502, 0.121979,
	0.763458, 0.579742, 0.284546, -0.893188, 0.079681, 0.442474, -0.795776, -0.523804, 0.303802, 0.734955, 0.67804,
	-0.007446, 0.15506, 0.986267, -0.056183, 0.258026, 0.571503, -0.778931, -0.681549, -0.702087, -0.206116, -0.96286,
	-0.177185, 0.203613, -0.470978, -0.515106, 0.716095, -0.740326, 0.57135, 0.354095, -0.56012, -0.824982, -0.074982,
	-0.507874, 0.753204, 0.417969, -0.503113, 0.038147, 0.863342, 0.594025, 0.673553, -0.439758, -0.119873, -0.005524,
	-0.992737, 0.098267, -0.213776, 0.971893, -0.615631, 0.643951, 0.454163, 0.896851, -0.441071, 0.032166, -0.555023,
	0.750763, -0.358093, 0.398773, 0.304688, 0.864929, -0.722961, 0.303589, 0.620544, -0.63559, -0.621948, -0.457306,
	-0.293243, 0.072327, 0.953278, -0.491638, 0.661041, -0.566772, -0.304199, -0.572083, -0.761688, 0.908081, -0.398956,
	0.127014, -0.523621, -0.549683, -0.650848, -0.932922, -0.19986, 0.299408, 0.099426, 0.140869, 0.984985, -0.020325,
	-0.999756, -0.002319, 0.952667, 0.280853, -0.11615, -0.971893, 0.082581, 0.220337, 0.65921, 0.705292, -0.260651,
	0.733063, -0.175537, 0.657043, -0.555206, 0.429504, -0.712189, 0.400421, -0.89859, 0.179352, 0.750885, -0.19696,
	0.630341, 0.785675, -0.569336, 0.241821, -0.058899, -0.464111, 0.883789, 0.129608, -0.94519, 0.299622, -0.357819,
	0.907654, 0.219238, -0.842133, -0.439117, -0.312927, -0.313477, 0.84433, 0.434479, -0.241211, 0.053253, 0.968994,
	0.063873, 0.823273, 0.563965, 0.476288, 0.862152, -0.172516, 0.620941, -0.298126, 0.724915, 0.25238, -0.749359,
	-0.612122, -0.577545, 0.386566, 0.718994, -0.406342, -0.737976, 0.538696, 0.04718, 0.556305, 0.82959, -0.802856,
	0.587463, 0.101166, -0.707733, -0.705963, 0.026428, 0.374908, 0.68457, 0.625092, 0.472137, 0.208405, -0.856506,
	-0.703064, -0.581085, -0.409821, -0.417206, -0.736328, 0.532623, -0.447876, -0.20285, -0.870728, 0.086945,
	-0.990417, 0.107086, 0.183685, 0.018341, -0.982788, 0.560638, -0.428864, 0.708282, 0.296722, -0.952576, -0.0672,
	0.135773, 0.990265, 0.030243, -0.068787, 0.654724, 0.752686, 0.762604, -0.551758, 0.337585, -0.819611, -0.407684,
	0.402466, -0.727844, -0.55072, -0.408539, -0.855774, -0.480011, 0.19281, 0.693176, -0.079285, 0.716339, 0.226013,
	0.650116, -0.725433, 0.246704, 0.953369, -0.173553, -0.970398, -0.239227, -0.03244, 0.136383, -0.394318, 0.908752,
	0.813232, 0.558167, 0.164368, 0.40451, 0.549042, -0.731323, -0.380249, -0.566711, 0.730865, 0.022156, 0.932739,
	0.359741, 0.00824, 0.996552, -0.082306, 0.956635, -0.065338, -0.283722, -0.743561, 0.008209, 0.668579, -0.859589,
	-0.509674, 0.035767, -0.852234, 0.363678, -0.375977, -0.201965, -0.970795, -0.12915, 0.313477, 0.947327, 0.06546,
	-0.254028, -0.528259, 0.81015, 0.628052, 0.601105, 0.49411, -0.494385, 0.868378, 0.037933, 0.275635, -0.086426,
	0.957336, -0.197937, 0.468903, -0.860748, 0.895599, 0.399384, 0.195801, 0.560791, 0.825012, -0.069214, 0.304199,
	-0.849487, 0.43103, 0.096375, 0.93576, 0.339111, -0.051422, 0.408966, -0.911072, 0.330444, 0.942841, -0.042389,
	-0.452362, -0.786407, 0.420563, 0.134308, -0.933472, -0.332489, 0.80191, -0.566711, -0.188934, -0.987946, -0.105988,
	0.112518, -0.24408, 0.892242, -0.379791, -0.920502, 0.229095, -0.316376, 0.7789, 0.325958, 0.535706, -0.912872,
	0.185211, -0.36377, -0.184784, 0.565369, -0.803833, -0.018463, 0.119537, 0.992615, -0.259247, -0.935608, 0.239532,
	-0.82373, -0.449127, -0.345947, -0.433105, 0.659515, 0.614349, -0.822754, 0.378845, -0.423676, 0.687195, -0.674835,
	-0.26889, -0.246582, -0.800842, 0.545715, -0.729187, -0.207794, 0.651978, 0.653534, -0.610443, -0.447388, 0.492584,
	-0.023346, 0.869934, 0.609039, 0.009094, -0.79306, 0.962494, -0.271088, -0.00885, 0.2659, -0.004913, 0.963959,
	0.651245, 0.553619, -0.518951, 0.280548, -0.84314, 0.458618, -0.175293, -0.983215, 0.049805, 0.035339, -0.979919,
	0.196045, -0.982941, 0.164307, -0.082245, 0.233734, -0.97226, -0.005005, -0.747253, -0.611328, 0.260437, 0.645599,
	0.592773, 0.481384, 0.117706, -0.949524, -0.29068, -0.535004, -0.791901, -0.294312, -0.627167, -0.214447, 0.748718,
	-0.047974, -0.813477, -0.57959, -0.175537, 0.477264, -0.860992, 0.738556, -0.414246, -0.53183, 0.562561, -0.704071,
	0.433289, -0.754944, 0.64801, -0.100586, 0.114716, 0.044525, -0.992371, 0.966003, 0.244873, -0.082764,
};

/**************************/
/*  IMPROVED PERLIN NOISE */
/**************************/

static float lerp(float t, float a, float b)
{
	return (a + t * (b - a));
}

static float npfade(float t)
{
	return (t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f));
}

static float grad(int hash_val, float x, float y, float z)
{
	int h = hash_val & 15;                 /* CONVERT LO 4 BITS OF HASH CODE */
	float u = h < 8 ? x : y;               /* INTO 12 GRADIENT DIRECTIONS. */
	float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
	return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

/* instead of adding another permutation array, just use hash table defined above */
static float newPerlin(float x, float y, float z)
{
	int A, AA, AB, B, BA, BB;
	float u = floor(x), v = floor(y), w = floor(z);
	int X = ((int)u) & 255, Y = ((int)v) & 255, Z = ((int)w) & 255;   /* FIND UNIT CUBE THAT CONTAINS POINT */
	x -= u;             /* FIND RELATIVE X,Y,Z */
	y -= v;             /* OF POINT IN CUBE. */
	z -= w;
	u = npfade(x);      /* COMPUTE FADE CURVES */
	v = npfade(y);      /* FOR EACH OF X,Y,Z. */
	w = npfade(z);
	A = hash[X    ] + Y;  AA = hash[A] + Z;  AB = hash[A + 1] + Z;      /* HASH COORDINATES OF */
	B = hash[X + 1] + Y;  BA = hash[B] + Z;  BB = hash[B + 1] + Z;      /* THE 8 CUBE CORNERS, */
	return lerp(w, lerp(v, lerp(u, grad(hash[AA   ],  x,     y,     z    ),   /* AND ADD */
	                               grad(hash[BA   ],  x - 1, y,     z    )),  /* BLENDED */
	                       lerp(u, grad(hash[AB   ],  x,     y - 1, z    ),   /* RESULTS */
	                               grad(hash[BB   ],  x - 1, y - 1, z    ))), /* FROM  8 */
	               lerp(v, lerp(u, grad(hash[AA + 1], x,     y,     z - 1),   /* CORNERS */
	                               grad(hash[BA + 1], x - 1, y,     z - 1)),  /* OF CUBE */
	                       lerp(u, grad(hash[AB + 1], x,     y - 1, z - 1),
	                               grad(hash[BB + 1], x - 1, y - 1, z - 1))));
}

/* for use with BLI_gNoise()/BLI_gTurbulence(), returns unsigned improved perlin noise */
static float newPerlinU(float x, float y, float z)
{
	return (0.5f + 0.5f * newPerlin(x, y, z));
}


/**************************/
/* END OF IMPROVED PERLIN */
/**************************/

/* Was BLI_hnoise(), removed noisesize, so other functions can call it without scaling. */
static float orgBlenderNoise(float x, float y, float z)
{
	register float cn1, cn2, cn3, cn4, cn5, cn6, i;
	register const float *h;
	float fx, fy, fz, ox, oy, oz, jx, jy, jz;
	float n = 0.5;
	int ix, iy, iz, b00, b01, b10, b11, b20, b21;

	fx = floor(x);
	fy = floor(y);
	fz = floor(z);

	ox = x - fx;
	oy = y - fy;
	oz = z - fz;

	ix = (int)fx;
	iy = (int)fy;
	iz = (int)fz;

	jx = ox - 1;
	jy = oy - 1;
	jz = oz - 1;

	cn1 = ox * ox; cn2 = oy * oy; cn3 = oz * oz;
	cn4 = jx * jx; cn5 = jy * jy; cn6 = jz * jz;

	cn1 = 1.0f - 3.0f * cn1 + 2.0f * cn1 * ox;
	cn2 = 1.0f - 3.0f * cn2 + 2.0f * cn2 * oy;
	cn3 = 1.0f - 3.0f * cn3 + 2.0f * cn3 * oz;
	cn4 = 1.0f - 3.0f * cn4 - 2.0f * cn4 * jx;
	cn5 = 1.0f - 3.0f * cn5 - 2.0f * cn5 * jy;
	cn6 = 1.0f - 3.0f * cn6 - 2.0f * cn6 * jz;

	b00 = hash[hash[ix & 255] + (iy & 255)];
	b10 = hash[hash[(ix + 1) & 255] + (iy & 255)];
	b01 = hash[hash[ix & 255] + ((iy + 1) & 255)];
	b11 = hash[hash[(ix + 1) & 255] + ((iy + 1) & 255)];

	b20 = iz & 255; b21 = (iz + 1) & 255;

	/* 0 */
	i = (cn1 * cn2 * cn3);
	h = hashvectf + 3 * hash[b20 + b00];
	n += i * (h[0] * ox + h[1] * oy + h[2] * oz);
	/* 1 */
	i = (cn1 * cn2 * cn6);
	h = hashvectf + 3 * hash[b21 + b00];
	n += i * (h[0] * ox + h[1] * oy + h[2] * jz);
	/* 2 */
	i = (cn1 * cn5 * cn3);
	h = hashvectf + 3 * hash[b20 + b01];
	n += i * (h[0] * ox + h[1] * jy + h[2] * oz);
	/* 3 */
	i = (cn1 * cn5 * cn6);
	h = hashvectf + 3 * hash[b21 + b01];
	n += i * (h[0] * ox + h[1] * jy + h[2] * jz);
	/* 4 */
	i = cn4 * cn2 * cn3;
	h = hashvectf + 3 * hash[b20 + b10];
	n += i * (h[0] * jx + h[1] * oy + h[2] * oz);
	/* 5 */
	i = cn4 * cn2 * cn6;
	h = hashvectf + 3 * hash[b21 + b10];
	n += i * (h[0] * jx + h[1] * oy + h[2] * jz);
	/* 6 */
	i = cn4 * cn5 * cn3;
	h = hashvectf + 3 * hash[b20 + b11];
	n +=  i * (h[0] * jx + h[1] * jy + h[2] * oz);
	/* 7 */
	i = (cn4 * cn5 * cn6);
	h = hashvectf + 3 * hash[b21 + b11];
	n += i * (h[0] * jx + h[1] * jy + h[2] * jz);

	if      (n < 0.0f) n = 0.0f;
	else if (n > 1.0f) n = 1.0f;
	return n;
}

/* as orgBlenderNoise(), returning signed noise */
static float orgBlenderNoiseS(float x, float y, float z)
{
	return (2.0f * orgBlenderNoise(x, y, z) - 1.0f);
}

/* separated from orgBlenderNoise above, with scaling */
float BLI_hnoise(float noisesize, float x, float y, float z)
{
	if (noisesize == 0.0f) return 0.0f;
	x = (1.0f + x) / noisesize;
	y = (1.0f + y) / noisesize;
	z = (1.0f + z) / noisesize;
	return orgBlenderNoise(x, y, z);
}


/* original turbulence functions */
float BLI_turbulence(float noisesize, float x, float y, float z, int nr)
{
	float s, d = 0.5, div = 1.0;

	s = BLI_hnoise(noisesize, x, y, z);
	
	while (nr > 0) {
	
		s += d * BLI_hnoise(noisesize * d, x, y, z);
		div += d;
		d *= 0.5f;

		nr--;
	}
	return s / div;
}

float BLI_turbulence1(float noisesize, float x, float y, float z, int nr)
{
	float s, d = 0.5, div = 1.0;

	s = fabsf((-1.0f + 2.0f * BLI_hnoise(noisesize, x, y, z)));
	
	while (nr > 0) {
	
		s += fabsf(d * (-1.0f + 2.0f * BLI_hnoise(noisesize * d, x, y, z)));
		div += d;
		d *= 0.5f;
		
		nr--;
	}
	return s / div;
}

// Gradient table for 2D. These could be programmed the Ken Perlin way with
// some clever bit-twiddling, but this is more clear, and not really slower.
static float grad2lut[8][2] = {
    { -1.0f, -1.0f }, { 1.0f,  0.0f }, { -1.0f, 0.0f }, { 1.0f,  1.0f },
    { -1.0f,  1.0f }, { 0.0f, -1.0f }, {  0.0f, 1.0f }, { 1.0f, -1.0f }
};

// Gradient directions for 3D.
// These vectors are based on the midpoints of the 12 edges of a cube.
// A larger array of random unit length vectors would also do the job,
// but these 12 (including 4 repeats to make the array length a power
// of two) work better. They are not random, they are carefully chosen
// to represent a small, isotropic set of directions.
static float grad3lut[16][3] = {
    {  1.0f,  0.0f,  1.0f }, {  0.0f,  1.0f,  1.0f }, // 12 cube edges
    { -1.0f,  0.0f,  1.0f }, {  0.0f, -1.0f,  1.0f },
    {  1.0f,  0.0f, -1.0f }, {  0.0f,  1.0f, -1.0f },
    { -1.0f,  0.0f, -1.0f }, {  0.0f, -1.0f, -1.0f },
    {  1.0f, -1.0f,  0.0f }, {  1.0f,  1.0f,  0.0f },
    { -1.0f,  1.0f,  0.0f }, { -1.0f, -1.0f,  0.0f },
    {  1.0f,  0.0f,  1.0f }, { -1.0f,  0.0f,  1.0f }, // 4 repeats to make 16
    {  0.0f,  1.0f, -1.0f }, {  0.0f, -1.0f, -1.0f }
};

// Gradient directions for 4D
static float grad4lut[32][4] = {
  { 0.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, -1.0f, -1.0f }, // 32 tesseract edges
  { 0.0f, -1.0f, 1.0f, 1.0f }, { 0.0f, -1.0f, 1.0f, -1.0f }, { 0.0f, -1.0f, -1.0f, 1.0f }, { 0.0f, -1.0f, -1.0f, -1.0f },
  { 1.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, -1.0f }, { 1.0f, 0.0f, -1.0f, 1.0f }, { 1.0f, 0.0f, -1.0f, -1.0f },
  { -1.0f, 0.0f, 1.0f, 1.0f }, { -1.0f, 0.0f, 1.0f, -1.0f }, { -1.0f, 0.0f, -1.0f, 1.0f }, { -1.0f, 0.0f, -1.0f, -1.0f },
  { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, -1.0f }, { 1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, -1.0f, 0.0f, -1.0f },
  { -1.0f, 1.0f, 0.0f, 1.0f }, { -1.0f, 1.0f, 0.0f, -1.0f }, { -1.0f, -1.0f, 0.0f, 1.0f }, { -1.0f, -1.0f, 0.0f, -1.0f },
  { 1.0f, 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, -1.0f, 0.0f },
  { -1.0f, 1.0f, 1.0f, 0.0f }, { -1.0f, 1.0f, -1.0f, 0.0f }, { -1.0f, -1.0f, 1.0f, 0.0f }, { -1.0f, -1.0f, -1.0f, 0.0f }
};

/* Bitwise circular rotation left by k bits. */
BLI_INLINE unsigned int rotl32(unsigned int x, int k)
{
	return (x<<k) | (x>>(32-k));
}

// Mix up and combine the bits of a, b, and c (doesn't change them, but
// returns a hash of those three original values).  21 ops
BLI_INLINE unsigned int bjfinal3(unsigned int a, unsigned int b, unsigned int c)
{
	c ^= b; c -= rotl32(b,14);
	a ^= c; a -= rotl32(c,11);
	b ^= a; b -= rotl32(a,25);
	c ^= b; c -= rotl32(b,16);
	a ^= c; a -= rotl32(c,4);
	b ^= a; b -= rotl32(a,14);
	c ^= b; c -= rotl32(b,24);
	return c;
}

BLI_INLINE unsigned int bjfinal2(unsigned int a, unsigned int b)
{
	return bjfinal3(a, b, 0xdeadbeef);
}

BLI_INLINE unsigned int scramble3(unsigned int v0, unsigned int v1, unsigned int v2)
{
	return bjfinal3(v0, v1, v2^0xdeadbeef);
}

BLI_INLINE unsigned int scramble2(unsigned int v0, unsigned int v1)
{
	return scramble3(v0, v1, 0);
}

BLI_INLINE unsigned int scramble1(unsigned int v0)
{
	return scramble3(v0, 0, 0);
}

/*
 * Helper functions to compute gradients in 1D to 4D
 * and gradients-dot-residualvectors in 2D to 4D.
 */

BLI_INLINE float grad1(int i, int seed)
{
	int h = scramble2(i, seed);
	float g = 1.0f + (h & 7);   // Gradient value is one of 1.0, 2.0, ..., 8.0
	if (h & 8)
		g = -g;   // Make half of the gradients negative
	return g;
}

BLI_INLINE const float *grad2 (int i, int j, int seed)
{
	int h = scramble3(i, j, seed);
	return grad2lut[h & 7];
}

BLI_INLINE const float *grad3(int i, int j, int k, int seed)
{
	int h = scramble3(i, j, scramble2(k, seed));
	return grad3lut[h & 15];
}

BLI_INLINE const float *grad4(int i, int j, int k, int l, int seed)
{
	int h = scramble3(i, j, scramble3(k, l, seed));
	return grad4lut[h & 31];
}

BLI_INLINE int quick_floor(float x)
{
	return (int)x - ((x < 0) ? 1 : 0);
}

BLI_INLINE int order_components4(float x, float y, float z, float w)
{
	int c1 = (x > y) ? 32 : 0;
	int c2 = (x > z) ? 16 : 0;
	int c3 = (y > z) ? 8 : 0;
	int c4 = (x > w) ? 4 : 0;
	int c5 = (y > w) ? 2 : 0;
	int c6 = (z > w) ? 1 : 0;
	return c1 | c2 | c3 | c4 | c5 | c6; // '|' is mostly faster than '+'
}

#if 0
#ifdef SIMPLEXNOISE_USE_SSE2
BLI_INLINE float get_elemf_sse(const __m128 a, int index)
{
	return ((const float *)(&a))[3-index];
}

BLI_INLINE int get_elemi_sse(const __m128i a, int index)
{
	return ((const int *)(&a))[3-index];
}

/* XXX horizontal-add intrinsics replacement for SSE2 (hadd needs SSE3) */
BLI_INLINE __m128 _mm_hadd4_ps(const __m128 i)
{
	__m128 s;
	s = _mm_add_ps(i, _mm_movehl_ps(i, i));
	s = _mm_add_ps(s, _mm_shuffle_ps(s, s, 1));
	return s;
}

/* XXX horizontal-add intrinsics replacement for SSE2 (hadd needs SSE3) */
BLI_INLINE __m128i _mm_hadd4_epi32(const __m128i i)
{
	// XXX how to do this with intrinsics?
	const int32_t *pi = (const int32_t *)(&i);
	return _mm_set_epi32(pi[0], pi[1], pi[2], pi[3]);
}

BLI_INLINE const __m128 cast_to_float_sse(const __m128i a)
{
	return _mm_castsi128_ps(a);
}

BLI_INLINE const __m128i cast_to_int_sse(const __m128 a)
{
	return _mm_castps_si128(a);
}

BLI_INLINE __m128i truncatei_sse(const __m128 a) {
	return _mm_cvttps_epi32(a);
}

BLI_INLINE __m128i quick_floor_sse(const __m128 x)
{
//	__m128i b = truncatei_sse(x);
//	__m128i isneg = cast_to_int_sse(_mm_cmplt_ps(x, _mm_set1_ps(0.0f)));
//	return _mm_add_epi32(b, isneg); // unsaturated add 0xffffffff is the same as subtract -1
	const float *pf = (const float *)(&x);
	return _mm_set_epi32(quick_floor(pf[3]), quick_floor(pf[2]), quick_floor(pf[1]), quick_floor(pf[0]));
}
#endif

// 3D simplex noise with derivatives.
// If the last tthree arguments are not null, the analytic derivative
// (the 3D gradient of the scalar noise field) is also calculated.
BLI_INLINE float simplexnoise3(float x, float y, float z, int seed)
{
	// Skewing factors for 3D simplex grid:
	const float F3 = 0.333333333f;   // = 1/3
	const float G3 = 0.166666667f;   // = 1/6
	
	// Skew the input space to determine which simplex cell we're in
	float s = (x + y + z) * F3; // Very nice and simple skew factor for 3D
#ifdef SIMPLEXNOISE_USE_SSE2
	__m128 xyz = _mm_set_ps(x, y, z, 0.0f);
	__m128i ijk = quick_floor_sse(_mm_add_ps(xyz, _mm_set_ps(s, s, s, 0.0f)));
	int i = get_elemi_sse(ijk, 0);
	int j = get_elemi_sse(ijk, 1);
	int k = get_elemi_sse(ijk, 2);
	
	float t = (float)get_elemi_sse(_mm_hadd4_epi32(ijk), 0) * G3;
	__m128 XYZ0 = cast_to_float_sse(ijk) - _mm_set_ps(t, t, t, 0.0f);  // Unskew the cell origin back to (x,y,z) space
	__m128 xyz0 = _mm_sub_ps(xyz, XYZ0);  // The x,y,z distances from the cell origin
#else
	int i = quick_floor(x + s);
	int j = quick_floor(y + s);
	int k = quick_floor(z + s);
	
	float t = (float)(i + j + k) * G3;
	float XYZ0[3] = { (float)i - t, (float)j - t, (float)k - t };  // Unskew the cell origin back to (x,y,z) space
	float xyz0[3] = { x - XYZ0[0], y - XYZ0[1], z - XYZ0[2] };  // The x,y,z distances from the cell origin
#endif
	
	// For the 3D case, the simplex shape is a slightly irregular tetrahedron.
	// Determine which simplex we are in.
	int i1, j1, k1; // Offsets for second corner of simplex in (i,j,k) coords
	int i2, j2, k2; // Offsets for third corner of simplex in (i,j,k) coords
	
	/* COMPAT */
#ifdef SIMPLEXNOISE_USE_SSE2
	float x0 = get_elemf_sse(xyz0, 0), y0 = get_elemf_sse(xyz0, 1), z0 = get_elemf_sse(xyz0, 2);
#else
	float x0 = xyz0[0], y0 = xyz0[1], z0 = xyz0[2];
#endif
	float x1, x2, x3, y1, y2, y3, z1, z2, z3;
	
#if 1
    // TODO: This code would benefit from a backport from the GLSL version!
    // (no it can't... see note below)
    if (x0>=y0) {
        if (y0>=z0) {
            i1=1; j1=0; k1=0; i2=1; j2=1; k2=0;  /* X Y Z order */
        } else if (x0>=z0) {
            i1=1; j1=0; k1=0; i2=1; j2=0; k2=1;  /* X Z Y order */
        } else {
            i1=0; j1=0; k1=1; i2=1; j2=0; k2=1;  /* Z X Y order */
        }
    } else { // x0<y0
        if (y0<z0) {
            i1=0; j1=0; k1=1; i2=0; j2=1; k2=1;  /* Z Y X order */
        } else if (x0<z0) {
            i1=0; j1=1; k1=0; i2=0; j2=1; k2=1;  /* Y Z X order */
        } else {
            i1=0; j1=1; k1=0; i2=1; j2=1; k2=0;  /* Y X Z order */
        }
    }
#else
    // Here's the logic "from the GLSL version", near as I (LG) could
    // translate it from GLSL to non-SIMD C++.  It was slower.  I'm
    // keeping this code here for reference anyway.
    {
        // vec3 g = step(x0.yzx, x0.xyz);
        // vec3 l = 1.0 - g;
        bool g0 = (x0 >= y0), l0 = !g0;
        bool g1 = (y0 >= z0), l1 = !g1;
        bool g2 = (z0 >= x0), l2 = !g2;
        // vec3 i1 = min (g.xyz, l.zxy);  // min of bools is &
        // vec3 i2 = max (g.xyz, l.zxy);  // max of bools is |
        i1 = g0 & l2;
        j1 = g1 & l0;
        k1 = g2 & l1;
        i2 = g0 | l2;
        j2 = g1 | l0;
        k2 = g2 | l1;
    }
#endif
	
	{
#ifdef SIMPLEXNOISE_USE_SSE2
		__m128i ijk1 = _mm_set_epi32(i1, j1, k1, 0);
		__m128i ijk2 = _mm_set_epi32(i2, j2, k2, 0);
		
		// A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
		// a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
		// a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z),
		// where c = 1/6.
		__m128 xyz1 = _mm_add_ps(_mm_sub_ps(xyz0, cast_to_float_sse(ijk1)), _mm_set1_ps(G3));  // Offsets for second corner in (x,y,z) coords
		__m128 xyz2 = _mm_add_ps(_mm_sub_ps(xyz0, cast_to_float_sse(ijk2)), _mm_set1_ps(2.0f * G3));  // Offsets for third corner in (x,y,z) coords
		__m128 xyz3 =            _mm_sub_ps(xyz0, _mm_set1_ps(1.0f - 3.0f * G3));  // Offsets for last corner in (x,y,z) coords
		
		/* Compat */
		x1 = get_elemf_sse(xyz1, 0); y1 = get_elemf_sse(xyz1, 1); z1 = get_elemf_sse(xyz1, 2);
		x2 = get_elemf_sse(xyz2, 0); y2 = get_elemf_sse(xyz2, 1); z2 = get_elemf_sse(xyz2, 2);
		x3 = get_elemf_sse(xyz3, 0); y3 = get_elemf_sse(xyz3, 1); z3 = get_elemf_sse(xyz3, 2);
		
		return (float)get_elemi_sse(ijk1, 0) * 0.1f;
#else
		// A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
		// a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
		// a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z),
		// where c = 1/6.
		
		float xyz1[3] = { xyz0[0] - (float)i1 + G3,
		                  xyz0[1] - (float)j1 + G3,
		                  xyz0[2] - (float)k1 + G3 };  // Offsets for second corner in (x,y,z) coords
		float xyz2[3] = { xyz0[0] - (float)i2 + 2.0f * G3,
		                  xyz0[1] - (float)j2 + 2.0f * G3,
		                  xyz0[2] - (float)k2 + 2.0f * G3 };  // Offsets for third corner in (x,y,z) coords
		float xyz3[3] = { xyz0[0] - 1.0f + 3.0f * G3,
		                  xyz0[1] - 1.0f + 3.0f * G3,
		                  xyz0[2] - 1.0f + 3.0f * G3 };  // Offsets for last corner in (x,y,z) coords
		
		/* Compat */
		x1 = xyz1[0]; y1 = xyz1[1]; z1 = xyz1[2];
		x2 = xyz2[0]; y2 = xyz2[1]; z2 = xyz2[2];
		x3 = xyz3[0]; y3 = xyz3[1]; z3 = xyz3[2];
#endif
	}
	
	
	{
		float t20 = 0.0f, t40 = 0.0f;
		float t21 = 0.0f, t41 = 0.0f;
		float t22 = 0.0f, t42 = 0.0f;
		float t23 = 0.0f, t43 = 0.0f;
		float n0=0.0f, n1=0.0f, n2=0.0f, n3=0.0f; // Noise contributions from the four simplex corners
		float t0, t1, t2, t3;
		
		// The scale is empirical, to make it cover [-1,1],
		// and to make it approximately match the range of our
		// Perlin noise implementation.
		const float scale = 68.0f;
		
		// Calculate the contribution from the four corners
		t0 = 0.5f - x0*x0 - y0*y0 - z0*z0;
		if (t0 >= 0.0f) {
			const float *g0 = grad3 (i, j, k, seed);
			t20 = t0 * t0;
			t40 = t20 * t20;
			n0 = t40 * (g0[0] * x0 + g0[1] * y0 + g0[2] * z0);
		}
		
		t1 = 0.5f - x1*x1 - y1*y1 - z1*z1;
		if (t1 >= 0.0f) {
			const float *g1 = grad3 (i+i1, j+j1, k+k1, seed);
			t21 = t1 * t1;
			t41 = t21 * t21;
			n1 = t41 * (g1[0] * x1 + g1[1] * y1 + g1[2] * z1);
		}
		
		t2 = 0.5f - x2*x2 - y2*y2 - z2*z2;
		if (t2 >= 0.0f) {
			const float *g2 = grad3 (i+i2, j+j2, k+k2, seed);
			t22 = t2 * t2;
			t42 = t22 * t22;
			n2 = t42 * (g2[0] * x2 + g2[1] * y2 + g2[2] * z2);
		}
		
		t3 = 0.5f - x3*x3 - y3*y3 - z3*z3;
		if (t3 >= 0.0f) {
			const float *g3 = grad3 (i+1, j+1, k+1, seed);
			t23 = t3 * t3;
			t43 = t23 * t23;
			n3 = t43 * (g3[0] * x3 + g3[1] * y3 + g3[2] * z3);
		}
		
		// Sum up and scale the result (0..1 range)
		return 0.5f * scale * (n0 + n1 + n2 + n3) + 0.5f;
	}
}
#endif

#if 0
// A lookup table to traverse the simplex around a given point in 4D.
// Details can be found where this table is used, in the 4D noise method.
/* TODO: This should not be required, backport it from Bill's GLSL code! */
static unsigned char simplex[64][4] = {
  {0,1,2,3},{0,1,3,2},{0,0,0,0},{0,2,3,1},{0,0,0,0},{0,0,0,0},{0,0,0,0},{1,2,3,0},
  {0,2,1,3},{0,0,0,0},{0,3,1,2},{0,3,2,1},{0,0,0,0},{0,0,0,0},{0,0,0,0},{1,3,2,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {1,2,0,3},{0,0,0,0},{1,3,0,2},{0,0,0,0},{0,0,0,0},{0,0,0,0},{2,3,0,1},{2,3,1,0},
  {1,0,2,3},{1,0,3,2},{0,0,0,0},{0,0,0,0},{0,0,0,0},{2,0,3,1},{0,0,0,0},{2,1,3,0},
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
  {2,0,1,3},{0,0,0,0},{0,0,0,0},{0,0,0,0},{3,0,1,2},{3,0,2,1},{0,0,0,0},{3,1,2,0},
  {2,1,0,3},{0,0,0,0},{0,0,0,0},{0,0,0,0},{3,1,0,2},{0,0,0,0},{3,2,0,1},{3,2,1,0}};
#endif

// 4D simplex noise with derivatives.
// If the last four arguments are not null, the analytic derivative
// (the 4D gradient of the scalar noise field) is also calculated.
BLI_INLINE float simplexnoise4(float x, float y, float z, float w, int seed,
                               float *dnoise_dx, float *dnoise_dy, float *dnoise_dz, float *dnoise_dw)
{
	// The skewing and unskewing factors are hairy again for the 4D case
	const float F4 = 0.309016994; // F4 = (sqrt(5.0)-1.0)/4.0
	const float G4 = 0.138196601; // G4 = (5.0-sqrt(5.0))/20.0
	static const float zero[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	
	// Gradients at simplex corners
	const float *g0 = zero, *g1 = zero, *g2 = zero, *g3 = zero, *g4 = zero;
	
	// Noise contributions from the four simplex corners
	float n0=0.0f, n1=0.0f, n2=0.0f, n3=0.0f, n4=0.0f;
	float t20 = 0.0f, t21 = 0.0f, t22 = 0.0f, t23 = 0.0f, t24 = 0.0f;
	float t40 = 0.0f, t41 = 0.0f, t42 = 0.0f, t43 = 0.0f, t44 = 0.0f;
	
	// Skew the (x,y,z,w) space to determine which cell of 24 simplices we're in
	float s = (x + y + z + w) * F4; // Factor for 4D skewing
	float xs = x + s;
	float ys = y + s;
	float zs = z + s;
	float ws = w + s;
	int i = quick_floor(xs);
	int j = quick_floor(ys);
	int k = quick_floor(zs);
	int l = quick_floor(ws);
	
	float t = (i + j + k + l) * G4; // Factor for 4D unskewing
	float X0 = i - t; // Unskew the cell origin back to (x,y,z,w) space
	float Y0 = j - t;
	float Z0 = k - t;
	float W0 = l - t;
	
	float x0 = x - X0;  // The x,y,z,w distances from the cell origin
	float y0 = y - Y0;
	float z0 = z - Z0;
	float w0 = w - W0;
	
	/* Simplex subdivision according to get the Schlaefli orthoscheme */
	bool gt0 = (x0 >= y0), lt0 = !gt0;
	bool gt1 = (y0 >= z0), lt1 = !gt1;
	bool gt2 = (z0 >= w0), lt2 = !gt2;
	bool gt3 = (w0 >= x0), lt3 = !gt3;
	bool gt4 = (x0 >= z0), lt4 = !gt4;
	bool gt5 = (y0 >= w0), lt5 = !gt5;
	
	int i1 = (lt3 & gt0 & gt4);
	int j1 = (lt0 & gt1 & gt5);
	int k1 = (lt1 & gt2 & lt4);
	int l1 = (lt2 & gt3 & lt5);
	int i2 = (lt3 & gt0) | (gt0 & gt4) | (gt4 & lt3);
	int j2 = (lt0 & gt1) | (gt1 & gt5) | (gt5 & lt0);
	int k2 = (lt1 & gt2) | (gt2 & lt4) | (lt4 & lt1);
	int l2 = (lt2 & gt3) | (gt3 & lt5) | (lt5 & lt2);
	int i3 = lt3 | gt0 | gt4;
	int j3 = lt0 | gt1 | gt5;
	int k3 = lt1 | gt2 | lt4;
	int l3 = lt2 | gt3 | lt5;
	// The fifth corner has all coordinate offsets = 1, so no need to look that up.
	
	float x1 = x0 - i1 + G4; // Offsets for second corner in (x,y,z,w) coords
	float y1 = y0 - j1 + G4;
	float z1 = z0 - k1 + G4;
	float w1 = w0 - l1 + G4;
	float x2 = x0 - i2 + 2.0f * G4; // Offsets for third corner in (x,y,z,w) coords
	float y2 = y0 - j2 + 2.0f * G4;
	float z2 = z0 - k2 + 2.0f * G4;
	float w2 = w0 - l2 + 2.0f * G4;
	float x3 = x0 - i3 + 3.0f * G4; // Offsets for fourth corner in (x,y,z,w) coords
	float y3 = y0 - j3 + 3.0f * G4;
	float z3 = z0 - k3 + 3.0f * G4;
	float w3 = w0 - l3 + 3.0f * G4;
	float x4 = x0 - 1.0f + 4.0f * G4; // Offsets for last corner in (x,y,z,w) coords
	float y4 = y0 - 1.0f + 4.0f * G4;
	float z4 = z0 - 1.0f + 4.0f * G4;
	float w4 = w0 - 1.0f + 4.0f * G4;
	
	float t0, t1, t2, t3, t4;
	
	// The scale is empirical, to make it
	// cover [-1,1], and to make it approximately match the range of our
	// Perlin noise implementation.
	const float scale = 54.0f;
	float noise;
	
	// Calculate the contribution from the five corners
	t0 = 0.5f - x0*x0 - y0*y0 - z0*z0 - w0*w0;
	if (t0 >= 0.0f) {
		t20 = t0 * t0;
		t40 = t20 * t20;
		g0 = grad4 (i, j, k, l, seed);
		n0 = t40 * (g0[0] * x0 + g0[1] * y0 + g0[2] * z0 + g0[3] * w0);
	}
	
	t1 = 0.5f - x1*x1 - y1*y1 - z1*z1 - w1*w1;
	if (t1 >= 0.0f) {
		t21 = t1 * t1;
		t41 = t21 * t21;
		g1 = grad4 (i+i1, j+j1, k+k1, l+l1, seed);
		n1 = t41 * (g1[0] * x1 + g1[1] * y1 + g1[2] * z1 + g1[3] * w1);
	}
	
	t2 = 0.5f - x2*x2 - y2*y2 - z2*z2 - w2*w2;
	if (t2 >= 0.0f) {
		t22 = t2 * t2;
		t42 = t22 * t22;
		g2 = grad4 (i+i2, j+j2, k+k2, l+l2, seed);
		n2 = t42 * (g2[0] * x2 + g2[1] * y2 + g2[2] * z2 + g2[3] * w2);
	}
	
	t3 = 0.5f - x3*x3 - y3*y3 - z3*z3 - w3*w3;
	if (t3 >= 0.0f) {
		t23 = t3 * t3;
		t43 = t23 * t23;
		g3 = grad4 (i+i3, j+j3, k+k3, l+l3, seed);
		n3 = t43 * (g3[0] * x3 + g3[1] * y3 + g3[2] * z3 + g3[3] * w3);
	}
	
	t4 = 0.5f - x4*x4 - y4*y4 - z4*z4 - w4*w4;
	if (t4 >= 0.0f) {
		t24 = t4 * t4;
		t44 = t24 * t24;
		g4 = grad4 (i+1, j+1, k+1, l+1, seed);
		n4 = t44 * (g4[0] * x4 + g4[1] * y4 + g4[2] * z4 + g4[3] * w4);
	}
	
	// Sum up and scale the result.
	noise = 0.5f * scale * (n0 + n1 + n2 + n3 + n4) + 0.5f;
	
	// Compute derivative, if requested by supplying non-null pointers
	// for the last four arguments
	if (dnoise_dx) {
		float temp0, temp1, temp2, temp3, temp4;
		BLI_assert(dnoise_dy && dnoise_dz && dnoise_dw);
		/*  A straight, unoptimised calculation would be like:
		 *     *dnoise_dx = -8.0f * t20 * t0 * x0 * dot(g0[0], g0[1], g0[2], g0[3], x0, y0, z0, w0) + t40 * g0[0];
		 *    *dnoise_dy = -8.0f * t20 * t0 * y0 * dot(g0[0], g0[1], g0[2], g0[3], x0, y0, z0, w0) + t40 * g0[1];
		 *    *dnoise_dz = -8.0f * t20 * t0 * z0 * dot(g0[0], g0[1], g0[2], g0[3], x0, y0, z0, w0) + t40 * g0[2];
		 *    *dnoise_dw = -8.0f * t20 * t0 * w0 * dot(g0[0], g0[1], g0[2], g0[3], x0, y0, z0, w0) + t40 * g0[3];
		 *    *dnoise_dx += -8.0f * t21 * t1 * x1 * dot(g1[0], g1[1], g1[2], g1[3], x1, y1, z1, w1) + t41 * g1[0];
		 *    *dnoise_dy += -8.0f * t21 * t1 * y1 * dot(g1[0], g1[1], g1[2], g1[3], x1, y1, z1, w1) + t41 * g1[1];
		 *    *dnoise_dz += -8.0f * t21 * t1 * z1 * dot(g1[0], g1[1], g1[2], g1[3], x1, y1, z1, w1) + t41 * g1[2];
		 *    *dnoise_dw = -8.0f * t21 * t1 * w1 * dot(g1[0], g1[1], g1[2], g1[3], x1, y1, z1, w1) + t41 * g1[3];
		 *    *dnoise_dx += -8.0f * t22 * t2 * x2 * dot(g2[0], g2[1], g2[2], g2[3], x2, y2, z2, w2) + t42 * g2[0];
		 *    *dnoise_dy += -8.0f * t22 * t2 * y2 * dot(g2[0], g2[1], g2[2], g2[3], x2, y2, z2, w2) + t42 * g2[1];
		 *    *dnoise_dz += -8.0f * t22 * t2 * z2 * dot(g2[0], g2[1], g2[2], g2[3], x2, y2, z2, w2) + t42 * g2[2];
		 *    *dnoise_dw += -8.0f * t22 * t2 * w2 * dot(g2[0], g2[1], g2[2], g2[3], x2, y2, z2, w2) + t42 * g2[3];
		 *    *dnoise_dx += -8.0f * t23 * t3 * x3 * dot(g3[0], g3[1], g3[2], g3[3], x3, y3, z3, w3) + t43 * g3[0];
		 *    *dnoise_dy += -8.0f * t23 * t3 * y3 * dot(g3[0], g3[1], g3[2], g3[3], x3, y3, z3, w3) + t43 * g3[1];
		 *    *dnoise_dz += -8.0f * t23 * t3 * z3 * dot(g3[0], g3[1], g3[2], g3[3], x3, y3, z3, w3) + t43 * g3[2];
		 *    *dnoise_dw += -8.0f * t23 * t3 * w3 * dot(g3[0], g3[1], g3[2], g3[3], x3, y3, z3, w3) + t43 * g3[3];
		 *    *dnoise_dx += -8.0f * t24 * t4 * x4 * dot(g4[0], g4[1], g4[2], g4[3], x4, y4, z4, w4) + t44 * g4[0];
		 *    *dnoise_dy += -8.0f * t24 * t4 * y4 * dot(g4[0], g4[1], g4[2], g4[3], x4, y4, z4, w4) + t44 * g4[1];
		 *    *dnoise_dz += -8.0f * t24 * t4 * z4 * dot(g4[0], g4[1], g4[2], g4[3], x4, y4, z4, w4) + t44 * g4[2];
		 *    *dnoise_dw += -8.0f * t24 * t4 * w4 * dot(g4[0], g4[1], g4[2], g4[3], x4, y4, z4, w4) + t44 * g4[3];
		 */
		temp0 = t20 * t0 * (g0[0] * x0 + g0[1] * y0 + g0[2] * z0 + g0[3] * w0);
		*dnoise_dx = temp0 * x0;
		*dnoise_dy = temp0 * y0;
		*dnoise_dz = temp0 * z0;
		*dnoise_dw = temp0 * w0;
		temp1 = t21 * t1 * (g1[0] * x1 + g1[1] * y1 + g1[2] * z1 + g1[3] * w1);
		*dnoise_dx += temp1 * x1;
		*dnoise_dy += temp1 * y1;
		*dnoise_dz += temp1 * z1;
		*dnoise_dw += temp1 * w1;
		temp2 = t22 * t2 * (g2[0] * x2 + g2[1] * y2 + g2[2] * z2 + g2[3] * w2);
		*dnoise_dx += temp2 * x2;
		*dnoise_dy += temp2 * y2;
		*dnoise_dz += temp2 * z2;
		*dnoise_dw += temp2 * w2;
		temp3 = t23 * t3 * (g3[0] * x3 + g3[1] * y3 + g3[2] * z3 + g3[3] * w3);
		*dnoise_dx += temp3 * x3;
		*dnoise_dy += temp3 * y3;
		*dnoise_dz += temp3 * z3;
		*dnoise_dw += temp3 * w3;
		temp4 = t24 * t4 * (g4[0] * x4 + g4[1] * y4 + g4[2] * z4 + g4[3] * w4);
		*dnoise_dx += temp4 * x4;
		*dnoise_dy += temp4 * y4;
		*dnoise_dz += temp4 * z4;
		*dnoise_dw += temp4 * w4;
		*dnoise_dx *= -8.0f;
		*dnoise_dy *= -8.0f;
		*dnoise_dz *= -8.0f;
		*dnoise_dw *= -8.0f;
		*dnoise_dx += t40 * g0[0] + t41 * g1[0] + t42 * g2[0] + t43 * g3[0] + t44 * g4[0];
		*dnoise_dy += t40 * g0[1] + t41 * g1[1] + t42 * g2[1] + t43 * g3[1] + t44 * g4[1];
		*dnoise_dz += t40 * g0[2] + t41 * g1[2] + t42 * g2[2] + t43 * g3[2] + t44 * g4[2];
		*dnoise_dw += t40 * g0[3] + t41 * g1[3] + t42 * g2[3] + t43 * g3[3] + t44 * g4[3];
		// Scale derivative to match the noise scaling
		*dnoise_dx *= 0.5f * scale;
		*dnoise_dy *= 0.5f * scale;
		*dnoise_dz *= 0.5f * scale;
		*dnoise_dw *= 0.5f * scale;
	}
	
	return noise;
}

#ifdef SIMPLEXNOISE_USE_SSE2

#define SHUFFLE_MASK(a, b, c, d) ((b) << 6) | ((a) << 4) | ((d) << 2) | (c)
//#define SHUFFLE_MASK(a, b, c, d) ((a) << 6) | ((b) << 4) | ((c) << 2) | (d)
//#define SHUFFLE_MASK(a, b, c, d) ((d) << 6) | ((c) << 4) | ((b) << 2) | (a)

BLI_INLINE void fuzzy_assert(float a, float b)
{
	static const float eps = 1e-6f;
	BLI_assert(fabs(a - b) < eps);
}

BLI_INLINE __m128 sse_invert(const __m128 v, bool a, bool b, bool c, bool d)
{
	return _mm_xor_ps(v, _mm_castsi128_ps(_mm_set_epi32(a? 0xffffffff: 0, b? 0xffffffff: 0, c? 0xffffffff: 0, d? 0xffffffff: 0)));
}

BLI_INLINE void sse_assert_f1(const __m128 f, float x)
{
	const float *p = (const float *)(&f);
	fuzzy_assert(p[3], x);
}

BLI_INLINE void sse_assert_f4(const __m128 f, float x, float y, float z, float w)
{
	const float *p = (const float *)(&f);
	fuzzy_assert(p[3], x);
	fuzzy_assert(p[2], y);
	fuzzy_assert(p[1], z);
	fuzzy_assert(p[0], w);
}

BLI_INLINE void sse_assert_cmp(const __m128 f, bool x, bool y, bool z, bool w)
{
	const float *p = (const float *)(&f);
	bool a = isnan(p[3]);
	bool b = isnan(p[2]);
	bool c = isnan(p[1]);
	bool d = isnan(p[0]);
	fuzzy_assert(a, x);
	fuzzy_assert(b, y);
	fuzzy_assert(c, z);
	fuzzy_assert(d, w);
}

BLI_INLINE void sse_assert_i4(const __m128i f, int x, int y, int z, int w)
{
	const int *p = (const int *)(&f);
	fuzzy_assert(p[3], x);
	fuzzy_assert(p[2], y);
	fuzzy_assert(p[1], z);
	fuzzy_assert(p[0], w);
}

/* XXX SSE2 does not have a native horizontal add (hadd) function, this is a replacement
 */
BLI_INLINE __m128 sse_hadd(const __m128 a)
{
	__m128 a1 = _mm_movehl_ps(a, a); /* (a2, a3, a2, a3) */
	__m128 s1 = _mm_add_ps(a, a1); /* (a0+a2, a1+a3, ---, ---) */
	__m128 a2 = _mm_shuffle_ps(s1, s1, SHUFFLE_MASK(0,0,1,0)); /* (a1+a3, ---, ---, ---) */
	__m128 s2 = _mm_add_ps(a1, a2); /* (a0+a1+a2+a3, ---, ---, ---) */
	return _mm_shuffle_ps(s2, s2, SHUFFLE_MASK(0,0,0,0));
}

BLI_INLINE __m128i sse_quick_floor(const __m128 x)
{
	__m128i b = _mm_cvttps_epi32(x);
	
	__m128 cmp = _mm_cmplt_ps(x, _mm_set1_ps(0.0f));
	__m128i isneg = _mm_castps_si128(cmp);
	__m128i result = _mm_add_epi32(b, isneg); // unsaturated add 0xffffffff is the same as subtract -1
	return result;
}

BLI_INLINE __m128i sse_bool_to_int(const __m128 b)
{
	return _mm_sub_epi32(_mm_set1_epi32(0), _mm_castps_si128(b));
}

// 4D simplex noise with derivatives.
// If the last four arguments are not null, the analytic derivative
// (the 4D gradient of the scalar noise field) is also calculated.
static float simplexnoise4_sse(float x, float y, float z, float w, int seed,
                               float *dnoise_dx, float *dnoise_dy, float *dnoise_dz, float *dnoise_dw)
{
	// The skewing and unskewing factors are hairy again for the 4D case
	const float F4 = 0.309016994; // F4 = (sqrt(5.0)-1.0)/4.0
	const float G4 = 0.138196601; // G4 = (5.0-sqrt(5.0))/20.0
	static const float zero[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	
	// Gradients at simplex corners
	const float *g0 = zero, *g1 = zero, *g2 = zero, *g3 = zero, *g4 = zero;
	
	// Noise contributions from the four simplex corners
	float n0=0.0f, n1=0.0f, n2=0.0f, n3=0.0f, n4=0.0f;
	float t20 = 0.0f, t21 = 0.0f, t22 = 0.0f, t23 = 0.0f, t24 = 0.0f;
	float t40 = 0.0f, t41 = 0.0f, t42 = 0.0f, t43 = 0.0f, t44 = 0.0f;
	
	// Skew the (x,y,z,w) space to determine which cell of 24 simplices we're in
	float s = (x + y + z + w) * F4; // Factor for 4D skewing
	float xs = x + s;
	float ys = y + s;
	float zs = z + s;
	float ws = w + s;
	int i = quick_floor(xs);
	int j = quick_floor(ys);
	int k = quick_floor(zs);
	int l = quick_floor(ws);
	
	float t = (i + j + k + l) * G4; // Factor for 4D unskewing
	float X0 = i - t; // Unskew the cell origin back to (x,y,z,w) space
	float Y0 = j - t;
	float Z0 = k - t;
	float W0 = l - t;
	
	float x0 = x - X0;  // The x,y,z,w distances from the cell origin
	float y0 = y - Y0;
	float z0 = z - Z0;
	float w0 = w - W0;
	
	/* Simplex subdivision according to get the Schlaefli orthoscheme */
	bool gt0 = (x0 >= y0), lt0 = !gt0;
	bool gt1 = (y0 >= z0), lt1 = !gt1;
	bool gt2 = (z0 >= w0), lt2 = !gt2;
	bool gt3 = (w0 >= x0), lt3 = !gt3;
	bool gt4 = (x0 >= z0), lt4 = !gt4;
	bool gt5 = (y0 >= w0), lt5 = !gt5;
	
	int i1 = (lt3 & gt0 & gt4);
	int j1 = (lt0 & gt1 & gt5);
	int k1 = (lt1 & gt2 & lt4);
	int l1 = (lt2 & gt3 & lt5);
	int i2 = (lt3 & gt0) | (gt0 & gt4) | (gt4 & lt3);
	int j2 = (lt0 & gt1) | (gt1 & gt5) | (gt5 & lt0);
	int k2 = (lt1 & gt2) | (gt2 & lt4) | (lt4 & lt1);
	int l2 = (lt2 & gt3) | (gt3 & lt5) | (lt5 & lt2);
	int i3 = lt3 | gt0 | gt4;
	int j3 = lt0 | gt1 | gt5;
	int k3 = lt1 | gt2 | lt4;
	int l3 = lt2 | gt3 | lt5;
	// The fifth corner has all coordinate offsets = 1, so no need to look that up.

	float x1 = x0 - i1 + G4; // Offsets for second corner in (x,y,z,w) coords
	float y1 = y0 - j1 + G4;
	float z1 = z0 - k1 + G4;
	float w1 = w0 - l1 + G4;
	float x2 = x0 - i2 + 2.0f * G4; // Offsets for third corner in (x,y,z,w) coords
	float y2 = y0 - j2 + 2.0f * G4;
	float z2 = z0 - k2 + 2.0f * G4;
	float w2 = w0 - l2 + 2.0f * G4;
	float x3 = x0 - i3 + 3.0f * G4; // Offsets for fourth corner in (x,y,z,w) coords
	float y3 = y0 - j3 + 3.0f * G4;
	float z3 = z0 - k3 + 3.0f * G4;
	float w3 = w0 - l3 + 3.0f * G4;
	float x4 = x0 - 1.0f + 4.0f * G4; // Offsets for last corner in (x,y,z,w) coords
	float y4 = y0 - 1.0f + 4.0f * G4;
	float z4 = z0 - 1.0f + 4.0f * G4;
	float w4 = w0 - 1.0f + 4.0f * G4;
	
	float t0, t1, t2, t3, t4;
	
	// The scale is empirical, to make it
	// cover [-1,1], and to make it approximately match the range of our
	// Perlin noise implementation.
	const float scale = 54.0f;
	float noise;
	
	// (((((((((((((((((((((((((((((((((((((((((((((((((
	// (((((((((((((((((((((((((((((((((((((((((((((((((
	__m128 xyzw = _mm_set_ps(x, y, z, w);
	__m128 ssss = _mm_mul_ps(sse_hadd(xyzw), _mm_set1_ps(F4));
	__m128 xyzws = _mm_add_ps(xyzw, ssss);
	__m128i ijkl = sse_quick_floor(xyzws);
	__m128 XYZW0 = _mm_sub_ps(_mm_cvtepi32_ps(ijkl), _mm_set1_ps(t));
	__m128 xyzw0 = _mm_sub_ps(xyzw, XYZW0);
	
	/* temporary shifts for horizontal comparisons:
	 * tmp1 = { x>=w, y>=x, z>=y, w>=z }
	 * tmp2 = { y<x, z<y, w<z, x<w }
	 * tmp3 = { z<x, w<y, z>=x, w>=y }
	 */
	__m128 xyxy0 = _mm_movehl_ps(xyzw0, xyzw0);
	__m128 wxyz0 = _mm_shuffle_ps(xyzw0, xyzw0, SHUFFLE_MASK(3,0,1,2));
	__m128 cmp1 = _mm_cmplt_ps(xyzw0, wxyz0);
	__m128 cmp2 = _mm_cmplt_ps(xyzw0, xyxy0);
	
	__m128 tmp1 = sse_invert(cmp1, true, true, true, true);
	__m128 tmp2 = _mm_shuffle_ps(cmp1, cmp1, SHUFFLE_MASK(1,2,3,0));
	__m128 tmp3 = sse_invert(_mm_movelh_ps(cmp2, cmp2), 0, 0, 1, 1);
	
	/* tmp1 & tmp2 & tmp3 */
	__m128i ijkl1 = sse_bool_to_int(_mm_and_ps(tmp1, _mm_and_ps(tmp2, tmp3)));
	/* (tmp1 & tmp2) | (tmp2 | tmp3) | (tmp3 & tmp1) */
	__m128i ijkl2 = sse_bool_to_int(_mm_or_ps(_mm_and_ps(tmp1, tmp2), _mm_or_ps(_mm_and_ps(tmp2, tmp3), _mm_and_ps(tmp3, tmp1))));
	/* tmp1 | tmp2 | tmp3 */
	__m128i ijkl3 = sse_bool_to_int(_mm_or_ps(tmp1, _mm_or_ps(tmp2, tmp3)));
	
	__m128 xyzw1 = _mm_add_ps(xyzw0, _mm_sub_ps(_mm_set1_ps(G4), _mm_cvtepi32_ps(ijkl1)));
	__m128 xyzw2 = _mm_add_ps(xyzw0, _mm_sub_ps(_mm_set1_ps(2.0f*G4), _mm_cvtepi32_ps(ijkl2)));
	__m128 xyzw3 = _mm_add_ps(xyzw0, _mm_sub_ps(_mm_set1_ps(3.0f*G4), _mm_cvtepi32_ps(ijkl3)));
	__m128 xyzw4 = _mm_add_ps(xyzw0, _mm_sub_ps(_mm_set1_ps(4.0f*G4), _mm_set1_ps(1.0f)));
	
	__m128 xyzw0_sq = sse_hadd(_mm_mul_ps(xyzw0, xyzw0));
	__m128 xyzw1_sq = sse_hadd(_mm_mul_ps(xyzw1, xyzw1));
	__m128 xyzw2_sq = sse_hadd(_mm_mul_ps(xyzw2, xyzw2));
	__m128 xyzw3_sq = sse_hadd(_mm_mul_ps(xyzw3, xyzw3));
	__m128 xyzw4_sq = sse_hadd(_mm_mul_ps(xyzw4, xyzw4));
	
	__m128 t0123 = _mm_sub_ps(_mm_set1_ps(0.5f),
	                           _mm_shuffle_ps(
	                               _mm_movehl_ps(xyzw0_sq,
	                                             xyzw1_sq),
	                               _mm_movehl_ps(xyzw2_sq,
	                                             xyzw3_sq),
	                               SHUFFLE_MASK(0,2,0,2))
	                           );
	__m128 t4444 = xyzw4_sq;
	
	
	// Calculate the contribution from the five corners
	t0 = 0.5f - x0*x0 - y0*y0 - z0*z0 - w0*w0;
	if (t0 >= 0.0f) {
		t20 = t0 * t0;
		t40 = t20 * t20;
		g0 = grad4 (i, j, k, l, seed);
		n0 = t40 * (g0[0] * x0 + g0[1] * y0 + g0[2] * z0 + g0[3] * w0);
	}
	
	t1 = 0.5f - x1*x1 - y1*y1 - z1*z1 - w1*w1;
	if (t1 >= 0.0f) {
		t21 = t1 * t1;
		t41 = t21 * t21;
		g1 = grad4 (i+i1, j+j1, k+k1, l+l1, seed);
		n1 = t41 * (g1[0] * x1 + g1[1] * y1 + g1[2] * z1 + g1[3] * w1);
	}
	
	t2 = 0.5f - x2*x2 - y2*y2 - z2*z2 - w2*w2;
	if (t2 >= 0.0f) {
		t22 = t2 * t2;
		t42 = t22 * t22;
		g2 = grad4 (i+i2, j+j2, k+k2, l+l2, seed);
		n2 = t42 * (g2[0] * x2 + g2[1] * y2 + g2[2] * z2 + g2[3] * w2);
	}
	
	t3 = 0.5f - x3*x3 - y3*y3 - z3*z3 - w3*w3;
	if (t3 >= 0.0f) {
		t23 = t3 * t3;
		t43 = t23 * t23;
		g3 = grad4 (i+i3, j+j3, k+k3, l+l3, seed);
		n3 = t43 * (g3[0] * x3 + g3[1] * y3 + g3[2] * z3 + g3[3] * w3);
	}
	
	t4 = 0.5f - x4*x4 - y4*y4 - z4*z4 - w4*w4;
	if (t4 >= 0.0f) {
		t24 = t4 * t4;
		t44 = t24 * t24;
		g4 = grad4 (i+1, j+1, k+1, l+1, seed);
		n4 = t44 * (g4[0] * x4 + g4[1] * y4 + g4[2] * z4 + g4[3] * w4);
	}
	
	// Sum up and scale the result.
	noise = 0.5f * scale * (n0 + n1 + n2 + n3 + n4) + 0.5f;
	
	// Compute derivative, if requested by supplying non-null pointers
	// for the last four arguments
	if (dnoise_dx) {
		float temp0, temp1, temp2, temp3, temp4;
		BLI_assert(dnoise_dy && dnoise_dz && dnoise_dw);
		/*  A straight, unoptimised calculation would be like:
		 *     *dnoise_dx = -8.0f * t20 * t0 * x0 * dot(g0[0], g0[1], g0[2], g0[3], x0, y0, z0, w0) + t40 * g0[0];
		 *    *dnoise_dy = -8.0f * t20 * t0 * y0 * dot(g0[0], g0[1], g0[2], g0[3], x0, y0, z0, w0) + t40 * g0[1];
		 *    *dnoise_dz = -8.0f * t20 * t0 * z0 * dot(g0[0], g0[1], g0[2], g0[3], x0, y0, z0, w0) + t40 * g0[2];
		 *    *dnoise_dw = -8.0f * t20 * t0 * w0 * dot(g0[0], g0[1], g0[2], g0[3], x0, y0, z0, w0) + t40 * g0[3];
		 *    *dnoise_dx += -8.0f * t21 * t1 * x1 * dot(g1[0], g1[1], g1[2], g1[3], x1, y1, z1, w1) + t41 * g1[0];
		 *    *dnoise_dy += -8.0f * t21 * t1 * y1 * dot(g1[0], g1[1], g1[2], g1[3], x1, y1, z1, w1) + t41 * g1[1];
		 *    *dnoise_dz += -8.0f * t21 * t1 * z1 * dot(g1[0], g1[1], g1[2], g1[3], x1, y1, z1, w1) + t41 * g1[2];
		 *    *dnoise_dw = -8.0f * t21 * t1 * w1 * dot(g1[0], g1[1], g1[2], g1[3], x1, y1, z1, w1) + t41 * g1[3];
		 *    *dnoise_dx += -8.0f * t22 * t2 * x2 * dot(g2[0], g2[1], g2[2], g2[3], x2, y2, z2, w2) + t42 * g2[0];
		 *    *dnoise_dy += -8.0f * t22 * t2 * y2 * dot(g2[0], g2[1], g2[2], g2[3], x2, y2, z2, w2) + t42 * g2[1];
		 *    *dnoise_dz += -8.0f * t22 * t2 * z2 * dot(g2[0], g2[1], g2[2], g2[3], x2, y2, z2, w2) + t42 * g2[2];
		 *    *dnoise_dw += -8.0f * t22 * t2 * w2 * dot(g2[0], g2[1], g2[2], g2[3], x2, y2, z2, w2) + t42 * g2[3];
		 *    *dnoise_dx += -8.0f * t23 * t3 * x3 * dot(g3[0], g3[1], g3[2], g3[3], x3, y3, z3, w3) + t43 * g3[0];
		 *    *dnoise_dy += -8.0f * t23 * t3 * y3 * dot(g3[0], g3[1], g3[2], g3[3], x3, y3, z3, w3) + t43 * g3[1];
		 *    *dnoise_dz += -8.0f * t23 * t3 * z3 * dot(g3[0], g3[1], g3[2], g3[3], x3, y3, z3, w3) + t43 * g3[2];
		 *    *dnoise_dw += -8.0f * t23 * t3 * w3 * dot(g3[0], g3[1], g3[2], g3[3], x3, y3, z3, w3) + t43 * g3[3];
		 *    *dnoise_dx += -8.0f * t24 * t4 * x4 * dot(g4[0], g4[1], g4[2], g4[3], x4, y4, z4, w4) + t44 * g4[0];
		 *    *dnoise_dy += -8.0f * t24 * t4 * y4 * dot(g4[0], g4[1], g4[2], g4[3], x4, y4, z4, w4) + t44 * g4[1];
		 *    *dnoise_dz += -8.0f * t24 * t4 * z4 * dot(g4[0], g4[1], g4[2], g4[3], x4, y4, z4, w4) + t44 * g4[2];
		 *    *dnoise_dw += -8.0f * t24 * t4 * w4 * dot(g4[0], g4[1], g4[2], g4[3], x4, y4, z4, w4) + t44 * g4[3];
		 */
		temp0 = t20 * t0 * (g0[0] * x0 + g0[1] * y0 + g0[2] * z0 + g0[3] * w0);
		*dnoise_dx = temp0 * x0;
		*dnoise_dy = temp0 * y0;
		*dnoise_dz = temp0 * z0;
		*dnoise_dw = temp0 * w0;
		temp1 = t21 * t1 * (g1[0] * x1 + g1[1] * y1 + g1[2] * z1 + g1[3] * w1);
		*dnoise_dx += temp1 * x1;
		*dnoise_dy += temp1 * y1;
		*dnoise_dz += temp1 * z1;
		*dnoise_dw += temp1 * w1;
		temp2 = t22 * t2 * (g2[0] * x2 + g2[1] * y2 + g2[2] * z2 + g2[3] * w2);
		*dnoise_dx += temp2 * x2;
		*dnoise_dy += temp2 * y2;
		*dnoise_dz += temp2 * z2;
		*dnoise_dw += temp2 * w2;
		temp3 = t23 * t3 * (g3[0] * x3 + g3[1] * y3 + g3[2] * z3 + g3[3] * w3);
		*dnoise_dx += temp3 * x3;
		*dnoise_dy += temp3 * y3;
		*dnoise_dz += temp3 * z3;
		*dnoise_dw += temp3 * w3;
		temp4 = t24 * t4 * (g4[0] * x4 + g4[1] * y4 + g4[2] * z4 + g4[3] * w4);
		*dnoise_dx += temp4 * x4;
		*dnoise_dy += temp4 * y4;
		*dnoise_dz += temp4 * z4;
		*dnoise_dw += temp4 * w4;
		*dnoise_dx *= -8.0f;
		*dnoise_dy *= -8.0f;
		*dnoise_dz *= -8.0f;
		*dnoise_dw *= -8.0f;
		*dnoise_dx += t40 * g0[0] + t41 * g1[0] + t42 * g2[0] + t43 * g3[0] + t44 * g4[0];
		*dnoise_dy += t40 * g0[1] + t41 * g1[1] + t42 * g2[1] + t43 * g3[1] + t44 * g4[1];
		*dnoise_dz += t40 * g0[2] + t41 * g1[2] + t42 * g2[2] + t43 * g3[2] + t44 * g4[2];
		*dnoise_dw += t40 * g0[3] + t41 * g1[3] + t42 * g2[3] + t43 * g3[3] + t44 * g4[3];
		// Scale derivative to match the noise scaling
		*dnoise_dx *= 0.5f * scale;
		*dnoise_dy *= 0.5f * scale;
		*dnoise_dz *= 0.5f * scale;
		*dnoise_dw *= 0.5f * scale;
	}
	
	sse_assert_f4(ssss, s, s, s, s);
	sse_assert_f4(xyzws, xs, ys, zs, ws);
	sse_assert_i4(ijkl, i, j, k, l);
	sse_assert_f4(XYZW0, X0, Y0, Z0, W0);
	sse_assert_f4(xyzw0, x0, y0, z0, w0);
	sse_assert_f4(xyxy0, x0, y0, x0, y0);
	sse_assert_f4(wxyz0, w0, x0, y0, z0);
	sse_assert_cmp(cmp1, x0 < w0, y0 < x0, z0 < y0, w0 < z0);
	sse_assert_cmp(cmp2, x0 < x0, y0 < y0, z0 < x0, w0 < y0);
	sse_assert_cmp(tmp1, x0 >= w0, y0 >= x0, z0 >= y0, w0 >= z0);
	sse_assert_cmp(tmp2, y0 < x0, z0 < y0, w0 < z0, x0 < w0);
	sse_assert_cmp(tmp3, z0 < x0, w0 < y0, z0 >= x0, w0 >= y0);
	sse_assert_i4(ijkl1, i1, j1, k1, l1);
	sse_assert_i4(ijkl2, i2, j2, k2, l2);
	sse_assert_i4(ijkl3, i3, j3, k3, l3);
	sse_assert_f4(xyzw1, x1, y1, z1, w1);
	sse_assert_f4(xyzw2, x2, y2, z2, w2);
	sse_assert_f4(xyzw3, x3, y3, z3, w3);
	sse_assert_f4(t0123, t0, t1, t2, t3);
	sse_assert_f1(t4444, t4);
	
	return noise;
}

#undef SHUFFLE_MASK

#endif

float BLI_simplexnoise3D(float noisesize, float x, float y, float z, int octaves, int seed)
{
	bool hard = false;
	float sum, t, amp = 1, fscale = 1;
	int i;
	
	if (noisesize != 0.0f) {
		noisesize = 1.0f / noisesize;
		x *= noisesize;
		y *= noisesize;
		z *= noisesize;
	}

	sum = 0;
	for (i = 0; i <= octaves; i++, amp *= 0.5f, fscale *= 2.0f) {
//		t = simplexnoise3(fscale * x, fscale * y, fscale * z, seed);
		t = 0.0f;
		if (hard)
			t = fabsf(2.0f * t - 1.0f);
		sum += t * amp;
	}
	
	sum *= ((float)(1 << octaves) / (float)((1 << (octaves + 1)) - 1));

	return sum;
}

float BLI_simplexnoise4D_ex(float noisesize, float x, float y, float z, float w, int octaves, int seed, float dnoise[4], float advect)
{
	const float oscale = ((float)(1 << octaves) / (float)((1 << (octaves + 1)) - 1));
	bool hard = false;
	float sum, dsum[3], t, amp = 1.0f;
	int i;
	
	if (noisesize != 0.0f) {
		noisesize = 1.0f / noisesize;
		x *= noisesize;
		y *= noisesize;
		z *= noisesize;
	}
	
	sum = 0;
	zero_v3(dsum);
	
	if (dnoise) {
		for (i = 0; i <= octaves; i++, amp *= 0.5f) {
#ifdef SIMPLEXNOISE_USE_SSE2
			t = simplexnoise4_sse(x, y, z, w, seed, &dnoise[0], &dnoise[1], &dnoise[2], &dnoise[3]);
#else
			t = simplexnoise4(x, y, z, w, seed, &dnoise[0], &dnoise[1], &dnoise[2], &dnoise[3]);
#endif
			if (hard) {
				t = fabsf(2.0f * t - 1.0f);
				mul_v3_fl(dsum, 2.0f);
			}
			
			sum += t * amp;
			madd_v3_v3fl(dsum, dnoise, amp);
			
			if (advect != 0.0f) {
				x += advect * dnoise[0];
				y += advect * dnoise[1];
				z += advect * dnoise[2];
				w += advect * dnoise[3];
			}
			
			x *= 2.0f;
			y *= 2.0f;
			z *= 2.0f;
			w *= 2.0f;
		}
		
		
		sum *= oscale;
		mul_v3_fl(dsum, oscale);
	}
	else {
		for (i = 0; i <= octaves; i++, amp *= 0.5f) {
#ifdef SIMPLEXNOISE_USE_SSE2
			t = simplexnoise4_sse(x, y, z, w, seed, NULL, NULL, NULL, NULL);
#else
			t = simplexnoise4(x, y, z, w, seed, NULL, NULL, NULL, NULL);
#endif
			if (hard)
				t = fabsf(2.0f * t - 1.0f);
			sum += t * amp;
			
			x *= 2.0f;
			y *= 2.0f;
			z *= 2.0f;
			w *= 2.0f;
		}
		
		sum *= oscale;
	}

	return sum;
}

float BLI_simplexnoise4D(float noisesize, float x, float y, float z, float w, int octaves, int seed)
{
	return BLI_simplexnoise4D_ex(noisesize, x, y, z, w, octaves, seed, NULL, 0.0f);
}

#undef SIMPLEXNOISE_USE_SSE2

/* ********************* FROM PERLIN HIMSELF: ******************** */

static const char p[512 + 2] = {
	0xA2, 0xA0, 0x19, 0x3B, 0xF8, 0xEB, 0xAA, 0xEE, 0xF3, 0x1C, 0x67, 0x28,
	0x1D, 0xED, 0x0,  0xDE, 0x95, 0x2E, 0xDC, 0x3F, 0x3A, 0x82, 0x35, 0x4D,
	0x6C, 0xBA, 0x36, 0xD0, 0xF6, 0xC,  0x79, 0x32, 0xD1, 0x59, 0xF4, 0x8,
	0x8B, 0x63, 0x89, 0x2F, 0xB8, 0xB4, 0x97, 0x83, 0xF2, 0x8F, 0x18, 0xC7,
	0x51, 0x14, 0x65, 0x87, 0x48, 0x20, 0x42, 0xA8, 0x80, 0xB5, 0x40, 0x13,
	0xB2, 0x22, 0x7E, 0x57, 0xBC, 0x7F, 0x6B, 0x9D, 0x86, 0x4C, 0xC8, 0xDB,
	0x7C, 0xD5, 0x25, 0x4E, 0x5A, 0x55, 0x74, 0x50, 0xCD, 0xB3, 0x7A, 0xBB,
	0xC3, 0xCB, 0xB6, 0xE2, 0xE4, 0xEC, 0xFD, 0x98, 0xB,  0x96, 0xD3, 0x9E,
	0x5C, 0xA1, 0x64, 0xF1, 0x81, 0x61, 0xE1, 0xC4, 0x24, 0x72, 0x49, 0x8C,
	0x90, 0x4B, 0x84, 0x34, 0x38, 0xAB, 0x78, 0xCA, 0x1F, 0x1,  0xD7, 0x93,
	0x11, 0xC1, 0x58, 0xA9, 0x31, 0xF9, 0x44, 0x6D, 0xBF, 0x33, 0x9C, 0x5F,
	0x9,  0x94, 0xA3, 0x85, 0x6,  0xC6, 0x9A, 0x1E, 0x7B, 0x46, 0x15, 0x30,
	0x27, 0x2B, 0x1B, 0x71, 0x3C, 0x5B, 0xD6, 0x6F, 0x62, 0xAC, 0x4F, 0xC2,
	0xC0, 0xE,  0xB1, 0x23, 0xA7, 0xDF, 0x47, 0xB0, 0x77, 0x69, 0x5,  0xE9,
	0xE6, 0xE7, 0x76, 0x73, 0xF,  0xFE, 0x6E, 0x9B, 0x56, 0xEF, 0x12, 0xA5,
	0x37, 0xFC, 0xAE, 0xD9, 0x3,  0x8E, 0xDD, 0x10, 0xB9, 0xCE, 0xC9, 0x8D,
	0xDA, 0x2A, 0xBD, 0x68, 0x17, 0x9F, 0xBE, 0xD4, 0xA,  0xCC, 0xD2, 0xE8,
	0x43, 0x3D, 0x70, 0xB7, 0x2,  0x7D, 0x99, 0xD8, 0xD,  0x60, 0x8A, 0x4,
	0x2C, 0x3E, 0x92, 0xE5, 0xAF, 0x53, 0x7,  0xE0, 0x29, 0xA6, 0xC5, 0xE3,
	0xF5, 0xF7, 0x4A, 0x41, 0x26, 0x6A, 0x16, 0x5E, 0x52, 0x2D, 0x21, 0xAD,
	0xF0, 0x91, 0xFF, 0xEA, 0x54, 0xFA, 0x66, 0x1A, 0x45, 0x39, 0xCF, 0x75,
	0xA4, 0x88, 0xFB, 0x5D, 0xA2, 0xA0, 0x19, 0x3B, 0xF8, 0xEB, 0xAA, 0xEE,
	0xF3, 0x1C, 0x67, 0x28, 0x1D, 0xED, 0x0,  0xDE, 0x95, 0x2E, 0xDC, 0x3F,
	0x3A, 0x82, 0x35, 0x4D, 0x6C, 0xBA, 0x36, 0xD0, 0xF6, 0xC,  0x79, 0x32,
	0xD1, 0x59, 0xF4, 0x8,  0x8B, 0x63, 0x89, 0x2F, 0xB8, 0xB4, 0x97, 0x83,
	0xF2, 0x8F, 0x18, 0xC7, 0x51, 0x14, 0x65, 0x87, 0x48, 0x20, 0x42, 0xA8,
	0x80, 0xB5, 0x40, 0x13, 0xB2, 0x22, 0x7E, 0x57, 0xBC, 0x7F, 0x6B, 0x9D,
	0x86, 0x4C, 0xC8, 0xDB, 0x7C, 0xD5, 0x25, 0x4E, 0x5A, 0x55, 0x74, 0x50,
	0xCD, 0xB3, 0x7A, 0xBB, 0xC3, 0xCB, 0xB6, 0xE2, 0xE4, 0xEC, 0xFD, 0x98,
	0xB,  0x96, 0xD3, 0x9E, 0x5C, 0xA1, 0x64, 0xF1, 0x81, 0x61, 0xE1, 0xC4,
	0x24, 0x72, 0x49, 0x8C, 0x90, 0x4B, 0x84, 0x34, 0x38, 0xAB, 0x78, 0xCA,
	0x1F, 0x1,  0xD7, 0x93, 0x11, 0xC1, 0x58, 0xA9, 0x31, 0xF9, 0x44, 0x6D,
	0xBF, 0x33, 0x9C, 0x5F, 0x9,  0x94, 0xA3, 0x85, 0x6,  0xC6, 0x9A, 0x1E,
	0x7B, 0x46, 0x15, 0x30, 0x27, 0x2B, 0x1B, 0x71, 0x3C, 0x5B, 0xD6, 0x6F,
	0x62, 0xAC, 0x4F, 0xC2, 0xC0, 0xE,  0xB1, 0x23, 0xA7, 0xDF, 0x47, 0xB0,
	0x77, 0x69, 0x5,  0xE9, 0xE6, 0xE7, 0x76, 0x73, 0xF,  0xFE, 0x6E, 0x9B,
	0x56, 0xEF, 0x12, 0xA5, 0x37, 0xFC, 0xAE, 0xD9, 0x3,  0x8E, 0xDD, 0x10,
	0xB9, 0xCE, 0xC9, 0x8D, 0xDA, 0x2A, 0xBD, 0x68, 0x17, 0x9F, 0xBE, 0xD4,
	0xA,  0xCC, 0xD2, 0xE8, 0x43, 0x3D, 0x70, 0xB7, 0x2,  0x7D, 0x99, 0xD8,
	0xD,  0x60, 0x8A, 0x4,  0x2C, 0x3E, 0x92, 0xE5, 0xAF, 0x53, 0x7,  0xE0,
	0x29, 0xA6, 0xC5, 0xE3, 0xF5, 0xF7, 0x4A, 0x41, 0x26, 0x6A, 0x16, 0x5E,
	0x52, 0x2D, 0x21, 0xAD, 0xF0, 0x91, 0xFF, 0xEA, 0x54, 0xFA, 0x66, 0x1A,
	0x45, 0x39, 0xCF, 0x75, 0xA4, 0x88, 0xFB, 0x5D, 0xA2, 0xA0
};


static const float g[512 + 2][3] = {
	{0.33783, 0.715698, -0.611206},
	{-0.944031, -0.326599, -0.045624},
	{-0.101074, -0.416443, -0.903503},
	{0.799286, 0.49411, -0.341949},
	{-0.854645, 0.518036, 0.033936},
	{0.42514, -0.437866, -0.792114},
	{-0.358948, 0.597046, 0.717377},
	{-0.985413, 0.144714, 0.089294},
	{-0.601776, -0.33728, -0.723907},
	{-0.449921, 0.594513, 0.666382},
	{0.208313, -0.10791, 0.972076},
	{0.575317, 0.060425, 0.815643},
	{0.293365, -0.875702, -0.383453},
	{0.293762, 0.465759, 0.834686},
	{-0.846008, -0.233398, -0.47934},
	{-0.115814, 0.143036, -0.98291},
	{0.204681, -0.949036, -0.239532},
	{0.946716, -0.263947, 0.184326},
	{-0.235596, 0.573822, 0.784332},
	{0.203705, -0.372253, -0.905487},
	{0.756989, -0.651031, 0.055298},
	{0.497803, 0.814697, -0.297363},
	{-0.16214, 0.063995, -0.98468},
	{-0.329254, 0.834381, 0.441925},
	{0.703827, -0.527039, -0.476227},
	{0.956421, 0.266113, 0.119781},
	{0.480133, 0.482849, 0.7323},
	{-0.18631, 0.961212, -0.203125},
	{-0.748474, -0.656921, -0.090393},
	{-0.085052, -0.165253, 0.982544},
	{-0.76947, 0.628174, -0.115234},
	{0.383148, 0.537659, 0.751068},
	{0.616486, -0.668488, -0.415924},
	{-0.259979, -0.630005, 0.73175},
	{0.570953, -0.087952, 0.816223},
	{-0.458008, 0.023254, 0.888611},
	{-0.196167, 0.976563, -0.088287},
	{-0.263885, -0.69812, -0.665527},
	{0.437134, -0.892273, -0.112793},
	{-0.621674, -0.230438, 0.748566},
	{0.232422, 0.900574, -0.367249},
	{0.22229, -0.796143, 0.562744},
	{-0.665497, -0.73764, 0.11377},
	{0.670135, 0.704803, 0.232605},
	{0.895599, 0.429749, -0.114655},
	{-0.11557, -0.474243, 0.872742},
	{0.621826, 0.604004, -0.498444},
	{-0.832214, 0.012756, 0.55426},
	{-0.702484, 0.705994, -0.089661},
	{-0.692017, 0.649292, 0.315399},
	{-0.175995, -0.977997, 0.111877},
	{0.096954, -0.04953, 0.994019},
	{0.635284, -0.606689, -0.477783},
	{-0.261261, -0.607422, -0.750153},
	{0.983276, 0.165436, 0.075958},
	{-0.29837, 0.404083, -0.864655},
	{-0.638672, 0.507721, 0.578156},
	{0.388214, 0.412079, 0.824249},
	{0.556183, -0.208832, 0.804352},
	{0.778442, 0.562012, 0.27951},
	{-0.616577, 0.781921, -0.091522},
	{0.196289, 0.051056, 0.979187},
	{-0.121216, 0.207153, -0.970734},
	{-0.173401, -0.384735, 0.906555},
	{0.161499, -0.723236, -0.671387},
	{0.178497, -0.006226, -0.983887},
	{-0.126038, 0.15799, 0.97934},
	{0.830475, -0.024811, 0.556458},
	{-0.510132, -0.76944, 0.384247},
	{0.81424, 0.200104, -0.544891},
	{-0.112549, -0.393311, -0.912445},
	{0.56189, 0.152222, -0.813049},
	{0.198914, -0.254517, -0.946381},
	{-0.41217, 0.690979, -0.593811},
	{-0.407257, 0.324524, 0.853668},
	{-0.690186, 0.366119, -0.624115},
	{-0.428345, 0.844147, -0.322296},
	{-0.21228, -0.297546, -0.930756},
	{-0.273071, 0.516113, 0.811798},
	{0.928314, 0.371643, 0.007233},
	{0.785828, -0.479218, -0.390778},
	{-0.704895, 0.058929, 0.706818},
	{0.173248, 0.203583, 0.963562},
	{0.422211, -0.904297, -0.062469},
	{-0.363312, -0.182465, 0.913605},
	{0.254028, -0.552307, -0.793945},
	{-0.28891, -0.765747, -0.574554},
	{0.058319, 0.291382, 0.954803},
	{0.946136, -0.303925, 0.111267},
	{-0.078156, 0.443695, -0.892731},
	{0.182098, 0.89389, 0.409515},
	{-0.680298, -0.213318, 0.701141},
	{0.062469, 0.848389, -0.525635},
	{-0.72879, -0.641846, 0.238342},
	{-0.88089, 0.427673, 0.202637},
	{-0.532501, -0.21405, 0.818878},
	{0.948975, -0.305084, 0.07962},
	{0.925446, 0.374664, 0.055817},
	{0.820923, 0.565491, 0.079102},
	{0.25882, 0.099792, -0.960724},
	{-0.294617, 0.910522, 0.289978},
	{0.137115, 0.320038, -0.937408},
	{-0.908386, 0.345276, -0.235718},
	{-0.936218, 0.138763, 0.322754},
	{0.366577, 0.925934, -0.090637},
	{0.309296, -0.686829, -0.657684},
	{0.66983, 0.024445, 0.742065},
	{-0.917999, -0.059113, -0.392059},
	{0.365509, 0.462158, -0.807922},
	{0.083374, 0.996399, -0.014801},
	{0.593842, 0.253143, -0.763672},
	{0.974976, -0.165466, 0.148285},
	{0.918976, 0.137299, 0.369537},
	{0.294952, 0.694977, 0.655731},
	{0.943085, 0.152618, -0.295319},
	{0.58783, -0.598236, 0.544495},
	{0.203796, 0.678223, 0.705994},
	{-0.478821, -0.661011, 0.577667},
	{0.719055, -0.1698, -0.673828},
	{-0.132172, -0.965332, 0.225006},
	{-0.981873, -0.14502, 0.121979},
	{0.763458, 0.579742, 0.284546},
	{-0.893188, 0.079681, 0.442474},
	{-0.795776, -0.523804, 0.303802},
	{0.734955, 0.67804, -0.007446},
	{0.15506, 0.986267, -0.056183},
	{0.258026, 0.571503, -0.778931},
	{-0.681549, -0.702087, -0.206116},
	{-0.96286, -0.177185, 0.203613},
	{-0.470978, -0.515106, 0.716095},
	{-0.740326, 0.57135, 0.354095},
	{-0.56012, -0.824982, -0.074982},
	{-0.507874, 0.753204, 0.417969},
	{-0.503113, 0.038147, 0.863342},
	{0.594025, 0.673553, -0.439758},
	{-0.119873, -0.005524, -0.992737},
	{0.098267, -0.213776, 0.971893},
	{-0.615631, 0.643951, 0.454163},
	{0.896851, -0.441071, 0.032166},
	{-0.555023, 0.750763, -0.358093},
	{0.398773, 0.304688, 0.864929},
	{-0.722961, 0.303589, 0.620544},
	{-0.63559, -0.621948, -0.457306},
	{-0.293243, 0.072327, 0.953278},
	{-0.491638, 0.661041, -0.566772},
	{-0.304199, -0.572083, -0.761688},
	{0.908081, -0.398956, 0.127014},
	{-0.523621, -0.549683, -0.650848},
	{-0.932922, -0.19986, 0.299408},
	{0.099426, 0.140869, 0.984985},
	{-0.020325, -0.999756, -0.002319},
	{0.952667, 0.280853, -0.11615},
	{-0.971893, 0.082581, 0.220337},
	{0.65921, 0.705292, -0.260651},
	{0.733063, -0.175537, 0.657043},
	{-0.555206, 0.429504, -0.712189},
	{0.400421, -0.89859, 0.179352},
	{0.750885, -0.19696, 0.630341},
	{0.785675, -0.569336, 0.241821},
	{-0.058899, -0.464111, 0.883789},
	{0.129608, -0.94519, 0.299622},
	{-0.357819, 0.907654, 0.219238},
	{-0.842133, -0.439117, -0.312927},
	{-0.313477, 0.84433, 0.434479},
	{-0.241211, 0.053253, 0.968994},
	{0.063873, 0.823273, 0.563965},
	{0.476288, 0.862152, -0.172516},
	{0.620941, -0.298126, 0.724915},
	{0.25238, -0.749359, -0.612122},
	{-0.577545, 0.386566, 0.718994},
	{-0.406342, -0.737976, 0.538696},
	{0.04718, 0.556305, 0.82959},
	{-0.802856, 0.587463, 0.101166},
	{-0.707733, -0.705963, 0.026428},
	{0.374908, 0.68457, 0.625092},
	{0.472137, 0.208405, -0.856506},
	{-0.703064, -0.581085, -0.409821},
	{-0.417206, -0.736328, 0.532623},
	{-0.447876, -0.20285, -0.870728},
	{0.086945, -0.990417, 0.107086},
	{0.183685, 0.018341, -0.982788},
	{0.560638, -0.428864, 0.708282},
	{0.296722, -0.952576, -0.0672},
	{0.135773, 0.990265, 0.030243},
	{-0.068787, 0.654724, 0.752686},
	{0.762604, -0.551758, 0.337585},
	{-0.819611, -0.407684, 0.402466},
	{-0.727844, -0.55072, -0.408539},
	{-0.855774, -0.480011, 0.19281},
	{0.693176, -0.079285, 0.716339},
	{0.226013, 0.650116, -0.725433},
	{0.246704, 0.953369, -0.173553},
	{-0.970398, -0.239227, -0.03244},
	{0.136383, -0.394318, 0.908752},
	{0.813232, 0.558167, 0.164368},
	{0.40451, 0.549042, -0.731323},
	{-0.380249, -0.566711, 0.730865},
	{0.022156, 0.932739, 0.359741},
	{0.00824, 0.996552, -0.082306},
	{0.956635, -0.065338, -0.283722},
	{-0.743561, 0.008209, 0.668579},
	{-0.859589, -0.509674, 0.035767},
	{-0.852234, 0.363678, -0.375977},
	{-0.201965, -0.970795, -0.12915},
	{0.313477, 0.947327, 0.06546},
	{-0.254028, -0.528259, 0.81015},
	{0.628052, 0.601105, 0.49411},
	{-0.494385, 0.868378, 0.037933},
	{0.275635, -0.086426, 0.957336},
	{-0.197937, 0.468903, -0.860748},
	{0.895599, 0.399384, 0.195801},
	{0.560791, 0.825012, -0.069214},
	{0.304199, -0.849487, 0.43103},
	{0.096375, 0.93576, 0.339111},
	{-0.051422, 0.408966, -0.911072},
	{0.330444, 0.942841, -0.042389},
	{-0.452362, -0.786407, 0.420563},
	{0.134308, -0.933472, -0.332489},
	{0.80191, -0.566711, -0.188934},
	{-0.987946, -0.105988, 0.112518},
	{-0.24408, 0.892242, -0.379791},
	{-0.920502, 0.229095, -0.316376},
	{0.7789, 0.325958, 0.535706},
	{-0.912872, 0.185211, -0.36377},
	{-0.184784, 0.565369, -0.803833},
	{-0.018463, 0.119537, 0.992615},
	{-0.259247, -0.935608, 0.239532},
	{-0.82373, -0.449127, -0.345947},
	{-0.433105, 0.659515, 0.614349},
	{-0.822754, 0.378845, -0.423676},
	{0.687195, -0.674835, -0.26889},
	{-0.246582, -0.800842, 0.545715},
	{-0.729187, -0.207794, 0.651978},
	{0.653534, -0.610443, -0.447388},
	{0.492584, -0.023346, 0.869934},
	{0.609039, 0.009094, -0.79306},
	{0.962494, -0.271088, -0.00885},
	{0.2659, -0.004913, 0.963959},
	{0.651245, 0.553619, -0.518951},
	{0.280548, -0.84314, 0.458618},
	{-0.175293, -0.983215, 0.049805},
	{0.035339, -0.979919, 0.196045},
	{-0.982941, 0.164307, -0.082245},
	{0.233734, -0.97226, -0.005005},
	{-0.747253, -0.611328, 0.260437},
	{0.645599, 0.592773, 0.481384},
	{0.117706, -0.949524, -0.29068},
	{-0.535004, -0.791901, -0.294312},
	{-0.627167, -0.214447, 0.748718},
	{-0.047974, -0.813477, -0.57959},
	{-0.175537, 0.477264, -0.860992},
	{0.738556, -0.414246, -0.53183},
	{0.562561, -0.704071, 0.433289},
	{-0.754944, 0.64801, -0.100586},
	{0.114716, 0.044525, -0.992371},
	{0.966003, 0.244873, -0.082764},
	{0.33783, 0.715698, -0.611206},
	{-0.944031, -0.326599, -0.045624},
	{-0.101074, -0.416443, -0.903503},
	{0.799286, 0.49411, -0.341949},
	{-0.854645, 0.518036, 0.033936},
	{0.42514, -0.437866, -0.792114},
	{-0.358948, 0.597046, 0.717377},
	{-0.985413, 0.144714, 0.089294},
	{-0.601776, -0.33728, -0.723907},
	{-0.449921, 0.594513, 0.666382},
	{0.208313, -0.10791, 0.972076},
	{0.575317, 0.060425, 0.815643},
	{0.293365, -0.875702, -0.383453},
	{0.293762, 0.465759, 0.834686},
	{-0.846008, -0.233398, -0.47934},
	{-0.115814, 0.143036, -0.98291},
	{0.204681, -0.949036, -0.239532},
	{0.946716, -0.263947, 0.184326},
	{-0.235596, 0.573822, 0.784332},
	{0.203705, -0.372253, -0.905487},
	{0.756989, -0.651031, 0.055298},
	{0.497803, 0.814697, -0.297363},
	{-0.16214, 0.063995, -0.98468},
	{-0.329254, 0.834381, 0.441925},
	{0.703827, -0.527039, -0.476227},
	{0.956421, 0.266113, 0.119781},
	{0.480133, 0.482849, 0.7323},
	{-0.18631, 0.961212, -0.203125},
	{-0.748474, -0.656921, -0.090393},
	{-0.085052, -0.165253, 0.982544},
	{-0.76947, 0.628174, -0.115234},
	{0.383148, 0.537659, 0.751068},
	{0.616486, -0.668488, -0.415924},
	{-0.259979, -0.630005, 0.73175},
	{0.570953, -0.087952, 0.816223},
	{-0.458008, 0.023254, 0.888611},
	{-0.196167, 0.976563, -0.088287},
	{-0.263885, -0.69812, -0.665527},
	{0.437134, -0.892273, -0.112793},
	{-0.621674, -0.230438, 0.748566},
	{0.232422, 0.900574, -0.367249},
	{0.22229, -0.796143, 0.562744},
	{-0.665497, -0.73764, 0.11377},
	{0.670135, 0.704803, 0.232605},
	{0.895599, 0.429749, -0.114655},
	{-0.11557, -0.474243, 0.872742},
	{0.621826, 0.604004, -0.498444},
	{-0.832214, 0.012756, 0.55426},
	{-0.702484, 0.705994, -0.089661},
	{-0.692017, 0.649292, 0.315399},
	{-0.175995, -0.977997, 0.111877},
	{0.096954, -0.04953, 0.994019},
	{0.635284, -0.606689, -0.477783},
	{-0.261261, -0.607422, -0.750153},
	{0.983276, 0.165436, 0.075958},
	{-0.29837, 0.404083, -0.864655},
	{-0.638672, 0.507721, 0.578156},
	{0.388214, 0.412079, 0.824249},
	{0.556183, -0.208832, 0.804352},
	{0.778442, 0.562012, 0.27951},
	{-0.616577, 0.781921, -0.091522},
	{0.196289, 0.051056, 0.979187},
	{-0.121216, 0.207153, -0.970734},
	{-0.173401, -0.384735, 0.906555},
	{0.161499, -0.723236, -0.671387},
	{0.178497, -0.006226, -0.983887},
	{-0.126038, 0.15799, 0.97934},
	{0.830475, -0.024811, 0.556458},
	{-0.510132, -0.76944, 0.384247},
	{0.81424, 0.200104, -0.544891},
	{-0.112549, -0.393311, -0.912445},
	{0.56189, 0.152222, -0.813049},
	{0.198914, -0.254517, -0.946381},
	{-0.41217, 0.690979, -0.593811},
	{-0.407257, 0.324524, 0.853668},
	{-0.690186, 0.366119, -0.624115},
	{-0.428345, 0.844147, -0.322296},
	{-0.21228, -0.297546, -0.930756},
	{-0.273071, 0.516113, 0.811798},
	{0.928314, 0.371643, 0.007233},
	{0.785828, -0.479218, -0.390778},
	{-0.704895, 0.058929, 0.706818},
	{0.173248, 0.203583, 0.963562},
	{0.422211, -0.904297, -0.062469},
	{-0.363312, -0.182465, 0.913605},
	{0.254028, -0.552307, -0.793945},
	{-0.28891, -0.765747, -0.574554},
	{0.058319, 0.291382, 0.954803},
	{0.946136, -0.303925, 0.111267},
	{-0.078156, 0.443695, -0.892731},
	{0.182098, 0.89389, 0.409515},
	{-0.680298, -0.213318, 0.701141},
	{0.062469, 0.848389, -0.525635},
	{-0.72879, -0.641846, 0.238342},
	{-0.88089, 0.427673, 0.202637},
	{-0.532501, -0.21405, 0.818878},
	{0.948975, -0.305084, 0.07962},
	{0.925446, 0.374664, 0.055817},
	{0.820923, 0.565491, 0.079102},
	{0.25882, 0.099792, -0.960724},
	{-0.294617, 0.910522, 0.289978},
	{0.137115, 0.320038, -0.937408},
	{-0.908386, 0.345276, -0.235718},
	{-0.936218, 0.138763, 0.322754},
	{0.366577, 0.925934, -0.090637},
	{0.309296, -0.686829, -0.657684},
	{0.66983, 0.024445, 0.742065},
	{-0.917999, -0.059113, -0.392059},
	{0.365509, 0.462158, -0.807922},
	{0.083374, 0.996399, -0.014801},
	{0.593842, 0.253143, -0.763672},
	{0.974976, -0.165466, 0.148285},
	{0.918976, 0.137299, 0.369537},
	{0.294952, 0.694977, 0.655731},
	{0.943085, 0.152618, -0.295319},
	{0.58783, -0.598236, 0.544495},
	{0.203796, 0.678223, 0.705994},
	{-0.478821, -0.661011, 0.577667},
	{0.719055, -0.1698, -0.673828},
	{-0.132172, -0.965332, 0.225006},
	{-0.981873, -0.14502, 0.121979},
	{0.763458, 0.579742, 0.284546},
	{-0.893188, 0.079681, 0.442474},
	{-0.795776, -0.523804, 0.303802},
	{0.734955, 0.67804, -0.007446},
	{0.15506, 0.986267, -0.056183},
	{0.258026, 0.571503, -0.778931},
	{-0.681549, -0.702087, -0.206116},
	{-0.96286, -0.177185, 0.203613},
	{-0.470978, -0.515106, 0.716095},
	{-0.740326, 0.57135, 0.354095},
	{-0.56012, -0.824982, -0.074982},
	{-0.507874, 0.753204, 0.417969},
	{-0.503113, 0.038147, 0.863342},
	{0.594025, 0.673553, -0.439758},
	{-0.119873, -0.005524, -0.992737},
	{0.098267, -0.213776, 0.971893},
	{-0.615631, 0.643951, 0.454163},
	{0.896851, -0.441071, 0.032166},
	{-0.555023, 0.750763, -0.358093},
	{0.398773, 0.304688, 0.864929},
	{-0.722961, 0.303589, 0.620544},
	{-0.63559, -0.621948, -0.457306},
	{-0.293243, 0.072327, 0.953278},
	{-0.491638, 0.661041, -0.566772},
	{-0.304199, -0.572083, -0.761688},
	{0.908081, -0.398956, 0.127014},
	{-0.523621, -0.549683, -0.650848},
	{-0.932922, -0.19986, 0.299408},
	{0.099426, 0.140869, 0.984985},
	{-0.020325, -0.999756, -0.002319},
	{0.952667, 0.280853, -0.11615},
	{-0.971893, 0.082581, 0.220337},
	{0.65921, 0.705292, -0.260651},
	{0.733063, -0.175537, 0.657043},
	{-0.555206, 0.429504, -0.712189},
	{0.400421, -0.89859, 0.179352},
	{0.750885, -0.19696, 0.630341},
	{0.785675, -0.569336, 0.241821},
	{-0.058899, -0.464111, 0.883789},
	{0.129608, -0.94519, 0.299622},
	{-0.357819, 0.907654, 0.219238},
	{-0.842133, -0.439117, -0.312927},
	{-0.313477, 0.84433, 0.434479},
	{-0.241211, 0.053253, 0.968994},
	{0.063873, 0.823273, 0.563965},
	{0.476288, 0.862152, -0.172516},
	{0.620941, -0.298126, 0.724915},
	{0.25238, -0.749359, -0.612122},
	{-0.577545, 0.386566, 0.718994},
	{-0.406342, -0.737976, 0.538696},
	{0.04718, 0.556305, 0.82959},
	{-0.802856, 0.587463, 0.101166},
	{-0.707733, -0.705963, 0.026428},
	{0.374908, 0.68457, 0.625092},
	{0.472137, 0.208405, -0.856506},
	{-0.703064, -0.581085, -0.409821},
	{-0.417206, -0.736328, 0.532623},
	{-0.447876, -0.20285, -0.870728},
	{0.086945, -0.990417, 0.107086},
	{0.183685, 0.018341, -0.982788},
	{0.560638, -0.428864, 0.708282},
	{0.296722, -0.952576, -0.0672},
	{0.135773, 0.990265, 0.030243},
	{-0.068787, 0.654724, 0.752686},
	{0.762604, -0.551758, 0.337585},
	{-0.819611, -0.407684, 0.402466},
	{-0.727844, -0.55072, -0.408539},
	{-0.855774, -0.480011, 0.19281},
	{0.693176, -0.079285, 0.716339},
	{0.226013, 0.650116, -0.725433},
	{0.246704, 0.953369, -0.173553},
	{-0.970398, -0.239227, -0.03244},
	{0.136383, -0.394318, 0.908752},
	{0.813232, 0.558167, 0.164368},
	{0.40451, 0.549042, -0.731323},
	{-0.380249, -0.566711, 0.730865},
	{0.022156, 0.932739, 0.359741},
	{0.00824, 0.996552, -0.082306},
	{0.956635, -0.065338, -0.283722},
	{-0.743561, 0.008209, 0.668579},
	{-0.859589, -0.509674, 0.035767},
	{-0.852234, 0.363678, -0.375977},
	{-0.201965, -0.970795, -0.12915},
	{0.313477, 0.947327, 0.06546},
	{-0.254028, -0.528259, 0.81015},
	{0.628052, 0.601105, 0.49411},
	{-0.494385, 0.868378, 0.037933},
	{0.275635, -0.086426, 0.957336},
	{-0.197937, 0.468903, -0.860748},
	{0.895599, 0.399384, 0.195801},
	{0.560791, 0.825012, -0.069214},
	{0.304199, -0.849487, 0.43103},
	{0.096375, 0.93576, 0.339111},
	{-0.051422, 0.408966, -0.911072},
	{0.330444, 0.942841, -0.042389},
	{-0.452362, -0.786407, 0.420563},
	{0.134308, -0.933472, -0.332489},
	{0.80191, -0.566711, -0.188934},
	{-0.987946, -0.105988, 0.112518},
	{-0.24408, 0.892242, -0.379791},
	{-0.920502, 0.229095, -0.316376},
	{0.7789, 0.325958, 0.535706},
	{-0.912872, 0.185211, -0.36377},
	{-0.184784, 0.565369, -0.803833},
	{-0.018463, 0.119537, 0.992615},
	{-0.259247, -0.935608, 0.239532},
	{-0.82373, -0.449127, -0.345947},
	{-0.433105, 0.659515, 0.614349},
	{-0.822754, 0.378845, -0.423676},
	{0.687195, -0.674835, -0.26889},
	{-0.246582, -0.800842, 0.545715},
	{-0.729187, -0.207794, 0.651978},
	{0.653534, -0.610443, -0.447388},
	{0.492584, -0.023346, 0.869934},
	{0.609039, 0.009094, -0.79306},
	{0.962494, -0.271088, -0.00885},
	{0.2659, -0.004913, 0.963959},
	{0.651245, 0.553619, -0.518951},
	{0.280548, -0.84314, 0.458618},
	{-0.175293, -0.983215, 0.049805},
	{0.035339, -0.979919, 0.196045},
	{-0.982941, 0.164307, -0.082245},
	{0.233734, -0.97226, -0.005005},
	{-0.747253, -0.611328, 0.260437},
	{0.645599, 0.592773, 0.481384},
	{0.117706, -0.949524, -0.29068},
	{-0.535004, -0.791901, -0.294312},
	{-0.627167, -0.214447, 0.748718},
	{-0.047974, -0.813477, -0.57959},
	{-0.175537, 0.477264, -0.860992},
	{0.738556, -0.414246, -0.53183},
	{0.562561, -0.704071, 0.433289},
	{-0.754944, 0.64801, -0.100586},
	{0.114716, 0.044525, -0.992371},
	{0.966003, 0.244873, -0.082764},
	{0.33783, 0.715698, -0.611206},
	{-0.944031, -0.326599, -0.045624},
};

#define SETUP(val, b0, b1, r0, r1)                                            \
	{                                                                         \
		t = val + 10000.0f;                                                   \
		b0 = ((int)t) & 255;                                                  \
		b1 = (b0 + 1) & 255;                                                  \
		r0 = t - floorf(t);                                                   \
		r1 = r0 - 1.0f;                                                       \
	} (void)0


static float noise3_perlin(float vec[3])
{
	int bx0, bx1, by0, by1, bz0, bz1, b00, b10, b01, b11;
	float rx0, rx1, ry0, ry1, rz0, rz1, sx, sy, sz, a, b, c, d, t, u, v;
	const float *q;
	register int i, j;


	SETUP(vec[0],  bx0, bx1,  rx0, rx1);
	SETUP(vec[1],  by0, by1,  ry0, ry1);
	SETUP(vec[2],  bz0, bz1,  rz0, rz1);

	i = p[bx0];
	j = p[bx1];

	b00 = p[i + by0];
	b10 = p[j + by0];
	b01 = p[i + by1];
	b11 = p[j + by1];

#define VALUE_AT(rx, ry, rz) (rx * q[0] + ry * q[1] + rz * q[2])
#define SURVE(t) (t * t * (3.0f - 2.0f * t))

/* lerp moved to improved perlin above */

	sx = SURVE(rx0);
	sy = SURVE(ry0);
	sz = SURVE(rz0);


	q = g[b00 + bz0];
	u = VALUE_AT(rx0, ry0, rz0);
	q = g[b10 + bz0];
	v = VALUE_AT(rx1, ry0, rz0);
	a = lerp(sx, u, v);

	q = g[b01 + bz0];
	u = VALUE_AT(rx0, ry1, rz0);
	q = g[b11 + bz0];
	v = VALUE_AT(rx1, ry1, rz0);
	b = lerp(sx, u, v);

	c = lerp(sy, a, b);          /* interpolate in y at lo x */

	q = g[b00 + bz1];
	u = VALUE_AT(rx0, ry0, rz1);
	q = g[b10 + bz1];
	v = VALUE_AT(rx1, ry0, rz1);
	a = lerp(sx, u, v);

	q = g[b01 + bz1];
	u = VALUE_AT(rx0, ry1, rz1);
	q = g[b11 + bz1];
	v = VALUE_AT(rx1, ry1, rz1);
	b = lerp(sx, u, v);

	d = lerp(sy, a, b);          /* interpolate in y at hi x */

	return 1.5f * lerp(sz, c, d); /* interpolate in z */

#undef VALUE_AT
#undef SURVE
}

#if 0
static float turbulence_perlin(const float point[3], float lofreq, float hifreq)
{
	float freq, t, p[3];

	p[0] = point[0] + 123.456;
	p[1] = point[1];
	p[2] = point[2];

	t = 0;
	for (freq = lofreq; freq < hifreq; freq *= 2.0) {
		t += fabsf(noise3_perlin(p)) / freq;
		p[0] *= 2.0f;
		p[1] *= 2.0f;
		p[2] *= 2.0f;
	}
	return t - 0.3; /* readjust to make mean value = 0.0 */
}
#endif

/* for use with BLI_gNoise/gTurbulence, returns signed noise */
static float orgPerlinNoise(float x, float y, float z)
{
	float v[3];

	v[0] = x;
	v[1] = y;
	v[2] = z;
	return noise3_perlin(v);
}

/* for use with BLI_gNoise/gTurbulence, returns unsigned noise */
static float orgPerlinNoiseU(float x, float y, float z)
{
	float v[3];

	v[0] = x;
	v[1] = y;
	v[2] = z;
	return (0.5f + 0.5f * noise3_perlin(v));
}

/* *************** CALL AS: *************** */

float BLI_hnoisep(float noisesize, float x, float y, float z)
{
	float vec[3];

	vec[0] = x / noisesize;
	vec[1] = y / noisesize;
	vec[2] = z / noisesize;

	return noise3_perlin(vec);
}

#if 0
static float turbulencep(float noisesize, float x, float y, float z, int nr)
{
	float vec[3];

	vec[0] = x / noisesize;
	vec[1] = y / noisesize;
	vec[2] = z / noisesize;
	nr++;
	return turbulence_perlin(vec, 1.0, (float)(1 << nr));
}
#endif

/******************/
/* VORONOI/WORLEY */
/******************/

/* distance metrics for voronoi, e parameter only used in Minkowski */
/* Camberra omitted, didn't seem useful */

/* distance squared */
static float dist_Squared(float x, float y, float z, float e)
{
	(void)e; return (x * x + y * y + z * z);
}
/* real distance */
static float dist_Real(float x, float y, float z, float e)
{
	(void)e; return sqrtf(x * x + y * y + z * z);
}
/* manhattan/taxicab/cityblock distance */
static float dist_Manhattan(float x, float y, float z, float e)
{
	(void)e; return (fabsf(x) + fabsf(y) + fabsf(z));
}
/* Chebychev */
static float dist_Chebychev(float x, float y, float z, float e)
{
	float t;
	(void)e;

	x = fabsf(x);
	y = fabsf(y);
	z = fabsf(z);
	t = (x > y) ? x : y;
	return ((z > t) ? z : t);
}

/* minkowski preset exponent 0.5 */
static float dist_MinkovskyH(float x, float y, float z, float e)
{
	float d = sqrtf(fabsf(x)) + sqrtf(fabsf(y)) + sqrtf(fabsf(z));
	(void)e;
	return (d * d);
}

/* minkowski preset exponent 4 */
static float dist_Minkovsky4(float x, float y, float z, float e)
{
	(void)e;
	x *= x;
	y *= y;
	z *= z;
	return sqrtf(sqrtf(x * x + y * y + z * z));
}

/* Minkowski, general case, slow, maybe too slow to be useful */
static float dist_Minkovsky(float x, float y, float z, float e)
{
	return powf(powf(fabsf(x), e) + powf(fabsf(y), e) + powf(fabsf(z), e), 1.0f / e);
}


/* Not 'pure' Worley, but the results are virtually the same.
 * Returns distances in da and point coords in pa */
void voronoi(float x, float y, float z, float *da, float *pa, float me, int dtype)
{
	int xx, yy, zz, xi, yi, zi;
	float xd, yd, zd, d;

	float (*distfunc)(float, float, float, float);
	switch (dtype) {
		case 1:
			distfunc = dist_Squared;
			break;
		case 2:
			distfunc = dist_Manhattan;
			break;
		case 3:
			distfunc = dist_Chebychev;
			break;
		case 4:
			distfunc = dist_MinkovskyH;
			break;
		case 5:
			distfunc = dist_Minkovsky4;
			break;
		case 6:
			distfunc = dist_Minkovsky;
			break;
		case 0:
		default:
			distfunc = dist_Real;
			break;
	}

	xi = (int)(floor(x));
	yi = (int)(floor(y));
	zi = (int)(floor(z));
	da[0] = da[1] = da[2] = da[3] = 1e10f;
	for (xx = xi - 1; xx <= xi + 1; xx++) {
		for (yy = yi - 1; yy <= yi + 1; yy++) {
			for (zz = zi - 1; zz <= zi + 1; zz++) {
				const float *p = HASHPNT(xx, yy, zz);
				xd = x - (p[0] + xx);
				yd = y - (p[1] + yy);
				zd = z - (p[2] + zz);
				d = distfunc(xd, yd, zd, me);
				if (d < da[0]) {
					da[3] = da[2];  da[2] = da[1];  da[1] = da[0];  da[0] = d;
					pa[9] = pa[6];  pa[10] = pa[7];  pa[11] = pa[8];
					pa[6] = pa[3];  pa[7] = pa[4];  pa[8] = pa[5];
					pa[3] = pa[0];  pa[4] = pa[1];  pa[5] = pa[2];
					pa[0] = p[0] + xx;  pa[1] = p[1] + yy;  pa[2] = p[2] + zz;
				}
				else if (d < da[1]) {
					da[3] = da[2];  da[2] = da[1];  da[1] = d;
					pa[9] = pa[6];  pa[10] = pa[7];  pa[11] = pa[8];
					pa[6] = pa[3];  pa[7] = pa[4];  pa[8] = pa[5];
					pa[3] = p[0] + xx;  pa[4] = p[1] + yy;  pa[5] = p[2] + zz;
				}
				else if (d < da[2]) {
					da[3] = da[2];  da[2] = d;
					pa[9] = pa[6];  pa[10] = pa[7];  pa[11] = pa[8];
					pa[6] = p[0] + xx;  pa[7] = p[1] + yy;  pa[8] = p[2] + zz;
				}
				else if (d < da[3]) {
					da[3] = d;
					pa[9] = p[0] + xx;  pa[10] = p[1] + yy;  pa[11] = p[2] + zz;
				}
			}
		}
	}
}

/* returns different feature points for use in BLI_gNoise() */
static float voronoi_F1(float x, float y, float z)
{
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1, 0);
	return da[0];
}

static float voronoi_F2(float x, float y, float z)
{
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1, 0);
	return da[1];
}

static float voronoi_F3(float x, float y, float z)
{
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1, 0);
	return da[2];
}

static float voronoi_F4(float x, float y, float z)
{
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1, 0);
	return da[3];
}

static float voronoi_F1F2(float x, float y, float z)
{
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1, 0);
	return (da[1] - da[0]);
}

/* Crackle type pattern, just a scale/clamp of F2-F1 */
static float voronoi_Cr(float x, float y, float z)
{
	float t = 10 * voronoi_F1F2(x, y, z);
	if (t > 1.f) return 1.f;
	return t;
}


/* Signed version of all 6 of the above, just 2x-1, not really correct though (range is potentially (0, sqrt(6)).
 * Used in the musgrave functions */
static float voronoi_F1S(float x, float y, float z)
{
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1, 0);
	return (2.0f * da[0] - 1.0f);
}

static float voronoi_F2S(float x, float y, float z)
{
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1, 0);
	return (2.0f * da[1] - 1.0f);
}

static float voronoi_F3S(float x, float y, float z)
{
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1, 0);
	return (2.0f * da[2] - 1.0f);
}

static float voronoi_F4S(float x, float y, float z)
{
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1, 0);
	return (2.0f * da[3] - 1.0f);
}

static float voronoi_F1F2S(float x, float y, float z)
{
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1, 0);
	return (2.0f * (da[1] - da[0]) - 1.0f);
}

/* Crackle type pattern, just a scale/clamp of F2-F1 */
static float voronoi_CrS(float x, float y, float z)
{
	float t = 10 * voronoi_F1F2(x, y, z);
	if (t > 1.f) return 1.f;
	return (2.0f * t - 1.0f);
}


/***************/
/* voronoi end */
/***************/

/*************/
/* CELLNOISE */
/*************/

/* returns unsigned cellnoise */
static float cellNoiseU(float x, float y, float z)
{
	int xi = (int)(floor(x));
	int yi = (int)(floor(y));
	int zi = (int)(floor(z));
	unsigned int n = xi + yi * 1301 + zi * 314159;
	n ^= (n << 13);
	return ((float)(n * (n * n * 15731 + 789221) + 1376312589) / 4294967296.0f);
}

/* idem, signed */
float cellNoise(float x, float y, float z)
{
	return (2.0f * cellNoiseU(x, y, z) - 1.0f);
}

/* returns a vector/point/color in ca, using point hasharray directly */
void cellNoiseV(float x, float y, float z, float ca[3])
{
	int xi = (int)(floor(x));
	int yi = (int)(floor(y));
	int zi = (int)(floor(z));
	const float *p = HASHPNT(xi, yi, zi);
	ca[0] = p[0];
	ca[1] = p[1];
	ca[2] = p[2];
}


/*****************/
/* end cellnoise */
/*****************/

/* newnoise: generic noise function for use with different noisebases */
float BLI_gNoise(float noisesize, float x, float y, float z, int hard, int noisebasis)
{
	float (*noisefunc)(float, float, float);

	switch (noisebasis) {
		case 1:
			noisefunc = orgPerlinNoiseU;
			break;
		case 2:
			noisefunc = newPerlinU;
			break;
		case 3:
			noisefunc = voronoi_F1;
			break;
		case 4:
			noisefunc = voronoi_F2;
			break;
		case 5:
			noisefunc = voronoi_F3;
			break;
		case 6:
			noisefunc = voronoi_F4;
			break;
		case 7:
			noisefunc = voronoi_F1F2;
			break;
		case 8:
			noisefunc = voronoi_Cr;
			break;
		case 14:
			noisefunc = cellNoiseU;
			break;
		case 0:
		default:
		{
			noisefunc = orgBlenderNoise;
			/* add one to make return value same as BLI_hnoise */
			x += 1;
			y += 1;
			z += 1;
			break;
		}
	}

	if (noisesize != 0.0f) {
		noisesize = 1.0f / noisesize;
		x *= noisesize;
		y *= noisesize;
		z *= noisesize;
	}
	
	if (hard) return fabsf(2.0f * noisefunc(x, y, z) - 1.0f);
	return noisefunc(x, y, z);
}

/* newnoise: generic turbulence function for use with different noisebasis */
float BLI_gTurbulence(float noisesize, float x, float y, float z, int oct, int hard, int noisebasis)
{
	float (*noisefunc)(float, float, float);
	float sum, t, amp = 1, fscale = 1;
	int i;
	
	switch (noisebasis) {
		case 1:
			noisefunc = orgPerlinNoiseU;
			break;
		case 2:
			noisefunc = newPerlinU;
			break;
		case 3:
			noisefunc = voronoi_F1;
			break;
		case 4:
			noisefunc = voronoi_F2;
			break;
		case 5:
			noisefunc = voronoi_F3;
			break;
		case 6:
			noisefunc = voronoi_F4;
			break;
		case 7:
			noisefunc = voronoi_F1F2;
			break;
		case 8:
			noisefunc = voronoi_Cr;
			break;
		case 14:
			noisefunc = cellNoiseU;
			break;
		case 0:
		default:
			noisefunc = orgBlenderNoise;
			x += 1;
			y += 1;
			z += 1;
			break;
	}

	if (noisesize != 0.0f) {
		noisesize = 1.0f / noisesize;
		x *= noisesize;
		y *= noisesize;
		z *= noisesize;
	}

	sum = 0;
	for (i = 0; i <= oct; i++, amp *= 0.5f, fscale *= 2.0f) {
		t = noisefunc(fscale * x, fscale * y, fscale * z);
		if (hard) t = fabsf(2.0f * t - 1.0f);
		sum += t * amp;
	}
	
	sum *= ((float)(1 << oct) / (float)((1 << (oct + 1)) - 1));

	return sum;

}


/*
 * The following code is based on Ken Musgrave's explanations and sample
 * source code in the book "Texturing and Modelling: A procedural approach"
 */

/*
 * Procedural fBm evaluated at "point"; returns value stored in "value".
 *
 * Parameters:
 *    ``H''  is the fractal increment parameter
 *    ``lacunarity''  is the gap between successive frequencies
 *    ``octaves''  is the number of frequencies in the fBm
 */
float mg_fBm(float x, float y, float z, float H, float lacunarity, float octaves, int noisebasis)
{
	float rmd, value = 0.0, pwr = 1.0, pwHL = powf(lacunarity, -H);
	int i;

	float (*noisefunc)(float, float, float);
	switch (noisebasis) {
		case 1:
			noisefunc = orgPerlinNoise;
			break;
		case 2:
			noisefunc = newPerlin;
			break;
		case 3:
			noisefunc = voronoi_F1S;
			break;
		case 4:
			noisefunc = voronoi_F2S;
			break;
		case 5:
			noisefunc = voronoi_F3S;
			break;
		case 6:
			noisefunc = voronoi_F4S;
			break;
		case 7:
			noisefunc = voronoi_F1F2S;
			break;
		case 8:
			noisefunc = voronoi_CrS;
			break;
		case 14:
			noisefunc = cellNoise;
			break;
		case 0:
		default:
		{
			noisefunc = orgBlenderNoiseS;
			break;
		}
	}
	
	for (i = 0; i < (int)octaves; i++) {
		value += noisefunc(x, y, z) * pwr;
		pwr *= pwHL;
		x *= lacunarity;
		y *= lacunarity;
		z *= lacunarity;
	}

	rmd = octaves - floorf(octaves);
	if (rmd != 0.f) value += rmd * noisefunc(x, y, z) * pwr;

	return value;

} /* fBm() */


/*
 * Procedural multifractal evaluated at "point";
 * returns value stored in "value".
 *
 * Parameters:
 *    ``H''  determines the highest fractal dimension
 *    ``lacunarity''  is gap between successive frequencies
 *    ``octaves''  is the number of frequencies in the fBm
 *    ``offset''  is the zero offset, which determines multifractality (NOT USED??)
 */

/* this one is in fact rather confusing,
 * there seem to be errors in the original source code (in all three versions of proc.text&mod),
 * I modified it to something that made sense to me, so it might be wrong... */
float mg_MultiFractal(float x, float y, float z, float H, float lacunarity, float octaves, int noisebasis)
{
	float rmd, value = 1.0, pwr = 1.0, pwHL = powf(lacunarity, -H);
	int i;

	float (*noisefunc)(float, float, float);
	switch (noisebasis) {
		case 1:
			noisefunc = orgPerlinNoise;
			break;
		case 2:
			noisefunc = newPerlin;
			break;
		case 3:
			noisefunc = voronoi_F1S;
			break;
		case 4:
			noisefunc = voronoi_F2S;
			break;
		case 5:
			noisefunc = voronoi_F3S;
			break;
		case 6:
			noisefunc = voronoi_F4S;
			break;
		case 7:
			noisefunc = voronoi_F1F2S;
			break;
		case 8:
			noisefunc = voronoi_CrS;
			break;
		case 14:
			noisefunc = cellNoise;
			break;
		case 0:
		default:
		{
			noisefunc = orgBlenderNoiseS;
			break;
		}
	}

	for (i = 0; i < (int)octaves; i++) {
		value *= (pwr * noisefunc(x, y, z) + 1.0f);
		pwr *= pwHL;
		x *= lacunarity;
		y *= lacunarity;
		z *= lacunarity;
	}
	rmd = octaves - floorf(octaves);
	if (rmd != 0.0f) value *= (rmd * noisefunc(x, y, z) * pwr + 1.0f);

	return value;

} /* multifractal() */

/*
 * Heterogeneous procedural terrain function: stats by altitude method.
 * Evaluated at "point"; returns value stored in "value".
 *
 * Parameters:
 *       ``H''  determines the fractal dimension of the roughest areas
 *       ``lacunarity''  is the gap between successive frequencies
 *       ``octaves''  is the number of frequencies in the fBm
 *       ``offset''  raises the terrain from `sea level'
 */
float mg_HeteroTerrain(float x, float y, float z, float H, float lacunarity, float octaves, float offset, int noisebasis)
{
	float value, increment, rmd;
	int i;
	float pwHL = powf(lacunarity, -H);
	float pwr = pwHL;   /* starts with i=1 instead of 0 */

	float (*noisefunc)(float, float, float);
	switch (noisebasis) {
		case 1:
			noisefunc = orgPerlinNoise;
			break;
		case 2:
			noisefunc = newPerlin;
			break;
		case 3:
			noisefunc = voronoi_F1S;
			break;
		case 4:
			noisefunc = voronoi_F2S;
			break;
		case 5:
			noisefunc = voronoi_F3S;
			break;
		case 6:
			noisefunc = voronoi_F4S;
			break;
		case 7:
			noisefunc = voronoi_F1F2S;
			break;
		case 8:
			noisefunc = voronoi_CrS;
			break;
		case 14:
			noisefunc = cellNoise;
			break;
		case 0:
		default:
		{
			noisefunc = orgBlenderNoiseS;
			break;
		}
	}

	/* first unscaled octave of function; later octaves are scaled */
	value = offset + noisefunc(x, y, z);
	x *= lacunarity;
	y *= lacunarity;
	z *= lacunarity;

	for (i = 1; i < (int)octaves; i++) {
		increment = (noisefunc(x, y, z) + offset) * pwr * value;
		value += increment;
		pwr *= pwHL;
		x *= lacunarity;
		y *= lacunarity;
		z *= lacunarity;
	}

	rmd = octaves - floorf(octaves);
	if (rmd != 0.0f) {
		increment = (noisefunc(x, y, z) + offset) * pwr * value;
		value += rmd * increment;
	}
	return value;
}


/* Hybrid additive/multiplicative multifractal terrain model.
 *
 * Some good parameter values to start with:
 *
 *      H:           0.25
 *      offset:      0.7
 */
float mg_HybridMultiFractal(float x, float y, float z, float H, float lacunarity, float octaves, float offset, float gain, int noisebasis)
{
	float result, signal, weight, rmd;
	int i;
	float pwHL = powf(lacunarity, -H);
	float pwr = pwHL;   /* starts with i=1 instead of 0 */
	float (*noisefunc)(float, float, float);

	switch (noisebasis) {
		case 1:
			noisefunc = orgPerlinNoise;
			break;
		case 2:
			noisefunc = newPerlin;
			break;
		case 3:
			noisefunc = voronoi_F1S;
			break;
		case 4:
			noisefunc = voronoi_F2S;
			break;
		case 5:
			noisefunc = voronoi_F3S;
			break;
		case 6:
			noisefunc = voronoi_F4S;
			break;
		case 7:
			noisefunc = voronoi_F1F2S;
			break;
		case 8:
			noisefunc = voronoi_CrS;
			break;
		case 14:
			noisefunc = cellNoise;
			break;
		case 0:
		default:
		{
			noisefunc = orgBlenderNoiseS;
			break;
		}
	}

	result = noisefunc(x, y, z) + offset;
	weight = gain * result;
	x *= lacunarity;
	y *= lacunarity;
	z *= lacunarity;

	for (i = 1; (weight > 0.001f) && (i < (int)octaves); i++) {
		if (weight > 1.0f) weight = 1.0f;
		signal = (noisefunc(x, y, z) + offset) * pwr;
		pwr *= pwHL;
		result += weight * signal;
		weight *= gain * signal;
		x *= lacunarity;
		y *= lacunarity;
		z *= lacunarity;
	}

	rmd = octaves - floorf(octaves);
	if (rmd != 0.f) result += rmd * ((noisefunc(x, y, z) + offset) * pwr);

	return result;

} /* HybridMultifractal() */


/* Ridged multifractal terrain model.
 *
 * Some good parameter values to start with:
 *
 *      H:           1.0
 *      offset:      1.0
 *      gain:        2.0
 */
float mg_RidgedMultiFractal(float x, float y, float z, float H, float lacunarity, float octaves, float offset, float gain, int noisebasis)
{
	float result, signal, weight;
	int i;
	float pwHL = powf(lacunarity, -H);
	float pwr = pwHL;   /* starts with i=1 instead of 0 */
	
	float (*noisefunc)(float, float, float);
	switch (noisebasis) {
		case 1:
			noisefunc = orgPerlinNoise;
			break;
		case 2:
			noisefunc = newPerlin;
			break;
		case 3:
			noisefunc = voronoi_F1S;
			break;
		case 4:
			noisefunc = voronoi_F2S;
			break;
		case 5:
			noisefunc = voronoi_F3S;
			break;
		case 6:
			noisefunc = voronoi_F4S;
			break;
		case 7:
			noisefunc = voronoi_F1F2S;
			break;
		case 8:
			noisefunc = voronoi_CrS;
			break;
		case 14:
			noisefunc = cellNoise;
			break;
		case 0:
		default:
		{
			noisefunc = orgBlenderNoiseS;
			break;
		}
	}

	signal = offset - fabsf(noisefunc(x, y, z));
	signal *= signal;
	result = signal;


	for (i = 1; i < (int)octaves; i++) {
		x *= lacunarity;
		y *= lacunarity;
		z *= lacunarity;
		weight = signal * gain;
		if      (weight > 1.0f) weight = 1.0f;
		else if (weight < 0.0f) weight = 0.0f;
		signal = offset - fabsf(noisefunc(x, y, z));
		signal *= signal;
		signal *= weight;
		result += signal * pwr;
		pwr *= pwHL;
	}

	return result;
} /* RidgedMultifractal() */

/* "Variable Lacunarity Noise"
 * A distorted variety of Perlin noise.
 */
float mg_VLNoise(float x, float y, float z, float distortion, int nbas1, int nbas2)
{
	float rv[3];
	float (*noisefunc1)(float, float, float);
	float (*noisefunc2)(float, float, float);

	switch (nbas1) {
		case 1:
			noisefunc1 = orgPerlinNoise;
			break;
		case 2:
			noisefunc1 = newPerlin;
			break;
		case 3:
			noisefunc1 = voronoi_F1S;
			break;
		case 4:
			noisefunc1 = voronoi_F2S;
			break;
		case 5:
			noisefunc1 = voronoi_F3S;
			break;
		case 6:
			noisefunc1 = voronoi_F4S;
			break;
		case 7:
			noisefunc1 = voronoi_F1F2S;
			break;
		case 8:
			noisefunc1 = voronoi_CrS;
			break;
		case 14:
			noisefunc1 = cellNoise;
			break;
		case 0:
		default:
		{
			noisefunc1 = orgBlenderNoiseS;
			break;
		}
	}

	switch (nbas2) {
		case 1:
			noisefunc2 = orgPerlinNoise;
			break;
		case 2:
			noisefunc2 = newPerlin;
			break;
		case 3:
			noisefunc2 = voronoi_F1S;
			break;
		case 4:
			noisefunc2 = voronoi_F2S;
			break;
		case 5:
			noisefunc2 = voronoi_F3S;
			break;
		case 6:
			noisefunc2 = voronoi_F4S;
			break;
		case 7:
			noisefunc2 = voronoi_F1F2S;
			break;
		case 8:
			noisefunc2 = voronoi_CrS;
			break;
		case 14:
			noisefunc2 = cellNoise;
			break;
		case 0:
		default:
		{
			noisefunc2 = orgBlenderNoiseS;
			break;
		}
	}

	/* get a random vector and scale the randomization */
	rv[0] = noisefunc1(x + 13.5f, y + 13.5f, z + 13.5f) * distortion;
	rv[1] = noisefunc1(x, y, z) * distortion;
	rv[2] = noisefunc1(x - 13.5f, y - 13.5f, z - 13.5f) * distortion;
	return noisefunc2(x + rv[0], y + rv[1], z + rv[2]);   /* distorted-domain noise */
}

/****************/
/* musgrave end */
/****************/
