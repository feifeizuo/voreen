/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2016 University of Muenster, Germany.                        *
 * Visualization and Computer Graphics Group <http://viscg.uni-muenster.de>        *
 * For a list of authors please refer to the file "CREDITS.txt".                   *
 *                                                                                 *
 * This file is part of the Voreen software package. Voreen is free software:      *
 * you can redistribute it and/or modify it under the terms of the GNU General     *
 * Public License version 2 as published by the Free Software Foundation.          *
 *                                                                                 *
 * Voreen is distributed in the hope that it will be useful, but WITHOUT ANY       *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR   *
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.      *
 *                                                                                 *
 * You should have received a copy of the GNU General Public License in the file   *
 * "LICENSE.txt" along with this file. If not, see <http://www.gnu.org/licenses/>. *
 *                                                                                 *
 * For non-commercial academic use see the license exception specified in the file *
 * "LICENSE-academic.txt". To get information about commercial licensing please    *
 * contact the authors.                                                            *
 *                                                                                 *
 ***********************************************************************************/

#version 150 core
#extension GL_ARB_explicit_attrib_location : enable

#include "modules/mod_transfunc.frag"


layout(location=0) in vec4 vert_position;
layout(location=2) in vec3 vert_normal;

uniform mat4 modelViewMatrixStack_;
uniform mat4 modelViewProjectionMatrixStack_;
uniform mat4 modelViewMatrixInverseStack_;
uniform float property_;

uniform TransFuncParameters transferFunc_;
uniform sampler1D transferFuncTex_;

out vec3 frag_EyePosition;
out vec3 frag_EyeNormal;
out vec4 frag_color;

/**
 * Transform the provided vertex data and pass it to the fragment shader.
 */
void main() {
    gl_Position = modelViewProjectionMatrixStack_ * vert_position;
    frag_EyePosition = (modelViewMatrixStack_ * vert_position).xyz;
    frag_EyeNormal = mat3(transpose(modelViewMatrixInverseStack_))* vert_normal;
    frag_color = vec4(applyTF(transferFunc_, transferFuncTex_, property_).rgb, 1);
}