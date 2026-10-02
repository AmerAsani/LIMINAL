#include "platform/image_io.hpp"

#include "core/paths.hpp"
#include "core/com_ptr.hpp"

#include <objbase.h>
#include <shlwapi.h>
#include <wincodec.h>

namespace lim::gfx {

using lim::Com;

namespace {

struct ComInit {
    bool ok;
    ComInit() : ok(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) || true) {}
};

Com<IWICImagingFactory> factory() {
    static ComInit init;
    Com<IWICImagingFactory> f;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (void**)f.put());
    return f;
}

bool encodeTo(IStream* stream, const u8* rgba, int w, int h) {
    auto f = factory();
    if (!f) return false;
    Com<IWICBitmapEncoder> enc;
    if (FAILED(f->CreateEncoder(GUID_ContainerFormatPng, nullptr, enc.put()))) return false;
    if (FAILED(enc->Initialize(stream, WICBitmapEncoderNoCache))) return false;
    Com<IWICBitmapFrameEncode> frame;
    if (FAILED(enc->CreateNewFrame(frame.put(), nullptr))) return false;
    frame->Initialize(nullptr);
    frame->SetSize((UINT)w, (UINT)h);
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppRGBA;
    frame->SetPixelFormat(&fmt);
    bool ok;
    if (IsEqualGUID(fmt, GUID_WICPixelFormat32bppRGBA)) {
        ok = SUCCEEDED(frame->WritePixels((UINT)h, (UINT)w * 4, (UINT)((size_t)w * h * 4), (BYTE*)rgba));
    } else {
        // Encoder will ein anderes Format (z. B. BGRA): umsortieren
        std::vector<u8> bgra((size_t)w * h * 4);
        for (size_t i = 0; i < bgra.size(); i += 4) {
            bgra[i] = rgba[i + 2];
            bgra[i + 1] = rgba[i + 1];
            bgra[i + 2] = rgba[i];
            bgra[i + 3] = rgba[i + 3];
        }
        ok = SUCCEEDED(frame->WritePixels((UINT)h, (UINT)w * 4, (UINT)bgra.size(), bgra.data()));
    }
    return ok && SUCCEEDED(frame->Commit()) && SUCCEEDED(enc->Commit());
}

bool decodeFrom(IWICBitmapDecoder* dec, std::vector<u8>& rgba, int& w, int& h) {
    auto f = factory();
    Com<IWICBitmapFrameDecode> frame;
    if (FAILED(dec->GetFrame(0, frame.put()))) return false;
    Com<IWICFormatConverter> conv;
    if (FAILED(f->CreateFormatConverter(conv.put()))) return false;
    if (FAILED(conv->Initialize(frame.get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                                WICBitmapPaletteTypeCustom)))
        return false;
    UINT uw, uh;
    conv->GetSize(&uw, &uh);
    w = (int)uw;
    h = (int)uh;
    rgba.resize((size_t)w * h * 4);
    return SUCCEEDED(conv->CopyPixels(nullptr, uw * 4, (UINT)rgba.size(), rgba.data()));
}

}  // namespace

bool savePng(const std::string& path, const u8* rgba, int w, int h) {
    paths::createDirectories(paths::parent(path));
    auto f = factory();
    if (!f) return false;
    Com<IWICStream> stream;
    if (FAILED(f->CreateStream(stream.put()))) return false;
    if (FAILED(stream->InitializeFromFilename(paths::widen(path).c_str(), GENERIC_WRITE))) return false;
    return encodeTo(stream.get(), rgba, w, h);
}

bool loadPng(const std::string& path, std::vector<u8>& rgba, int& w, int& h) {
    auto f = factory();
    if (!f) return false;
    Com<IWICBitmapDecoder> dec;
    if (FAILED(f->CreateDecoderFromFilename(paths::widen(path).c_str(), nullptr, GENERIC_READ,
                                            WICDecodeMetadataCacheOnDemand, dec.put())))
        return false;
    return decodeFrom(dec.get(), rgba, w, h);
}

bool encodePng(const u8* rgba, int w, int h, std::vector<u8>& out) {
    Com<IStream> mem;
    mem = Com<IStream>();
    IStream* raw = SHCreateMemStream(nullptr, 0);
    if (!raw) return false;
    *mem.put() = raw;
    if (!encodeTo(mem.get(), rgba, w, h)) return false;
    STATSTG st{};
    mem->Stat(&st, STATFLAG_NONAME);
    out.resize((size_t)st.cbSize.QuadPart);
    LARGE_INTEGER zero{};
    mem->Seek(zero, STREAM_SEEK_SET, nullptr);
    ULONG read = 0;
    mem->Read(out.data(), (ULONG)out.size(), &read);
    return read == out.size();
}

bool decodePng(const u8* data, size_t size, std::vector<u8>& rgba, int& w, int& h) {
    auto f = factory();
    if (!f) return false;
    Com<IWICStream> stream;
    if (FAILED(f->CreateStream(stream.put()))) return false;
    if (FAILED(stream->InitializeFromMemory((BYTE*)data, (DWORD)size))) return false;
    Com<IWICBitmapDecoder> dec;
    if (FAILED(f->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnDemand, dec.put()))) return false;
    return decodeFrom(dec.get(), rgba, w, h);
}

}  // namespace lim::gfx
