#include "render/gpu.hpp"

#include <format>

#include "core/log.hpp"
#include "core/paths.hpp"

#include <dxgi1_6.h>

namespace lim::gfx {

bool hrFailed(long hr, const char* what) {
    if (SUCCEEDED(hr)) return false;
    log::error("{} fehlgeschlagen (HRESULT 0x{:08X})", what, (unsigned)hr);
    return true;
}

Gpu::~Gpu() {
    if (ctx_) {
        ctx_->ClearState();
        ctx_->Flush();
    }
    if (swap_) swap_->SetFullscreenState(FALSE, nullptr);
}

bool Gpu::init(HWND hwnd, int width, int height, bool forceWarp, std::string& error) {
    hwnd_ = hwnd;
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
    // Debug-Schicht nur, wenn installiert (Windows "Grafiktools")
    if (LoadLibraryExW(L"d3d11sdklayers.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL got{};
    HRESULT hr = E_FAIL;
    if (!forceWarp) {
        // V6: Auf Laptops mit zwei Grafikchips die leistungsstaerkere GPU waehlen (sonst landet das Spiel
        // oft auf der integrierten). Ohne DXGI 1.6 (aelteres Windows) entscheidet das System.
        Com<IDXGIFactory6> f6;
        Com<IDXGIAdapter1> preferred;
        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)f6.put())) && f6 &&
            SUCCEEDED(f6->EnumAdapterByGpuPreference(0, preferLowPower ? DXGI_GPU_PREFERENCE_MINIMUM_POWER : DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter1),
                                                     (void**)preferred.put()))) {
            DXGI_ADAPTER_DESC1 pd{};
            preferred->GetDesc1(&pd);
            if (!(pd.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                hr = D3D11CreateDevice(preferred.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, levels, 2, D3D11_SDK_VERSION,
                                       dev_.put(), &got, ctx_.put());
                if (hr == E_INVALIDARG)
                    hr = D3D11CreateDevice(preferred.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, levels + 1, 1,
                                           D3D11_SDK_VERSION, dev_.put(), &got, ctx_.put());
            }
        }
    }
    if (!forceWarp && FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 2, D3D11_SDK_VERSION,
                               dev_.put(), &got, ctx_.put());
        if (hr == E_INVALIDARG)  // Windows 7 kennt 11_1 nicht
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels + 1, 1,
                                   D3D11_SDK_VERSION, dev_.put(), &got, ctx_.put());
    }
    if (FAILED(hr)) {
        if (!forceWarp) log::warn("Keine Grafikkarte mit Direct3D 11 gefunden - versuche WARP (Software)");
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels + 1, 1, D3D11_SDK_VERSION,
                               dev_.put(), &got, ctx_.put());
        info_.warp = SUCCEEDED(hr);
    }
    if (FAILED(hr)) {
        error = "Direct3D 11 konnte nicht initialisiert werden. Bitte den Grafiktreiber aktualisieren.";
        return false;
    }
    info_.featureLevel = got;

    Com<IDXGIDevice> dxgiDev;
    dev_->QueryInterface(__uuidof(IDXGIDevice), (void**)dxgiDev.put());
    Com<IDXGIAdapter> adapter;
    dxgiDev->GetAdapter(adapter.put());
    DXGI_ADAPTER_DESC ad{};
    adapter->GetDesc(&ad);
    info_.adapter = paths::narrow(ad.Description);
    info_.dedicatedVram = ad.DedicatedVideoMemory;
    info_.sharedMemory = ad.SharedSystemMemory;
    info_.vendorId = ad.VendorId;
    info_.deviceId = ad.DeviceId;
    // integriert: kaum eigener Grafikspeicher (Intel/AMD-APU teilen sich den Arbeitsspeicher)
    info_.integrated = !info_.warp && ad.DedicatedVideoMemory < (u64)768 * 1024 * 1024;
    log::info("Grafik: {} ({} MB eigener, {} MB geteilter Speicher{})", info_.adapter, ad.DedicatedVideoMemory >> 20,
              ad.SharedSystemMemory >> 20, info_.integrated ? ", integriert" : "");
    Com<IDXGIFactory2> factory;
    adapter->GetParent(__uuidof(IDXGIFactory2), (void**)factory.put());
    if (!factory) {
        error = "DXGI 1.2 fehlt (Windows 8 oder neuer mit aktuellem Grafiktreiber erforderlich).";
        return false;
    }
    Com<IDXGIFactory5> f5;
    factory->QueryInterface(__uuidof(IDXGIFactory5), (void**)f5.put());
    if (f5) {
        BOOL allow = FALSE;
        if (SUCCEEDED(f5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow))))
            info_.tearing = allow == TRUE;
    }

    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = (UINT)width_;
    sd.Height = (UINT)height_;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    // Flip-Modell (Windows 10) mit Tearing fuer ungebremste Bildraten, sonst klassisch
    const DXGI_SWAP_EFFECT effects[] = {DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
                                        DXGI_SWAP_EFFECT_DISCARD};
    for (auto e : effects) {
        sd.SwapEffect = e;
        bool flip = e != DXGI_SWAP_EFFECT_DISCARD;
        sd.BufferCount = flip ? 3 : 1;
        sd.Flags = (flip && info_.tearing) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
        hr = factory->CreateSwapChainForHwnd(dev_.get(), hwnd, &sd, nullptr, nullptr, swap_.put());
        if (SUCCEEDED(hr)) {
            flipModel_ = flip;
            swapFlags_ = sd.Flags;
            break;
        }
    }
    if (FAILED(hr)) {
        error = std::format("Swapchain konnte nicht erstellt werden (0x{:08X}).", (unsigned)hr);
        return false;
    }
    if (!flipModel_) info_.tearing = false;
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);  // Alt+Enter steuert das Fenster selbst
    createBackbuffer();
    createStates();
    log::info("Grafik: {} ({} MB VRAM), Feature Level {:X}, {}{}", info_.adapter, info_.dedicatedVram >> 20,
              (unsigned)got, flipModel_ ? "Flip-Modell" : "Blit-Modell", info_.tearing ? ", Tearing" : "");
    return true;
}

