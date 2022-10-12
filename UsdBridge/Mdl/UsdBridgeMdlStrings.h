// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeMacros.h"

#ifdef CUSTOM_PBR_MDL 

static const char Mdl_PBRBase_string[] = R"matstrdelim(
/*****************************************************************************
*     Copyright 1986-2017 NVIDIA Corporation. All rights reserved.
******************************************************************************

 MDL MATERIALS ARE PROVIDED PURSUANT TO AN END USER LICENSE  AGREEMENT,
 WHICH WAS ACCEPTED IN ORDER TO GAIN ACCESS TO THIS FILE.  IN PARTICULAR,
 THE MDL MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL NVIDIA
 CORPORATION BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, INCLUDING
 ANY GENERAL, SPECIAL,  INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 THE USE OR INABILITY TO USE THE MDL MATERIALS OR FROM OTHER DEALINGS IN
 THE MDL MATERIALS.
*/


mdl 1.4;
import df::*;
import state::*;
import math::*;
import base::*;
import tex::*;
import anno::*;

import ::nvidia::core_definitions::file_texture;
import ::nvidia::core_definitions::normalmap_texture;

// -------------------- HELPER FUNCTIONS ----------------------

base::texture_return multiply_colors(
    color color_1 = color(1.0, 1.0, 1.0),
    color color_2 = color(.5, .5, .5),
  float weight  = 1.0
) [[
    anno::hidden()
]]
{
    return base::blend_color_layers(
                layers: base::color_layer[](
                    base::color_layer(
                        layer_color:    color_2,
                        weight:         weight,
                        mode:           base::color_layer_multiply
                        )),
                base: color_1
    );
}

float4 vertex_color_from_coordinate(int VertexColorCoordinateIndex)
[[
    anno::description("Vertex Color for float2 PrimVar"),
    anno::noinline()
]]
{
	// Kit only supports 4 uv sets, 2 uvs are available to vertex color. if vertex color index is invalid, output the constant WHITE color intead
	return (VertexColorCoordinateIndex > 2) ? float4(1.0f) : float4(state::texture_coordinate(VertexColorCoordinateIndex).x, state::texture_coordinate(VertexColorCoordinateIndex).y, state::texture_coordinate(VertexColorCoordinateIndex+1).x, state::texture_coordinate(VertexColorCoordinateIndex+1).y);
}

color get_translucent_tint(color base_color, float opacity)
[[
    anno::description("base color of Basic translucent"),
    anno::noinline()
]]
{
  return math::lerp(color(1.0), base_color, opacity);
}

tex::wrap_mode get_wrap_mode(int wrap_constant)
{
  tex::wrap_mode result = tex::wrap_clip;
  switch(wrap_constant)
  {
  case 1: result = tex::wrap_clamp; break;
  case 2: result = tex::wrap_repeat; break;
  case 3: result = tex::wrap_mirrored_repeat; break;
  default: break;
  }
  return result;
}

// -------------------- MATERIAL ----------------------

