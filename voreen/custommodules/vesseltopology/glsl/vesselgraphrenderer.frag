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

// structs for light source and materials
struct LightSource {
    vec4 position_;        // light position eye space (w=0 -> directional)
    vec3 ambientColor_;    // ambient color (r,g,b)
    vec3 diffuseColor_;    // diffuse color (r,g,b)
    vec3 specularColor_;   // specular color (r,g,b)
    //vec3 attenuation_;     // attenuation (constant, linear, quadratic)
};

struct Material {
    vec3 ambientColor_;
    vec3 diffuseColor_;
    vec3 specularColor_;
    float shininess_;
};


// input varyings
in vec3 frag_EyePosition;
in vec3 frag_EyeNormal;
in vec4 frag_color;


// lighting uniforms
uniform bool lightingEnabled_;
uniform LightSource lightSource_;
uniform Material material_;

// output color
out vec4 color;


// ---------- functions for phong lighting ---------------------------- //

/**
 * Returns attenuation based on the currently set opengl values.
 * Incorporates constant, linear and quadratic attenuation.
 *
 * @param d Distance to the light source.
 */
/*float getAttenuation(in float d) {
    float att = 1.0 / (lightSource_.attenuation_.x +
                       lightSource_.attenuation_.y * d +
                       lightSource_.attenuation_.z * d * d);
    return min(att, 1.0);
}*/


/**
 * Returns the ambient term, considering the user defined lighting
 * parameters.
 *
 * @param ka The ambient color to be used, which is fetched from the
 * transfer function.
 */
vec3 getAmbientTerm(in vec3 ka) {
    return ka * lightSource_.ambientColor_;
}


/**
 * Returns the diffuse term, considering the user defined lighting
 * parameters.
 *
 * @param kd The diffuse color to be used, which is fetched from the
 * transfer function.
 * @param N The surface normal used for lambert shading.
 * @param L The normalized light vector used for lambert shading.
 */
vec3 getDiffuseTerm(in vec3 kd, in vec3 N, in vec3 L) {
    float NdotL = max(dot(N, L), 0.0);
    return kd * lightSource_.diffuseColor_ * NdotL;
}

/**
 * Returns the specular term, considering the user defined lighting
 * parameters.
 *
 * @param ks The specular material color to be used.
 * @param N The surface normal used.
 * @param L The normalized light vector used.
 * @param V The viewing vector used.
 * @param alpha The shininess coefficient used.
 */
vec3 getSpecularTerm(in vec3 ks, in vec3 N, in vec3 L, in vec3 V, in float alpha) {
    vec3 H = normalize(V + L);
    float NdotH = pow(max(dot(N, H), 0.0), alpha);
    return ks * lightSource_.specularColor_ * NdotH;
}

/**
 * Calculates phong shading in eye space by considering the uniforms.
 * Attenuation is applied, if the w component of the light position is 1.
 *
 * @param n Normal (does not need to be normalized) in eye space.
 * @param pos Position in eye space
 * @param ma Material ambient color
 * @param md Material diffuse color
 * @param ms Material specular color
 */
vec3 phongShading(in vec3 n, in vec3 pos, in vec3 ma, in vec3 md, in vec3 ms) {

    vec3 N = normalize(n);
    vec3 L = lightSource_.position_.xyz - pos;

    // we are in eye space
    vec3 V = normalize(-pos);

    // get light source distance for attenuation and normalize light vector
    float d = length(L);
    L /= d;

    vec3 shadedColor = vec3(0.0);
    shadedColor += getAmbientTerm(ma);
    shadedColor += getDiffuseTerm(md, N, L);
    shadedColor += getSpecularTerm(ms, N, L, V, material_.shininess_);
    //shadedColor *= getAttenuation(d);

    return shadedColor;
}

void main() {
    if (!lightingEnabled_) {
        color = frag_color;
    } else {
        vec3 ma = frag_color.rgb;
        vec3 md = frag_color.rgb;
        vec3 ms = frag_color.rgb;

        color = vec4(phongShading(frag_EyeNormal, frag_EyePosition, ma, md, ms), 1);
    }
}
