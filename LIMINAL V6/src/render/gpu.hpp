// Direct3D-11-Schicht: Geraet, Swapchain, Ressourcen und Zustaende.
//
// Direct3D 11 (Feature Level 11_0) laeuft auf praktisch jedem Windows-10/11-PC
// ab etwa 2010 und braucht keine zusaetzliche Laufzeit. Ohne passende
// Grafikkarte kann auf WARP (Software) ausgewichen werden.
#pragma once

#include <string>
#include <vector>

#include "core/com_ptr.hpp"
#include "core/core.hpp"

#include <d3d11.h>
#include <dxgi1_2.h>

namespace lim::gfx {

using lim::Com;

struct Texture {
    Com<ID3D11Texture2D> tex;
    Com<ID3D11ShaderResourceView> srv;
    Com<ID3D11RenderTargetView> rtv;
    Com<ID3D11UnorderedAccessView> uav;
    Com<ID3D11DepthStencilView> dsv;
    std::vector<Com<ID3D11RenderTargetView>> mipRtv;
    std::vector<Com<ID3D11ShaderResourceView>> mipSrv;
    int width = 0, height = 0, mips = 1, layers = 1;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool valid() const { return (bool)tex; }
};

struct Buffer {
    Com<ID3D11Buffer> buf;
    Com<ID3D11ShaderResourceView> srv;
    Com<ID3D11UnorderedAccessView> uav;
    u32 size = 0, stride = 0, count = 0;
    bool valid() const { return (bool)buf; }
};

enum TexFlags : u32 {
    TEX_SRV = 1,
    TEX_RTV = 2,
    TEX_UAV = 4,
    TEX_DSV = 8,
    TEX_MIP_RTV = 16,  // RTV/SRV je Mip-Stufe (Bloom-Kette)
    TEX_GEN_MIPS = 32,
};

struct GpuInfo {
    std::string adapter;
    u64 dedicatedVram = 0;
    u64 sharedMemory = 0;      // V6: mitbenutzter Arbeitsspeicher (integrierte Grafik)
    u32 vendorId = 0, deviceId = 0;
    bool integrated = false;   // V6: Grafik im Prozessor (wenig eigener Speicher)
    bool warp = false;
    bool tearing = false;
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
};

class Gpu : NonCopyable {
public:
    ~Gpu();
    bool init(HWND hwnd, int width, int height, bool forceWarp, std::string& error);
    // V6: Grafikkarte bei mehreren (Laptop): false = leistungsstaerkste (Standard), true = sparsamste
    static inline bool preferLowPower = false;
    void resize(int width, int height);
    void present(bool vsync);

    ID3D11Device* dev() const { return dev_.get(); }
    ID3D11DeviceContext* ctx() const { return ctx_.get(); }
    ID3D11RenderTargetView* backbufferRtv() const { return backRtv_.get(); }
    ID3D11Texture2D* backbuffer() const { return backTex_.get(); }
    int width() const { return width_; }
    int height() const { return height_; }
    const GpuInfo& info() const { return info_; }

    // --- Ressourcen --------------------------------------------------------------------
    Texture createTexture(int w, int h, DXGI_FORMAT fmt, u32 flags, int mips = 1, int layers = 1,
                          const D3D11_SUBRESOURCE_DATA* init = nullptr, DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN);
    Texture createDepth(int w, int h);
    Buffer createConstant(u32 size);
    Buffer createStructured(u32 stride, u32 count, bool dynamic, bool uav, const void* init = nullptr);
    Buffer createVertex(const void* data, u32 size, u32 stride);
    Buffer createIndex(const void* data, u32 size);
    void update(const Buffer& b, const void* data, u32 size);  // dynamisch (Map/Discard)

    // --- Gemeinsame Zustaende ------------------------------------------------------------
    ID3D11SamplerState* samplerAniso() const { return sAniso_.get(); }
    ID3D11SamplerState* samplerLinearWrap() const { return sLinearWrap_.get(); }
    ID3D11SamplerState* samplerLinearClamp() const { return sLinearClamp_.get(); }
    ID3D11SamplerState* samplerPointClamp() const { return sPointClamp_.get(); }
    ID3D11RasterizerState* rsCullBack() const { return rsBack_.get(); }
    ID3D11RasterizerState* rsCullNone() const { return rsNone_.get(); }
    ID3D11RasterizerState* rsScissor() const { return rsScissor_.get(); }
    ID3D11DepthStencilState* dsGreater() const { return dsGreater_.get(); }  // umgekehrtes z
    ID3D11DepthStencilState* dsGreaterRead() const { return dsGreaterRead_.get(); }  // V6: testen, nicht schreiben
    ID3D11DepthStencilState* dsNone() const { return dsNone_.get(); }
    ID3D11BlendState* bsOpaque() const { return bsOpaque_.get(); }
    ID3D11BlendState* bsPremul() const { return bsPremul_.get(); }
    ID3D11BlendState* bsAdd() const { return bsAdd_.get(); }
    void setAnisotropy(int level);
    void bindCommonSamplers();

    // Hilfen
    void setViewport(int w, int h);
    void unbindAll();

private:
    void createBackbuffer();
    void createStates();

    Com<ID3D11Device> dev_;
    Com<ID3D11DeviceContext> ctx_;
    Com<IDXGISwapChain1> swap_;
    Com<ID3D11Texture2D> backTex_;
    Com<ID3D11RenderTargetView> backRtv_;
    Com<ID3D11SamplerState> sAniso_, sLinearWrap_, sLinearClamp_, sPointClamp_;
    Com<ID3D11RasterizerState> rsBack_, rsNone_, rsScissor_;
    Com<ID3D11DepthStencilState> dsGreater_, dsGreaterRead_, dsNone_;
    Com<ID3D11BlendState> bsOpaque_, bsPremul_, bsAdd_;
    GpuInfo info_;
    HWND hwnd_ = nullptr;
    int width_ = 0, height_ = 0;
    bool flipModel_ = false;
    UINT swapFlags_ = 0;
};

// Pruefen, ob ein HRESULT fehlgeschlagen ist (mit Protokolleintrag).
bool hrFailed(long hr, const char* what);

}  // namespace lim::gfx