export material OmniPBR(

  color diffuse_color_constant = color(0.2f)
  [[
    anno::display_name("Base Color"),
    anno::description("This is the base color"),
    anno::in_group("Albedo")
  ]],
  uniform texture_2d diffuse_texture = texture_2d()
  [[
    anno::display_name("Albedo Map"),
    anno::in_group("Albedo")
  ]],
  float albedo_desaturation = float(0.0)
  [[
    anno::display_name("Albedo Desaturation"),
    anno::soft_range(float(0.0f), float(1.0f)),
    anno::description("Desaturates the diffuse color"),
    anno::in_group("Albedo")
  ]],
  float albedo_add = float(0.0)
  [[
    anno::display_name("Albedo Add"),
    anno::soft_range(float(-1.0f), float(1.0f)),
    anno::description("Adds a constant value to the diffuse color "),
    anno::in_group("Albedo")
  ]],
  float albedo_brightness = float(1.0)
  [[
    anno::display_name("Albedo Brightness"),
    anno::soft_range(float(0.0f), float(1.0f)),
    anno::description("Multiplier for the diffuse color "),
    anno::in_group("Albedo")
  ]],
  color diffuse_tint = color(1.0f)
  [[
    anno::display_name("Color Tint"),
    anno::description("When enabled, this color value is multiplied over the final albedo color"),
    anno::in_group("Albedo")
  ]],


  // -------------------- REFLECTIVITY/METALLIC ----------------------
  float reflection_roughness_constant = 0.5f
  [[
    anno::display_name("Roughness Amount"),
    anno::hard_range(0.0,1.),
    anno::description("Higher roughness values lead to more blurry reflections"),
    anno::in_group("Reflectivity")
  ]],
  float reflection_roughness_texture_influence = 0.0f
  [[
    anno::display_name("Roughness Map Influence"),
    anno::hard_range(0.0, 1.),
    anno::description("Blends between the constant value and the lookup of the roughness texture"),
    anno::in_group("Reflectivity")
  ]],
  uniform texture_2d reflection_roughness_texture = texture_2d()
  [[
    anno::display_name("Roughness Map"),
    anno::in_group("Reflectivity")
  ]],

  float metallic_constant = 0.f
  [[
    anno::display_name("Metallic Amount"),
    anno::hard_range(0.0,1.),
    anno::description("Metallic Material"),
    anno::in_group("Reflectivity")
  ]],
  float metallic_texture_influence = 0.0f
  [[
    anno::display_name("Metallic Map Influence"),
    anno::hard_range(0.0, 1.),
    anno::description("Blends between the constant value and the lookup of the metallic texture"),
    anno::in_group("Reflectivity")
  ]],
  uniform texture_2d metallic_texture = texture_2d()
  [[
    anno::display_name("Metallic Map"),
    anno::in_group("Reflectivity")
  ]],
  float specular_level = float(0.5)
  [[
    anno::display_name("Specular"),
    anno::soft_range(float(0.0f), float(1.0f)),
    anno::description("The specular level (intensity) of the material"),
    anno::in_group("Reflectivity")
  ]],


  // -------------------- EMISSIVE ----------------------
  uniform bool enable_emission = false
  [[
    anno::display_name("Enable Emission"),
    anno::description("Enables the emission of light from the material"),
    anno::in_group("Emissive")
  ]],

  color emissive_color = color(1.0, 0.1, 0.1)
  [[
    anno::enable_if("enable_emission == true"),
    anno::display_name("Emissive Color"),
    anno::description("The emission color"),
    anno::in_group("Emissive")

  ]],
  uniform texture_2d emissive_mask_texture    = texture_2d()
  [[
    anno::enable_if("enable_emission == true"),
    anno::display_name("Emissive Mask map"),
    anno::description("The texture masking the emissive color"),
    anno::in_group("Emissive")
  ]],
  uniform float emissive_intensity = 40.f
  [[
    anno::enable_if("enable_emission == true"),
    anno::display_name("Emissive Intensity"),
    anno::description("Intensity of the emission"),
    anno::in_group("Emissive")
  ]],

  // Opacity Map
  uniform bool enable_opacity_texture = false
  [[
    anno::display_name("Enable Opacity Texture"),
    anno::description("Enables or disbales the usage of the opacity texture map"),
    anno::in_group("Opacity")
  ]],
  uniform float opacity_constant = 1.0f
  [[
    anno::enable_if("enable_opacity_texture == true"),
    anno::hard_range(0.0, 1.0),
    anno::display_name("Opacity Amount"),
    anno::description("Amount of Opacity"),
    anno::in_group("Opacity")
  ]],
  uniform texture_2d opacity_texture = texture_2d()
  [[
    anno::enable_if("enable_opacity_texture==true"),
    anno::display_name("Opacity Map"),
    anno::in_group("Opacity")

  ]],
  uniform float ior_constant = 1.f
  [[
    anno::enable_if("opacity_constant < 1.0"),
    anno::display_name("Index of Refraction"),
    anno::description("Index of Refraction for transparent materials"),
    anno::in_group("Opacity")
  ]],

  // -------------------- Normal ----------------------
  uniform float bump_factor = 1.f
  [[
    anno::display_name("Normal Map Strength"),
    anno::description("Strength of normal map."),
      anno::in_group("Normal")
  ]],
  uniform texture_2d normalmap_texture = texture_2d()
  [[
    anno::display_name("Normal Map"),
    anno::description("Enables the usage of the normalmap texture"),
    anno::in_group("Normal")
  ]],

  // -------------------- Vertex Colors ----------------------
  uniform int vertexcolor_coordinate_index = -1
  [[
    anno::display_name("Vertex Color Texture Coordinate Index"),
    anno::description("Index for texture coordinate with vertex colors"),
    anno::in_group("VertexColor")
  ]],

  // -------------------- Wrapping ----------------------
  uniform int diffuse_wrapmode_u = 2
  [[
    anno::display_name("Diffuse Wrapping Mode U"),
    anno::description("Wrapping mode along u axis of diffuse_texture, see get_wrap_mode"),
    anno::in_group("Texture")
  ]],
  
  uniform int diffuse_wrapmode_v = 2
  [[
    anno::display_name("Diffuse Wrapping Mode V"),
    anno::description("Wrapping mode along v axis of diffuse_texture, see get_wrap_mode"),
    anno::in_group("Texture")
  ]]
)
[[
  anno::display_name("PBR Opacity"),
  anno::description("PBR material, supports opacity"),
  anno::version( 1, 0, 0),
  anno::author("NVIDIA CORPORATION"),
  anno::key_words(string[]("PBR", "generic"))
]]

 = let{
  
  // -------------------- Texcoord --------------------
  int uv_space_index = 0;
  base::texture_coordinate_info uvw = base::coordinate_source(
                        coordinate_system: base::texture_coordinate_uvw,
                        texture_space: uv_space_index );

  base::texture_coordinate_info  transformed_uvw =  uvw;

  // -------------------- Base texture --------------------
  
  base::texture_return base_lookup = base::file_texture(
              texture: diffuse_texture,
              color_offset: color(albedo_add),
              color_scale: color(albedo_brightness),
              mono_source: base::mono_luminance,
              uvw: transformed_uvw,
              wrap_u: get_wrap_mode(diffuse_wrapmode_u),
              wrap_v: get_wrap_mode(diffuse_wrapmode_v),
              clip: false);

  float alpha = tex::texture_isvalid(diffuse_texture) ? base::file_texture(
                  texture: diffuse_texture,
                  mono_source: base::mono_alpha,
                  uvw: transformed_uvw,
                  wrap_u: get_wrap_mode(diffuse_wrapmode_u),
                  wrap_v: get_wrap_mode(diffuse_wrapmode_v),
                  clip: false).mono 
                : 1.0;

  // -------------------- Rougness/Reflection --------------------
  
  base::texture_return roughness_lookup = base::file_texture(
              texture: reflection_roughness_texture,
              mono_source: base::mono_average,
              uvw: transformed_uvw,
              clip: false
  );

  float roughness_selection = roughness_lookup.mono;

  float reflection_roughness_1 =  math::lerp(reflection_roughness_constant, roughness_selection, reflection_roughness_texture_influence);

  color desaturated_base = math::lerp(base_lookup.tint, color(base_lookup.mono), albedo_desaturation);
  
  // -------------------- Metallic --------------------
  
  base::texture_return metallic_lookup = base::file_texture(
            texture: metallic_texture,
            color_offset: color(0.0, 0.0, 0.0),
            color_scale: color(1.0, 1.0, 1.0),
            mono_source: base::mono_average,
            uvw: transformed_uvw,
            clip: false
  );

  // Choose between ORM or metallic map
  float metallic_selection = metallic_lookup.mono;

  // blend between the constant metallic value and the map lookup
  float metallic =  math::lerp(metallic_constant, metallic_selection, metallic_texture_influence);
  
  // -------------------- Emissive --------------------
  
  color emissive_mask = tex::texture_isvalid(emissive_mask_texture) ?
                          base::file_texture(
                            texture: emissive_mask_texture,
                            color_offset: color(0.0, 0.0, 0.0),
                            color_scale: color(1.0, 1.0, 1.0),
                            mono_source: base::mono_average,
                            uvw: transformed_uvw,
                            clip: false).tint 
                          : color(1.0);

  // -------------------- Normal --------------------
  
  float3 the_normal =  tex::texture_isvalid(normalmap_texture) ?
              base::tangent_space_normal_texture(
              texture:        normalmap_texture,
              factor:         bump_factor,
              uvw:            transformed_uvw
              //flip_tangent_u: false,
              //flip_tangent_v: true
              ) : state::normal() ;

  float3 final_normal = the_normal;

  // -------------------- Opacity --------------------

  float final_opacity = enable_opacity_texture ? 
                          base::file_texture(
                            texture:   opacity_texture,
                            mono_source: base::mono_average,
                            uvw:     transformed_uvw ).mono 
                          : (opacity_constant > 0 ? opacity_constant*alpha : alpha);
  
  // -------------------- Establish base color --------------------
  
  float4 vcData = vertex_color_from_coordinate(vertexcolor_coordinate_index);

  color diffuse_color =  tex::texture_isvalid(diffuse_texture) ? desaturated_base : 
    ((vertexcolor_coordinate_index >= 0 && vertexcolor_coordinate_index < state::texture_space_max()) ?  color(vcData.x, vcData.y, vcData.z) : diffuse_color_constant);
  
  color tinted_diffuse_color = multiply_colors(diffuse_color, diffuse_tint, 1.0).tint;

  color final_base_color = tinted_diffuse_color;

  // -------------------- Construct bsdf --------------------

  float reflection_roughness_sq = reflection_roughness_1*reflection_roughness_1;
)matstrdelim";

