// PNG lesen und schreiben ueber die Windows Imaging Component (Teil von Windows).
#pragma once

#include <string>
#include <vector>

#include "core/core.hpp"

namespace lim::gfx {

bool savePng(const std::string& path, const u8* rgba, int w, int h);
bool loadPng(const std::string& path, std::vector<u8>& rgba, int& w, int& h);
// PNG im Speicher (fuer Vorschaubilder in Spielstaenden)
bool encodePng(const u8* rgba, int w, int h, std::vector<u8>& out);
bool decodePng(const u8* data, size_t size, std::vector<u8>& rgba, int& w, int& h);

}  // namespace lim::gfx