void Gpu::createBackbuffer() {
    backRtv_.reset();
    backTex_.reset();
    swap_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backTex_.put());
    D3D11_RENDER_TARGET_VIEW_DESC rd{};
    rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;  // Hardware wandelt linear -> sRGB
    rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    hrFailed(dev_->CreateRenderTargetView(backTex_.get(), &rd, backRtv_.put()), "Backbuffer-RTV");
}

void Gpu::resize(int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    if (width == width_ && height == height_) return;
    width_ = width;
    height_ = height;
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    backRtv_.reset();
    backTex_.reset();
    ctx_->Flush();
    hrFailed(swap_->ResizeBuffers(0, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN, swapFlags_), "ResizeBuffers");
    createBackbuffer();
}

void Gpu::present(bool vsync) {
    UINT flags = (!vsync && info_.tearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    HRESULT hr = swap_->Present(vsync ? 1 : 0, flags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        HRESULT reason = dev_->GetDeviceRemovedReason();
        LIM_CHECK(false, std::format("Die Grafikkarte wurde getrennt oder zurueckgesetzt (0x{:08X}).\n"
                                     "Bitte den Grafiktreiber aktualisieren.",
                                     (unsigned)reason));
    }
    if (flipModel_) {
        // Flip-Modell loest die Backbuffer-Bindung nach Present
        ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    }
}

void Gpu::createStates() {
    D3D11_SAMPLER_DESC s{};
    s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    s.MaxLOD = D3D11_FLOAT32_MAX;
    s.ComparisonFunc = D3D11_COMPARISON_NEVER;
    s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    dev_->CreateSamplerState(&s, sLinearWrap_.put());
    s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    dev_->CreateSamplerState(&s, sLinearClamp_.put());
    s.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    dev_->CreateSamplerState(&s, sPointClamp_.put());
    setAnisotropy(8);

    D3D11_RASTERIZER_DESC r{};
    r.FillMode = D3D11_FILL_SOLID;
    r.CullMode = D3D11_CULL_BACK;
    r.FrontCounterClockwise = FALSE;
    r.DepthClipEnable = TRUE;
    dev_->CreateRasterizerState(&r, rsBack_.put());
    r.CullMode = D3D11_CULL_NONE;
    dev_->CreateRasterizerState(&r, rsNone_.put());
    r.ScissorEnable = TRUE;
    dev_->CreateRasterizerState(&r, rsScissor_.put());

    D3D11_DEPTH_STENCIL_DESC d{};
    d.DepthEnable = TRUE;
    d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    d.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
    dev_->CreateDepthStencilState(&d, dsGreater_.put());
    d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dev_->CreateDepthStencilState(&d, dsGreaterRead_.put());
    d.DepthEnable = FALSE;
    d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dev_->CreateDepthStencilState(&d, dsNone_.put());

    D3D11_BLEND_DESC b{};
    b.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    for (int i = 1; i < 8; ++i) b.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dev_->CreateBlendState(&b, bsOpaque_.put());
    auto& rt = b.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_ONE;
    rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    dev_->CreateBlendState(&b, bsPremul_.put());
    rt.DestBlend = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_ONE;
    dev_->CreateBlendState(&b, bsAdd_.put());
}

void Gpu::setAnisotropy(int level) {
    D3D11_SAMPLER_DESC s{};
    s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    s.MaxLOD = D3D11_FLOAT32_MAX;
    s.MaxAnisotropy = (UINT)std::clamp(level, 1, 16);
    s.Filter = level > 1 ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    s.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sAniso_.reset();
    dev_->CreateSamplerState(&s, sAniso_.put());
}

void Gpu::bindCommonSamplers() {
    ID3D11SamplerState* s[4] = {sAniso_.get(), sLinearWrap_.get(), sLinearClamp_.get(), sPointClamp_.get()};
    ctx_->VSSetSamplers(0, 4, s);
    ctx_->PSSetSamplers(0, 4, s);
    ctx_->CSSetSamplers(0, 4, s);
}

void Gpu::setViewport(int w, int h) {
    D3D11_VIEWPORT vp{0, 0, (float)w, (float)h, 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
}

void Gpu::unbindAll() {
    ID3D11ShaderResourceView* nullSrv[16] = {};
    ID3D11UnorderedAccessView* nullUav[8] = {};
    ctx_->PSSetShaderResources(0, 16, nullSrv);
    ctx_->CSSetShaderResources(0, 16, nullSrv);
    ctx_->VSSetShaderResources(0, 16, nullSrv);
    ctx_->CSSetUnorderedAccessViews(0, 8, nullUav, nullptr);
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
}

// --- Ressourcen ---------------------------------------------------------------------------
Texture Gpu::createTexture(int w, int h, DXGI_FORMAT fmt, u32 flags, int mips, int layers,
                           const D3D11_SUBRESOURCE_DATA* init, DXGI_FORMAT viewFormat) {
    Texture t;
    t.width = w;
    t.height = h;
    t.mips = mips;
    t.layers = layers;
    t.format = fmt;
    D3D11_TEXTURE2D_DESC d{};
    d.Width = (UINT)w;
    d.Height = (UINT)h;
    d.MipLevels = (UINT)mips;
    d.ArraySize = (UINT)layers;
    d.Format = fmt;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    if (flags & TEX_SRV) d.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    if (flags & (TEX_RTV | TEX_MIP_RTV | TEX_GEN_MIPS)) d.BindFlags |= D3D11_BIND_RENDER_TARGET;
    if (flags & TEX_UAV) d.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    if (flags & TEX_DSV) d.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
    if (flags & TEX_GEN_MIPS) d.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
    if (hrFailed(dev_->CreateTexture2D(&d, init, t.tex.put()), "CreateTexture2D")) return t;
    DXGI_FORMAT vf = viewFormat != DXGI_FORMAT_UNKNOWN ? viewFormat : fmt;
    if (flags & TEX_SRV) {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = vf;
        if (layers > 1) {
            sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            sd.Texture2DArray.MipLevels = (UINT)mips;
            sd.Texture2DArray.ArraySize = (UINT)layers;
        } else {
            sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            sd.Texture2D.MipLevels = (UINT)mips;
        }
        hrFailed(dev_->CreateShaderResourceView(t.tex.get(), &sd, t.srv.put()), "SRV");
    }
    if (flags & TEX_RTV) {
        D3D11_RENDER_TARGET_VIEW_DESC rd{};
        rd.Format = vf;
        rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        hrFailed(dev_->CreateRenderTargetView(t.tex.get(), &rd, t.rtv.put()), "RTV");
    }
    if (flags & TEX_UAV) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};
        ud.Format = vf;
        ud.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        hrFailed(dev_->CreateUnorderedAccessView(t.tex.get(), &ud, t.uav.put()), "UAV");
    }
    if (flags & TEX_MIP_RTV) {
        for (int m = 0; m < mips; ++m) {
            D3D11_RENDER_TARGET_VIEW_DESC rd{};
            rd.Format = vf;
            rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rd.Texture2D.MipSlice = (UINT)m;
            Com<ID3D11RenderTargetView> r;
            dev_->CreateRenderTargetView(t.tex.get(), &rd, r.put());
            t.mipRtv.push_back(r);
            D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
            sd.Format = vf;
            sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            sd.Texture2D.MostDetailedMip = (UINT)m;
            sd.Texture2D.MipLevels = 1;
            Com<ID3D11ShaderResourceView> s;
            dev_->CreateShaderResourceView(t.tex.get(), &sd, s.put());
            t.mipSrv.push_back(s);
        }
    }
    return t;
}

