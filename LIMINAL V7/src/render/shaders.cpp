#include "render/shaders.hpp"

#include "core/log.hpp"
#include "core/paths.hpp"

namespace lim::gfx {

namespace {

struct Loader {
    Gpu& gpu;
    std::string& error;
    bool ok = true;

    std::vector<unsigned char> blob(const char* name) {
        std::string path = paths::join(paths::shaderDir(), std::string(name) + ".cso");
        auto data = paths::readBinary(path);
        if (!data || data->empty()) {
            if (ok) error = "Shader fehlt: " + path + "\nBitte das Spiel vollstaendig neu installieren.";
            ok = false;
            return {};
        }
        return std::move(*data);
    }
    void vs(const char* name, Com<ID3D11VertexShader>& out, const D3D11_INPUT_ELEMENT_DESC* layout = nullptr,
            UINT count = 0, Com<ID3D11InputLayout>* il = nullptr) {
        auto b = blob(name);
        if (b.empty()) return;
        if (hrFailed(gpu.dev()->CreateVertexShader(b.data(), b.size(), nullptr, out.put()), name)) ok = false;
        if (layout && il && hrFailed(gpu.dev()->CreateInputLayout(layout, count, b.data(), b.size(), il->put()), name))
            ok = false;
    }
    void ps(const char* name, Com<ID3D11PixelShader>& out) {
        auto b = blob(name);
        if (!b.empty() && hrFailed(gpu.dev()->CreatePixelShader(b.data(), b.size(), nullptr, out.put()), name)) ok = false;
    }
    void cs(const char* name, Com<ID3D11ComputeShader>& out) {
        auto b = blob(name);
        if (!b.empty() && hrFailed(gpu.dev()->CreateComputeShader(b.data(), b.size(), nullptr, out.put()), name)) ok = false;
    }
};

}  // namespace

bool ShaderSet::load(Gpu& gpu, std::string& error) {
    Loader L{gpu, error};
    const D3D11_INPUT_ELEMENT_DESC world[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32_UINT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    const D3D11_INPUT_ELEMENT_DESC mesh[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    const D3D11_INPUT_ELEMENT_DESC ui[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    L.vs("world_gbuffer_VSMain", worldVS, world, 2, &worldLayout);
    L.ps("world_gbuffer_PSMain", worldPS);
    L.vs("mesh_gbuffer_VSMain", meshVS, mesh, 4, &meshLayout);
    L.ps("mesh_gbuffer_PSMain", meshPS);
    L.vs("fullscreen_VSMain", fullscreenVS);
    L.ps("bloom_DownPS", bloomDownPS);
    L.ps("bloom_UpPS", bloomUpPS);
    L.ps("post_PSMain", postPS);
    L.vs("ui_VSMain", uiVS, ui, 6, &uiLayout);
    L.ps("ui_PSMain", uiPS);
    L.cs("lighting_CSMain", lightingCS);
    L.cs("ssao_AoCS", ssaoCS);
    L.cs("ssao_BlurCS", ssaoBlurCS);
    L.cs("taa_CSMain", taaCS);
    L.cs("exposure_CSMain", exposureCS);
    L.cs("ssr_CSMain", ssrCS);
    L.cs("volumetric_CSMain", volumetricCS);  // V6: Lichtschaechte
    L.vs("particles_VSMain", dustVS);  // V6: Staub in der Luft
    L.ps("particles_PSMain", dustPS);
    if (L.ok) log::info("Shader geladen");
    return L.ok;
}

}  // namespace lim::gfx
