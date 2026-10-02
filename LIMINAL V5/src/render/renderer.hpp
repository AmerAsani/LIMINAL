// Renderer: oeffentliche Schnittstelle fuer das Spiel.
//
// Das Gameplay kennt keine GPU-Typen. Es liefert pro Bild eine Kamera, ein paar
// Szenenparameter (Nebel, Effekte) und eine Liste von Objekten; die Welt-
// geometrie kommt ueber onChunkLoaded/onChunkUnloaded aus dem Streaming.
//
// Ablauf eines Bildes (Deferred Rendering):
//   G-Buffer -> SSAO -> Beleuchtung (Tiled, Compute) -> TAA -> Belichtung
//   -> Bloom -> Tonemapping/Nachbearbeitung -> Oberflaeche -> Present
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "render/font.hpp"
#include "render/gpu.hpp"
#include "render/gpu_types.hpp"
#include "render/meshes.hpp"
#include "render/shaders.hpp"
#include "render/texture_gen.hpp"
#include "render/ui_renderer.hpp"
#include "render/world_renderer.hpp"

namespace lim {
class JobSystem;
namespace plat {
class Window;
}
namespace world {
struct ChunkData;
class MaterialBank;
}  // namespace world
}  // namespace lim

namespace lim::gfx {

struct RenderSettings {
    float renderScale = 1.0f;   // interne Aufloesung relativ zum Fenster
    int shadows = 2;            // 0 aus, 1 normal, 2 hoch
    int ssao = 1;               // 0 aus, 1 an
    bool taa = true;
    bool bloom = true;
    bool ssr = true;            // Spiegelungen auf glatten Flaechen
    float volumetric = 1.0f;    // Lichthoefe im Nebel (0 = aus)
    int textureSize = 1024;     // 512 / 1024 / 2048
    int anisotropy = 8;
    float filmGrain = 0.35f;
    float vignette = 0.3f;
    float chromatic = 0.2f;
    int displayMode = 0;        // 0 modern, 1 Terminal (Halbbloecke), 2 ASCII, 3 Monochrom
    bool vsync = true;
    float exposureBias = 0.0f;  // Blendenstufen
    float bloomStrength = 0.05f;
    // Feinabstimmung (nur ueber die Einstellungsdatei bzw. --set)
    float ambientStrength = 0.3f;  // vorberechnetes Umgebungslicht
    float lightIntensity = 14.0f;   // Staerke der Leuchten
    float fogBrightness = 0.9f;
    float fogDensity = 0.45f;        // Faktor auf die Nebeldichte der Raeume
    float aoDirect = 0.6f;          // Anteil der Umgebungsverdeckung auf direktes Licht
    float exposureAuto = 0.3f;      // Anteil der automatischen Belichtung (Rest: feste Belichtung)
    float exposureBase = 1.2f;      // feste Belichtung
    int debugView = 0;              // 0 aus, 1 Direktlicht, 2 Umgebung, 3 Normalen, 4 SSAO, 5 Schatten, 6 Lichter je Kachel
};

struct CameraState {
    vec3 pos;
    float yaw = 0, pitch = 0;
    float fovY = 1.0f;
};

struct SceneParams {
    vec3 fogColor{0.1f, 0.1f, 0.1f};  // sRGB-Anzeigefarbe wie in V4
    float fogDensity = 0.05f;
    float viewDistance = 60.0f;
    float time = 0, dt = 0.016f;
    float redEdge = 0, redPulse = 0;
    float menuDim = 0, desaturate = 0;
    float moodExposure = 1.0f;  // Faktor aus der Raumhelligkeit (dunkle Zonen bleiben dunkel)
    bool resetHistory = false;
};

struct MeshDraw {
    int mesh = -1;
    mat4 world;
    float highlight = 0;
};

struct RenderStats {
    u32 chunksDrawn = 0, lights = 0, meshes = 0;
    u64 triangles = 0;
    int internalW = 0, internalH = 0;
    float exposure = 1.0f;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(plat::Window& window, JobSystem& jobs, const RenderSettings& settings, bool forceWarp, std::string& error);
    void applySettings(const RenderSettings& s);
    const RenderSettings& settings() const { return settings_; }
    const GpuInfo& gpuInfo() const { return gpu_.info(); }

    // Prozedurale Texturen entstehen im Hintergrund.
    void startTextureGeneration();
    bool texturesReady() const;
    float textureProgress() const;

    // Weltgeometrie
    void onChunkLoaded(const world::ChunkData& cd);
    void onChunkUnloaded(const world::ChunkData& cd);
    void clearWorld();
    void syncMaterials(const world::MaterialBank& bank);

    // Bild
    void resize(int w, int h);
    void renderScene(const CameraState& cam, const SceneParams& p, const std::vector<MeshDraw>& meshes);
    void renderBlank(vec3 color);  // Ladebildschirm-Hintergrund
    UiRenderer& ui() { return ui_; }
    void renderUi() { ui_.render(gpu_); }
    void present();

    int meshId(const std::string& name) const { return meshes_.find(name); }
    ID3D11ShaderResourceView* itemIcon(const std::string& kind);
    // Bilder fuer die Oberflaeche (Karte, Vorschaubilder): anlegen oder aktualisieren
    ID3D11ShaderResourceView* uiImage(const std::string& key, const u8* rgba, int w, int h);
    ID3D11ShaderResourceView* uiImage(const std::string& key) const;
    // Bildschirminhalt (ohne Oberflaeche) als RGBA8 in gewuenschter Groesse
    bool captureScene(int w, int h, std::vector<u8>& rgba);
    // Aktueller Backbuffer (mit Oberflaeche) in voller Groesse
    bool captureBackbuffer(std::vector<u8>& rgba, int& w, int& h);
    const RenderStats& stats() const { return stats_; }
    int outputWidth() const { return gpu_.width(); }
    int outputHeight() const { return gpu_.height(); }

private:
    void createTargets(int w, int h);
    void uploadTextures();
    void gbufferPass(const Frustum& frustum, vec3 camPos, const std::vector<MeshDraw>& meshes);
    void postChain(const SceneParams& p);

    Gpu gpu_;
    ShaderSet shaders_;
    FontAtlas font_;
    UiRenderer ui_;
    WorldRenderer world_;
    MeshLibrary meshes_;
    RenderSettings settings_;
    JobSystem* jobs_ = nullptr;

    // Texturen
    Texture albedoArr_, normalArr_, ormArr_, macro_, glyphs_;
    struct TexJob;
    std::shared_ptr<TexJob> texJob_;
    bool texturesUploaded_ = false;
    std::vector<std::pair<std::string, Texture>> icons_;
    std::vector<std::pair<std::string, Texture>> images_;

    // Ziele
    int iw_ = 0, ih_ = 0;
    Texture gAlbedo_, gNormal_, gMisc_, gEmissive_, depth_, aoRaw_, ao_, hdr_, hdrSsr_, taa_[2], bloom_;
    int taaIndex_ = 0;
    int bloomMips_ = 0;
    Buffer exposure_;
    Buffer frameCb_, passCb_, objectCb_, layerCb_;

    // Zeitlicher Zustand
    FrameConstants fc_{};
    mat4 prevViewProj_ = mat4::identity();
    bool historyValid_ = false;
    u32 frame_ = 0;
    RenderStats stats_;
    Com<ID3D11Texture2D> staging_;
};

}  // namespace lim::gfx
