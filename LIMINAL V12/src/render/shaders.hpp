// Laedt die beim Build vorkompilierten Shader (shaders/*.cso) und erstellt die
// Eingabelayouts. Fehlende Dateien bedeuten eine unvollstaendige Installation.
#pragma once

#include <string>

#include "render/gpu.hpp"

namespace lim::gfx {

struct ShaderSet {
    Com<ID3D11VertexShader> worldVS, meshVS, fullscreenVS, uiVS, dustVS, skinVS, propVS;
    Com<ID3D11PixelShader> worldPS, meshPS, bloomDownPS, bloomUpPS, postPS, uiPS, dustPS, propPS;
    Com<ID3D11ComputeShader> lightingCS, ssaoCS, ssaoBlurCS, taaCS, exposureCS, ssrCS, volumetricCS;
    Com<ID3D11InputLayout> worldLayout, meshLayout, uiLayout, skinLayout, propLayout;  // V11: prop

    bool load(Gpu& gpu, std::string& error);
};

}  // namespace lim::gfx
