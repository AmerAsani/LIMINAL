// Build-Werkzeug: kompiliert HLSL zu Direct3D-Bytecode (.cso).
//
//   shaderc <eingabe.hlsl> <einstieg> <profil> <ausgabe.cso> [-DNAME=WERT ...] [-g]
//
// Nutzt d3dcompiler_47.dll aus Windows. Das fertige Spiel liest nur die .cso-
// Dateien und braucht den Compiler daher zur Laufzeit nicht.
#include <windows.h>
#include <d3dcompiler.h>

#include <cstdio>
#include <string>
#include <vector>

using CompileFn = HRESULT(WINAPI*)(LPCWSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT,
                                   ID3DBlob**, ID3DBlob**);

static std::string narrow(const wchar_t* w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    s.resize(strlen(s.c_str()));
    return s;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 5) {
        std::fprintf(stderr, "Aufruf: shaderc <eingabe.hlsl> <einstieg> <profil> <ausgabe.cso> [-DNAME=WERT] [-g]\n");
        return 2;
    }
    HMODULE dll = LoadLibraryW(L"d3dcompiler_47.dll");
    if (!dll) {
        std::fprintf(stderr, "d3dcompiler_47.dll nicht gefunden\n");
        return 3;
    }
    auto compile = (CompileFn)(void*)GetProcAddress(dll, "D3DCompileFromFile");
    std::string entry = narrow(argv[2]), profile = narrow(argv[3]);
    std::vector<std::string> names, values;
    bool debug = false;
    for (int i = 5; i < argc; ++i) {
        std::string a = narrow(argv[i]);
        if (a == "-g") debug = true;
        else if (a.rfind("-D", 0) == 0) {
            auto eq = a.find('=');
            names.push_back(a.substr(2, eq == std::string::npos ? std::string::npos : eq - 2));
            values.push_back(eq == std::string::npos ? "1" : a.substr(eq + 1));
        }
    }
    std::vector<D3D_SHADER_MACRO> macros;
    for (size_t i = 0; i < names.size(); ++i) macros.push_back({names[i].c_str(), values[i].c_str()});
    macros.push_back({nullptr, nullptr});
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
    flags |= debug ? (D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION) : D3DCOMPILE_OPTIMIZATION_LEVEL3;
    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    HRESULT hr = compile(argv[1], macros.data(), D3D_COMPILE_STANDARD_FILE_INCLUDE, entry.c_str(), profile.c_str(),
                         flags, 0, &code, &errors);
    if (errors) {
        std::fprintf(stderr, "%s\n", (const char*)errors->GetBufferPointer());
        errors->Release();
    }
    if (FAILED(hr) || !code) {
        std::fprintf(stderr, "Shader-Fehler: %s (%s)\n", narrow(argv[1]).c_str(), entry.c_str());
        return 1;
    }
    FILE* f = _wfopen(argv[4], L"wb");
    if (!f) {
        std::fprintf(stderr, "Kann %s nicht schreiben\n", narrow(argv[4]).c_str());
        return 1;
    }
    std::fwrite(code->GetBufferPointer(), 1, code->GetBufferSize(), f);
    std::fclose(f);
    code->Release();
    return 0;
}
