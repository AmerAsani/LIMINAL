#include "render/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/jobs.hpp"
#include "core/log.hpp"
#include "platform/window.hpp"
#include "world/chunk.hpp"

namespace lim::gfx {

struct Renderer::TexJob {
    std::atomic<bool> done{false};
    TextureSet set;
    int size = 1024;
};

namespace {
float halton(int i, int b) {
    float f = 1.0f, r = 0.0f;
    while (i > 0) {
        f /= (float)b;
        r += f * (float)(i % b);
        i /= b;
    }
    return r;
}
constexpr int kBloomLevels = 6;
}  // namespace

Renderer::Renderer() = default;
Renderer::~Renderer() {
    if (jobs_ && texJob_ && !texJob_->done) jobs_->waitIdle();
}

bool Renderer::init(plat::Window& window, JobSystem& jobs, const RenderSettings& s, bool forceWarp, std::string& error) {
    jobs_ = &jobs;
    settings_ = s;
    if (!gpu_.init(window.hwnd(), window.width(), window.height(), forceWarp, error)) return false;
    if (!shaders_.load(gpu_, error)) return false;
    font_.build(48);
    if (!ui_.init(gpu_, shaders_, font_)) {
        error = "Oberflaeche konnte nicht initialisiert werden.";
        return false;
    }
    if (!world_.init(gpu_, 256)) {
        error = "Weltpuffer konnten nicht angelegt werden.";
        return false;
    }
    meshes_.init(gpu_);
    frameCb_ = gpu_.createConstant(sizeof(FrameConstants));
    passCb_ = gpu_.createConstant(sizeof(PassConstants));
    objectCb_ = gpu_.createConstant(sizeof(ObjectConstants));
    layerCb_ = gpu_.createConstant(sizeof(vec4) * 32);
    float one[2] = {0.0f, 0.0f};
    exposure_ = gpu_.createStructured(sizeof(float), 2, false, true, one);

    // Ebenen-Konstanten (Massstab, Normalstaerke, Rauheit)
    vec4 layers[32]{};
    for (int i = 0; i < L_COUNT; ++i) layers[i] = {kLayers[i].meters, kLayers[i].normalStrength, kLayers[i].roughBias, 0};
    gpu_.update(layerCb_, layers, sizeof(layers));

    // Variationsrauschen und Zeichenstreifen (Retro-Darstellung)
    {
        auto macro = generateMacroNoise(256);
        macro_ = gpu_.createTexture(256, 256, DXGI_FORMAT_R8G8B8A8_UNORM, TEX_SRV | TEX_GEN_MIPS, 9);
        gpu_.ctx()->UpdateSubresource(macro_.tex.get(), 0, nullptr, macro.data(), 256 * 4, 0);
        gpu_.ctx()->GenerateMips(macro_.srv.get());
        int gw, gh;
        auto strip = font_.buildGlyphStrip(gw, gh);
        D3D11_SUBRESOURCE_DATA sd{strip.data(), (UINT)gw, 0};
        glyphs_ = gpu_.createTexture(gw, gh, DXGI_FORMAT_R8_UNORM, TEX_SRV, 1, 1, &sd);
    }
    gpu_.setAnisotropy(settings_.anisotropy);
    createTargets(gpu_.width(), gpu_.height());
    return true;
}

void Renderer::applySettings(const RenderSettings& s) {
    bool rescale = s.renderScale != settings_.renderScale;
    bool retex = s.textureSize != settings_.textureSize;
    bool aniso = s.anisotropy != settings_.anisotropy;
    settings_ = s;
    if (aniso) gpu_.setAnisotropy(s.anisotropy);
    if (rescale) createTargets(gpu_.width(), gpu_.height());
    if (retex && texturesUploaded_) {
        texturesUploaded_ = false;
        startTextureGeneration();
    }
}

// --- Texturen -------------------------------------------------------------------------------
void Renderer::startTextureGeneration() {
    auto job = std::make_shared<TexJob>();
    job->size = std::clamp(settings_.textureSize, 256, 2048);
    texJob_ = job;
    JobSystem* js = jobs_;
    jobs_->submit([job, js] {
        job->set = generateTextures(job->size, *js);
        job->done = true;
    });
}

bool Renderer::texturesReady() const { return texturesUploaded_ || (texJob_ && texJob_->done); }
float Renderer::textureProgress() const { return texturesReady() ? 1.0f : 0.5f; }

void Renderer::uploadTextures() {
    if (texturesUploaded_ || !texJob_ || !texJob_->done) return;
    const TextureSet& ts = texJob_->set;
    std::vector<D3D11_SUBRESOURCE_DATA> a, n, o;
    for (int l = 0; l < L_COUNT; ++l)
        for (int m = 0; m < ts.mips; ++m) {
            UINT s = (UINT)std::max(1, ts.size >> m);
            a.push_back({ts.albedo.data() + ts.mipOffset(l, m, 4), s * 4, 0});
            n.push_back({ts.normal.data() + ts.mipOffset(l, m, 2), s * 2, 0});
            o.push_back({ts.orm.data() + ts.mipOffset(l, m, 4), s * 4, 0});
        }
    albedoArr_ = gpu_.createTexture(ts.size, ts.size, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, TEX_SRV, ts.mips, L_COUNT, a.data());
    normalArr_ = gpu_.createTexture(ts.size, ts.size, DXGI_FORMAT_R8G8_UNORM, TEX_SRV, ts.mips, L_COUNT, n.data());
    ormArr_ = gpu_.createTexture(ts.size, ts.size, DXGI_FORMAT_R8G8B8A8_UNORM, TEX_SRV, ts.mips, L_COUNT, o.data());
    texturesUploaded_ = true;
    log::info("Texturen: {} Ebenen a {} px, {} Mip-Stufen", (int)L_COUNT, ts.size, ts.mips);
    texJob_.reset();  // Speicher freigeben
}

ID3D11ShaderResourceView* Renderer::itemIcon(const std::string& kind) {
    for (auto& [k, t] : icons_)
        if (k == kind) return t.srv.get();
    const int S = 128;
    auto px = generateItemIcon(kind, S);
    D3D11_SUBRESOURCE_DATA sd{px.data(), S * 4, 0};
    Texture t = gpu_.createTexture(S, S, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, TEX_SRV, 1, 1, &sd);
    icons_.push_back({kind, t});
    return icons_.back().second.srv.get();
}

ID3D11ShaderResourceView* Renderer::uiImage(const std::string& key, const u8* rgba, int w, int h) {
    for (auto& [k, t] : images_)
        if (k == key) {
            if (t.width == w && t.height == h) {
                gpu_.ctx()->UpdateSubresource(t.tex.get(), 0, nullptr, rgba, (UINT)w * 4, 0);
                return t.srv.get();
            }
            D3D11_SUBRESOURCE_DATA sd{rgba, (UINT)w * 4, 0};
            t = gpu_.createTexture(w, h, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, TEX_SRV, 1, 1, &sd);
            return t.srv.get();
        }
    D3D11_SUBRESOURCE_DATA sd{rgba, (UINT)w * 4, 0};
    images_.push_back({key, gpu_.createTexture(w, h, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, TEX_SRV, 1, 1, &sd)});
    if (images_.size() > 96) images_.erase(images_.begin());
    return images_.back().second.srv.get();
}

ID3D11ShaderResourceView* Renderer::uiImage(const std::string& key) const {
    for (const auto& [k, t] : images_)
        if (k == key) return t.srv.get();
    return nullptr;
}

// --- Welt ------------------------------------------------------------------------------------
void Renderer::onChunkLoaded(const world::ChunkData& cd) { world_.addChunk(gpu_, cd); }
void Renderer::onChunkUnloaded(const world::ChunkData& cd) { world_.removeChunk(gpu_, cd); }
void Renderer::clearWorld() {
    world_.clear(gpu_);
    historyValid_ = false;
}
void Renderer::syncMaterials(const world::MaterialBank& bank) { world_.syncMaterials(gpu_, bank); }

// --- Ziele ------------------------------------------------------------------------------------
void Renderer::resize(int w, int h) {
    if (w <= 0 || h <= 0) return;
    gpu_.resize(w, h);
    createTargets(w, h);
}

void Renderer::createTargets(int w, int h) {
    int iw = std::max(64, (int)std::lround(w * std::clamp(settings_.renderScale, 0.25f, 2.0f)));
    int ih = std::max(64, (int)std::lround(h * std::clamp(settings_.renderScale, 0.25f, 2.0f)));
    if (iw == iw_ && ih == ih_ && gAlbedo_.valid()) return;
    iw_ = iw;
    ih_ = ih;
    gpu_.unbindAll();
    gAlbedo_ = gpu_.createTexture(iw, ih, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, TEX_SRV | TEX_RTV);
    gNormal_ = gpu_.createTexture(iw, ih, DXGI_FORMAT_R10G10B10A2_UNORM, TEX_SRV | TEX_RTV);
    gMisc_ = gpu_.createTexture(iw, ih, DXGI_FORMAT_R8G8B8A8_UNORM, TEX_SRV | TEX_RTV);
    gEmissive_ = gpu_.createTexture(iw, ih, DXGI_FORMAT_R11G11B10_FLOAT, TEX_SRV | TEX_RTV);
    depth_ = gpu_.createDepth(iw, ih);
    aoRaw_ = gpu_.createTexture(iw, ih, DXGI_FORMAT_R8_UNORM, TEX_SRV | TEX_UAV);
    ao_ = gpu_.createTexture(iw, ih, DXGI_FORMAT_R8_UNORM, TEX_SRV | TEX_UAV);
    hdr_ = gpu_.createTexture(iw, ih, DXGI_FORMAT_R16G16B16A16_FLOAT, TEX_SRV | TEX_UAV);
    hdrSsr_ = gpu_.createTexture(iw, ih, DXGI_FORMAT_R16G16B16A16_FLOAT, TEX_SRV | TEX_UAV);
    for (auto& t : taa_) t = gpu_.createTexture(iw, ih, DXGI_FORMAT_R16G16B16A16_FLOAT, TEX_SRV | TEX_UAV);
    int bw = std::max(1, iw / 2), bh = std::max(1, ih / 2);
    bloomMips_ = 1;
    while (bloomMips_ < kBloomLevels && (bw >> bloomMips_) >= 4 && (bh >> bloomMips_) >= 4) ++bloomMips_;
    bloom_ = gpu_.createTexture(bw, bh, DXGI_FORMAT_R11G11B10_FLOAT, TEX_SRV | TEX_MIP_RTV, bloomMips_);
    historyValid_ = false;
    staging_.reset();
    stats_.internalW = iw;
    stats_.internalH = ih;
}

// --- Bild ------------------------------------------------------------------------------------
void Renderer::renderBlank(vec3 color) {
    vec4 c = srgbToLinear(vec4(color, 1.0f));
    float cc[4] = {c.x, c.y, c.z, 1.0f};
    gpu_.ctx()->ClearRenderTargetView(gpu_.backbufferRtv(), cc);
}

void Renderer::gbufferPass(const Frustum& frustum, vec3 camPos, const std::vector<MeshDraw>& meshes) {
    auto* ctx = gpu_.ctx();
    const float zero[4] = {0, 0, 0, 0};
    ID3D11RenderTargetView* rtvs[4] = {gAlbedo_.rtv.get(), gNormal_.rtv.get(), gMisc_.rtv.get(), gEmissive_.rtv.get()};
    for (auto* r : rtvs) ctx->ClearRenderTargetView(r, zero);
    ctx->ClearDepthStencilView(depth_.dsv.get(), D3D11_CLEAR_DEPTH, 0.0f, 0);  // umgekehrtes z: fern = 0
    ctx->OMSetRenderTargets(4, rtvs, depth_.dsv.get());
    gpu_.setViewport(iw_, ih_);
    float bf[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(gpu_.bsOpaque(), bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(gpu_.dsGreater(), 0);
    ctx->RSSetState(gpu_.rsCullBack());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11Buffer* cbs[4] = {frameCb_.buf.get(), layerCb_.buf.get(), objectCb_.buf.get(), passCb_.buf.get()};
    ctx->VSSetConstantBuffers(0, 4, cbs);
    ctx->PSSetConstantBuffers(0, 4, cbs);
    ID3D11ShaderResourceView* srvs[5] = {world_.materialSrv(), albedoArr_.srv.get(), normalArr_.srv.get(),
                                         ormArr_.srv.get(), macro_.srv.get()};
    ctx->PSSetShaderResources(0, 5, srvs);
    gpu_.bindCommonSamplers();

    ctx->IASetInputLayout(shaders_.worldLayout.get());
    ctx->VSSetShader(shaders_.worldVS.get(), nullptr, 0);
    ctx->PSSetShader(shaders_.worldPS.get(), nullptr, 0);
    stats_.chunksDrawn = world_.drawChunks(gpu_, frustum, camPos);
    stats_.triangles = world_.triangles();

    // Objekte
    ctx->IASetInputLayout(shaders_.meshLayout.get());
    ctx->VSSetShader(shaders_.meshVS.get(), nullptr, 0);
    ctx->PSSetShader(shaders_.meshPS.get(), nullptr, 0);
    ctx->RSSetState(gpu_.rsCullNone());
    stats_.meshes = 0;
    for (const MeshDraw& d : meshes) {
        if (d.mesh < 0) continue;
        const GpuMesh& m = meshes_.get(d.mesh);
        ObjectConstants oc{};
        oc.world = d.world;
        oc.material = m.material;
        oc.highlight = d.highlight;
        gpu_.update(objectCb_, &oc, sizeof(oc));
        UINT stride = sizeof(MeshVertex), offset = 0;
        ctx->IASetVertexBuffers(0, 1, m.vb.buf.addr(), &stride, &offset);
        ctx->IASetIndexBuffer(m.ib.buf.get(), DXGI_FORMAT_R16_UINT, 0);
        ctx->DrawIndexed(m.indexCount, 0, 0);
        ++stats_.meshes;
    }
    ctx->RSSetState(gpu_.rsCullBack());
    ctx->OMSetRenderTargets(0, nullptr, nullptr);
}

void Renderer::renderScene(const CameraState& cam, const SceneParams& p, const std::vector<MeshDraw>& meshes) {
    uploadTextures();
    if (!texturesUploaded_) {
        renderBlank({0.02f, 0.02f, 0.02f});
        return;
    }
    auto* ctx = gpu_.ctx();
    const int ow = gpu_.width(), oh = gpu_.height();
    ++frame_;
    const float nearZ = 0.04f;
    const float aspect = (float)iw_ / (float)ih_;

    // Kamera und TAA-Versatz
    bool taa = settings_.taa && settings_.displayMode == 0;
    vec2 jit{0, 0};
    if (taa) {
        int i = (int)(frame_ % 8) + 1;
        jit = {(halton(i, 2) - 0.5f) * 2.0f / (float)iw_, (halton(i, 3) - 0.5f) * 2.0f / (float)ih_};
    }
    mat4 view = viewMatrix(cam.pos, cam.yaw, cam.pitch);
    mat4 proj = perspectiveReversedInfinite(cam.fovY, aspect, nearZ, jit.x, jit.y);
    mat4 projNJ = perspectiveReversedInfinite(cam.fovY, aspect, nearZ);
    mat4 vp = proj * view, vpNJ = projNJ * view;
    bool reset = p.resetHistory || !historyValid_;

    FrameConstants& f = fc_;
    f.viewProj = vp;
    f.viewProjNoJitter = vpNJ;
    f.invViewProj = inverse(vp);
    f.prevViewProjNoJitter = reset ? vpNJ : prevViewProj_;
    f.invViewProjNoJitter = inverse(vpNJ);
    f.view = view;
    f.cameraPos = {cam.pos, std::fmod(p.time, 3600.0f)};
    f.screen = {(float)iw_, (float)ih_, 1.0f / iw_, 1.0f / ih_};
    f.jitter = {jit.x, jit.y, f.jitter.x, f.jitter.y};
    vec4 fogLin = srgbToLinear(vec4(p.fogColor, 1.0f));
    // Nebel leuchtet etwas: er traegt das Licht der Raeume
    const float fb = settings_.fogBrightness;
    f.fogColor = {fogLin.x * fb, fogLin.y * fb, fogLin.z * fb, p.fogDensity};
    f.fogParams = {p.viewDistance, 0.72f, settings_.ambientStrength, 1.0f};
    f.proj = {proj.at(0, 0), proj.at(1, 1), nearZ, 0};
    f.ringParams = {(float)world_.ringSize(), 1.0f / (float)world_.ringSize(), (float)(frame_ % 1024), nearZ};
    f.post = {p.redEdge, p.redPulse, p.menuDim, p.desaturate};
    f.post2 = {settings_.exposureBias + std::log2(std::clamp(p.moodExposure, 0.1f, 4.0f)), settings_.filmGrain, settings_.vignette,
               settings_.displayMode == 0 ? settings_.chromatic : 0.0f};
    f.post3 = {(float)settings_.displayMode, (float)ow, (float)oh,
               settings_.bloom ? settings_.bloomStrength : 0.0f};
    f.taa = {reset ? 1.0f : 0.0f, 0.1f, 0.0f, std::clamp(p.dt, 0.0f, 0.25f)};
    f.debug = {(float)settings_.debugView, settings_.ssao ? settings_.aoDirect : 0.0f, settings_.volumetric, 0};

    Frustum frustum = Frustum::fromViewProj(vpNJ);
    u32 nLights = world_.updateLights(gpu_, view, frustum, cam.pos, p.viewDistance, f.cameraPos.w, settings_.lightIntensity,
                                      settings_.shadows > 0);
    f.lightParams = {(float)nLights, settings_.lightIntensity, (float)settings_.shadows, settings_.ssao ? 1.0f : 0.0f};
    stats_.lights = nLights;
    gpu_.update(frameCb_, &f, sizeof(f));

    gbufferPass(frustum, cam.pos, meshes);

    ID3D11Buffer* cbs[4] = {frameCb_.buf.get(), layerCb_.buf.get(), objectCb_.buf.get(), passCb_.buf.get()};
    ctx->CSSetConstantBuffers(0, 4, cbs);
    gpu_.bindCommonSamplers();
    const UINT gx8 = (UINT)((iw_ + 7) / 8), gy8 = (UINT)((ih_ + 7) / 8);
    ID3D11ShaderResourceView* nullSrv[10] = {};
    ID3D11UnorderedAccessView* nullUav[1] = {nullptr};

    // SSAO
    if (settings_.ssao) {
        ID3D11ShaderResourceView* s1[2] = {depth_.srv.get(), gNormal_.srv.get()};
        ctx->CSSetShaderResources(0, 2, s1);
        ctx->CSSetUnorderedAccessViews(0, 1, aoRaw_.uav.addr(), nullptr);
        ctx->CSSetShader(shaders_.ssaoCS.get(), nullptr, 0);
        ctx->Dispatch(gx8, gy8, 1);
        ctx->CSSetUnorderedAccessViews(0, 1, nullUav, nullptr);
        ID3D11ShaderResourceView* s2[3] = {depth_.srv.get(), gNormal_.srv.get(), aoRaw_.srv.get()};
        ctx->CSSetShaderResources(0, 3, s2);
        ctx->CSSetUnorderedAccessViews(0, 1, ao_.uav.addr(), nullptr);
        ctx->CSSetShader(shaders_.ssaoBlurCS.get(), nullptr, 0);
        ctx->Dispatch(gx8, gy8, 1);
        ctx->CSSetUnorderedAccessViews(0, 1, nullUav, nullptr);
        ctx->CSSetShaderResources(0, 3, nullSrv);
    } else {
        const float one[4] = {1, 1, 1, 1};
        ctx->ClearUnorderedAccessViewFloat(ao_.uav.get(), one);
    }

    // Beleuchtung
    {
        ID3D11ShaderResourceView* s[9] = {gAlbedo_.srv.get(), gNormal_.srv.get(),  gMisc_.srv.get(),
                                          gEmissive_.srv.get(), depth_.srv.get(), ao_.srv.get(),
                                          world_.heightSrv(), world_.ambientSrv(), world_.lightSrv()};
        ctx->CSSetShaderResources(0, 9, s);
        ctx->CSSetUnorderedAccessViews(0, 1, hdr_.uav.addr(), nullptr);
        ctx->CSSetShader(shaders_.lightingCS.get(), nullptr, 0);
        ctx->Dispatch((UINT)((iw_ + 15) / 16), (UINT)((ih_ + 15) / 16), 1);
        ctx->CSSetUnorderedAccessViews(0, 1, nullUav, nullptr);
        ctx->CSSetShaderResources(0, 9, nullSrv);
    }

    // Spiegelungen auf glatten Flaechen
    Texture* litTex = &hdr_;
    if (settings_.ssr && settings_.displayMode == 0) {
        ID3D11ShaderResourceView* s[5] = {hdr_.srv.get(), depth_.srv.get(), gNormal_.srv.get(), gMisc_.srv.get(),
                                          gAlbedo_.srv.get()};
        ctx->CSSetShaderResources(0, 5, s);
        ctx->CSSetUnorderedAccessViews(0, 1, hdrSsr_.uav.addr(), nullptr);
        ctx->CSSetShader(shaders_.ssrCS.get(), nullptr, 0);
        ctx->Dispatch(gx8, gy8, 1);
        ctx->CSSetUnorderedAccessViews(0, 1, nullUav, nullptr);
        ctx->CSSetShaderResources(0, 5, nullSrv);
        litTex = &hdrSsr_;
    }

    // TAA
    Texture* sceneTex = litTex;
    if (taa) {
        Texture& out = taa_[taaIndex_];
        Texture& hist = taa_[taaIndex_ ^ 1];
        ID3D11ShaderResourceView* s[3] = {litTex->srv.get(), hist.srv.get(), depth_.srv.get()};
        ctx->CSSetShaderResources(0, 3, s);
        ctx->CSSetUnorderedAccessViews(0, 1, out.uav.addr(), nullptr);
        ctx->CSSetShader(shaders_.taaCS.get(), nullptr, 0);
        ctx->Dispatch(gx8, gy8, 1);
        ctx->CSSetUnorderedAccessViews(0, 1, nullUav, nullptr);
        ctx->CSSetShaderResources(0, 3, nullSrv);
        sceneTex = &out;
        taaIndex_ ^= 1;
    }
    prevViewProj_ = vpNJ;
    historyValid_ = true;

    // Belichtung
    {
        PassConstants pc{};
        pc.p0 = {-4.0f, 3.5f, 1.6f, settings_.exposureAuto};  // EV-Grenzen, Geschwindigkeit, Anteil Automatik
        pc.p1 = {settings_.exposureBase, 0.2f, 0, 0};          // feste Belichtung, Zielhelligkeit
        gpu_.update(passCb_, &pc, sizeof(pc));
        ID3D11ShaderResourceView* s[1] = {sceneTex->srv.get()};
        ctx->CSSetShaderResources(0, 1, s);
        ctx->CSSetUnorderedAccessViews(0, 1, exposure_.uav.addr(), nullptr);
        ctx->CSSetShader(shaders_.exposureCS.get(), nullptr, 0);
        ctx->Dispatch(1, 1, 1);
        ctx->CSSetUnorderedAccessViews(0, 1, nullUav, nullptr);
        ctx->CSSetShaderResources(0, 1, nullSrv);
    }

    // Bloom: Verkleinerungskette, dann additiv zurueck
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(shaders_.fullscreenVS.get(), nullptr, 0);
    ctx->OMSetDepthStencilState(gpu_.dsNone(), 0);
    ctx->RSSetState(gpu_.rsCullNone());
    ctx->PSSetConstantBuffers(0, 4, cbs);
    float bf[4] = {0, 0, 0, 0};
    {
        ctx->PSSetShader(shaders_.bloomDownPS.get(), nullptr, 0);
        ctx->OMSetBlendState(gpu_.bsOpaque(), bf, 0xFFFFFFFF);
        for (int m = 0; m < bloomMips_; ++m) {
            int w = std::max(1, bloom_.width >> m), h = std::max(1, bloom_.height >> m);
            ID3D11ShaderResourceView* src = m == 0 ? sceneTex->srv.get() : bloom_.mipSrv[(size_t)m - 1].get();
            int sw = m == 0 ? iw_ : std::max(1, bloom_.width >> (m - 1));
            int sh = m == 0 ? ih_ : std::max(1, bloom_.height >> (m - 1));
            PassConstants pc{};
            pc.p0 = {1.0f / (float)sw, 1.0f / (float)sh, m == 0 ? 1.0f : 0.0f, 0};
            gpu_.update(passCb_, &pc, sizeof(pc));
            ctx->OMSetRenderTargets(1, bloom_.mipRtv[(size_t)m].addr(), nullptr);
            gpu_.setViewport(w, h);
            ctx->PSSetShaderResources(0, 1, &src);
            ctx->Draw(3, 0);
            ctx->PSSetShaderResources(0, 1, nullSrv);
        }
        ctx->PSSetShader(shaders_.bloomUpPS.get(), nullptr, 0);
        ctx->OMSetBlendState(gpu_.bsAdd(), bf, 0xFFFFFFFF);
        for (int m = bloomMips_ - 2; m >= 0; --m) {
            int w = std::max(1, bloom_.width >> m), h = std::max(1, bloom_.height >> m);
            int sw = std::max(1, bloom_.width >> (m + 1)), sh = std::max(1, bloom_.height >> (m + 1));
            PassConstants pc{};
            pc.p0 = {1.0f / (float)sw, 1.0f / (float)sh, 0, 1.0f};
            gpu_.update(passCb_, &pc, sizeof(pc));
            ctx->OMSetRenderTargets(1, bloom_.mipRtv[(size_t)m].addr(), nullptr);
            gpu_.setViewport(w, h);
            ID3D11ShaderResourceView* src = bloom_.mipSrv[(size_t)m + 1].get();
            ctx->PSSetShaderResources(0, 1, &src);
            ctx->Draw(3, 0);
            ctx->PSSetShaderResources(0, 1, nullSrv);
        }
    }

    // Abschluss in den Backbuffer
    {
        // Bloom-Summe ueber alle Stufen -> Staerke je Stufe normieren
        f.post3.w = settings_.bloom ? settings_.bloomStrength / (float)std::max(1, bloomMips_) * 2.0f : 0.0f;
        gpu_.update(frameCb_, &f, sizeof(f));
        ID3D11RenderTargetView* rtv = gpu_.backbufferRtv();
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->OMSetBlendState(gpu_.bsOpaque(), bf, 0xFFFFFFFF);
        gpu_.setViewport(ow, oh);
        ctx->PSSetShader(shaders_.postPS.get(), nullptr, 0);
        ID3D11ShaderResourceView* s[5] = {sceneTex->srv.get(), bloom_.mipSrv[0].get(), exposure_.srv.get(),
                                          bloom_.mipSrv[(size_t)std::min(2, bloomMips_ - 1)].get(), glyphs_.srv.get()};
        ctx->PSSetShaderResources(0, 5, s);
        gpu_.bindCommonSamplers();
        ctx->Draw(3, 0);
        ctx->PSSetShaderResources(0, 5, nullSrv);
    }
    ctx->RSSetState(gpu_.rsCullBack());
}

void Renderer::present() {
    gpu_.present(settings_.vsync);
}

bool Renderer::captureBackbuffer(std::vector<u8>& rgba, int& w, int& h) {
    auto* ctx = gpu_.ctx();
    w = gpu_.width();
    h = gpu_.height();
    D3D11_TEXTURE2D_DESC d{};
    gpu_.backbuffer()->GetDesc(&d);
    D3D11_TEXTURE2D_DESC sd = d;
    sd.BindFlags = 0;
    sd.MiscFlags = 0;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Com<ID3D11Texture2D> st;
    if (FAILED(gpu_.dev()->CreateTexture2D(&sd, nullptr, st.put()))) return false;
    ctx->CopyResource(st.get(), gpu_.backbuffer());
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ctx->Map(st.get(), 0, D3D11_MAP_READ, 0, &m))) return false;
    rgba.resize((size_t)w * h * 4);
    for (int y = 0; y < h; ++y) std::memcpy(&rgba[(size_t)y * w * 4], (u8*)m.pData + (size_t)y * m.RowPitch, (size_t)w * 4);
    ctx->Unmap(st.get(), 0);
    for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
    return true;
}

bool Renderer::captureScene(int tw, int th, std::vector<u8>& out) {
    std::vector<u8> full;
    int w, h;
    if (!captureBackbuffer(full, w, h)) return false;
    out.assign((size_t)tw * th * 4, 0);
    // Flaechenmittel (Box-Filter) auf die Zielgroesse
    for (int y = 0; y < th; ++y)
        for (int x = 0; x < tw; ++x) {
            int x0 = x * w / tw, x1 = std::max(x0 + 1, (x + 1) * w / tw);
            int y0 = y * h / th, y1 = std::max(y0 + 1, (y + 1) * h / th);
            u32 acc[3] = {0, 0, 0}, n = 0;
            for (int yy = y0; yy < y1; yy += 2)
                for (int xx = x0; xx < x1; xx += 2) {
                    const u8* p = &full[((size_t)yy * w + xx) * 4];
                    acc[0] += p[0], acc[1] += p[1], acc[2] += p[2];
                    ++n;
                }
            u8* q = &out[((size_t)y * tw + x) * 4];
            for (int c = 0; c < 3; ++c) q[c] = (u8)(acc[c] / std::max(1u, n));
            q[3] = 255;
        }
    return true;
}

}  // namespace lim::gfx