Texture Gpu::createDepth(int w, int h) {
    Texture t;
    t.width = w;
    t.height = h;
    t.format = DXGI_FORMAT_R32_TYPELESS;
    D3D11_TEXTURE2D_DESC d{};
    d.Width = (UINT)w;
    d.Height = (UINT)h;
    d.MipLevels = 1;
    d.ArraySize = 1;
    d.Format = DXGI_FORMAT_R32_TYPELESS;
    d.SampleDesc.Count = 1;
    d.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    if (hrFailed(dev_->CreateTexture2D(&d, nullptr, t.tex.put()), "Tiefenpuffer")) return t;
    D3D11_DEPTH_STENCIL_VIEW_DESC dd{};
    dd.Format = DXGI_FORMAT_D32_FLOAT;
    dd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dev_->CreateDepthStencilView(t.tex.get(), &dd, t.dsv.put());
    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = DXGI_FORMAT_R32_FLOAT;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sd.Texture2D.MipLevels = 1;
    dev_->CreateShaderResourceView(t.tex.get(), &sd, t.srv.put());
    return t;
}

Buffer Gpu::createConstant(u32 size) {
    Buffer b;
    b.size = (size + 15) & ~15u;
    D3D11_BUFFER_DESC d{};
    d.ByteWidth = b.size;
    d.Usage = D3D11_USAGE_DYNAMIC;
    d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hrFailed(dev_->CreateBuffer(&d, nullptr, b.buf.put()), "Konstantenpuffer");
    return b;
}

