/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2024 University of Muenster, Germany,                        *
 * Department of Computer Science.                                                 *
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

#version 330

out vec4 out_color;

in vec2 frag_coord;
in vec3 frag_vel;
in vec3 position;

uniform float alphaFactor_;
uniform vec3 pos;
uniform vec3 light;

uniform float scale_;
uniform float offset_;

void main(){
	if (dot(frag_coord, frag_coord) > 1)
		discard;

        float z= 1- sqrt(frag_coord.x*frag_coord.x + frag_coord.y*frag_coord.y) ;
	vec3 normal = vec3(frag_coord.x,frag_coord.y,z);


        float dcont=max(0.0,dot(normal,vec3(0,0,1)));
	vec3 diffuse=dcont*vec3(0.7,0.7,0.7);
	vec3 color =normalize(frag_vel);
	out_color = vec4(0.5*color+diffuse,alphaFactor_);
}