static const char Mdl_PBRBase_string_opaque[] = R"matstrdelim(
  bsdf diffuse_bsdf = df::diffuse_reflection_bsdf(
    tint: final_base_color,
    roughness: 0.f );

  bsdf ggx_smith_bsdf = df::microfacet_ggx_smith_bsdf(
    roughness_u: reflection_roughness_sq,
    roughness_v: reflection_roughness_sq,
    tint: color(1.0, 1.0, 1.0),
    mode: df::scatter_reflect );

  bsdf custom_curve_layer_bsdf = df::custom_curve_layer(
    normal_reflectivity: 0.08,
    grazing_reflectivity: 1.0,
    exponent: 5.0,
    weight:   specular_level,
    layer: ggx_smith_bsdf,
    base: diffuse_bsdf );

  bsdf directional_factor_bsdf = df::directional_factor(
    normal_tint:  final_base_color,
    grazing_tint: final_base_color,
    exponent: 3.0f,
    base: ggx_smith_bsdf );

  bsdf final_bsdf = df::weighted_layer(
    weight: metallic,
    layer:  directional_factor_bsdf,
    base: custom_curve_layer_bsdf );
    
    
} in material(
    surface: material_surface(
      scattering: final_bsdf,
      emission:  material_emission (
        df::diffuse_edf(),
        intensity: enable_emission? emissive_color * emissive_mask * color(emissive_intensity) : color(0)
        )
    ),
    geometry: material_geometry(
      normal: final_normal,
      cutout_opacity: final_opacity
    )
);
)matstrdelim";

static const char Mdl_PBRBase_string_translucent[] = R"matstrdelim(
  bsdf ggx_smith_bsdf = df::microfacet_ggx_smith_bsdf(
    roughness_u: reflection_roughness_sq,
    roughness_v: reflection_roughness_sq,
    tint: get_translucent_tint(base_color: final_base_color, opacity: final_opacity),
    mode: df::scatter_reflect_transmit
    );

  bsdf directional_factor_bsdf = df::directional_factor(
    normal_tint:  final_base_color,
    grazing_tint: final_base_color,    
    exponent: 3.0f,
    base: ggx_smith_bsdf );

  bsdf final_bsdf = df::weighted_layer(
    weight: metallic,
    layer:  directional_factor_bsdf,
    base: ggx_smith_bsdf );

} in material(
    thin_walled: true,
    ior: color(ior_constant), 
    surface: material_surface(
      scattering: final_bsdf,
      emission:  material_emission (
        df::diffuse_edf(),
        intensity: enable_emission? emissive_color * emissive_mask * color(emissive_intensity) : color(0)
        )
    ),
    geometry: material_geometry(
      normal: final_normal,
      cutout_opacity: 1.0
    )
);
)matstrdelim";

#endif