Buffer Gpu::createStructured(u32 stride, u32 count, bool dynamic, bool uav, const void* init) {
    Buffer b;
    b.stride = stride;
    b.count = count;
    b.size = stride * count;
    D3D11_BUFFER_DESC d{};
    d.ByteWidth = b.size;
    d.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE | (uav ? D3D11_BIND_UNORDERED_ACCESS : 0);
    d.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
    d.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    d.StructureByteStride = stride;
    D3D11_SUBRESOURCE_DATA sd{init, 0, 0};
    if (hrFailed(dev_->CreateBuffer(&d, init ? &sd : nullptr, b.buf.put()), "StructuredBuffer")) return b;
    D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_UNKNOWN;
    srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srv.Buffer.NumElements = count;
    dev_->CreateShaderResourceView(b.buf.get(), &srv, b.srv.put());
    if (uav) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC u{};
        u.Format = DXGI_FORMAT_UNKNOWN;
        u.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        u.Buffer.NumElements = count;
        dev_->CreateUnorderedAccessView(b.buf.get(), &u, b.uav.put());
    }
    return b;
}

Buffer Gpu::createVertex(const void* data, u32 size, u32 stride) {
    Buffer b;
    b.size = size;
    b.stride = stride;
    D3D11_BUFFER_DESC d{};
    d.ByteWidth = size;
    d.Usage = D3D11_USAGE_IMMUTABLE;
    d.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd{data, 0, 0};
    hrFailed(dev_->CreateBuffer(&d, &sd, b.buf.put()), "Vertexpuffer");
    return b;
}

Buffer Gpu::createIndex(const void* data, u32 size) {
    Buffer b;
    b.size = size;
    D3D11_BUFFER_DESC d{};
    d.ByteWidth = size;
    d.Usage = D3D11_USAGE_IMMUTABLE;
    d.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd{data, 0, 0};
    hrFailed(dev_->CreateBuffer(&d, &sd, b.buf.put()), "Indexpuffer");
    return b;
}

void Gpu::update(const Buffer& b, const void* data, u32 size) {
    D3D11_MAPPED_SUBRESOURCE m;
    if (SUCCEEDED(ctx_->Map(b.buf.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        std::memcpy(m.pData, data, std::min(size, b.size));
        ctx_->Unmap(b.buf.get(), 0);
    }
}

}  // namespace lim::gfx
