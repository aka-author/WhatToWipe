// Build a Windows .ico and per-size PNGs from broombunny art (PNG-in-ICO format).
// Compiled with the same MinGW toolchain as EraseAndRewrite (see build.ps1).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "third_party/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"

struct Image {
    int w = 0;
    int h = 0;
    std::vector<unsigned char> rgba;
};

struct Layer {
    int size;
    const char* file;
    bool pixelArt;
};

// Taskbar/title bar need 16/20/24/32 at common Windows DPI scales.
// Use broombunny-small for small layers: the *-16x16/*-32x32 files are full-size art.
static const Layer kLayers[] = {
    {256, "broombunny.png", false},
    {64, "broombunny-small.png", false},
    {48, "broombunny-small.png", false},
    {32, "broombunny-small.png", false},
    {24, "broombunny-small.png", false},
    {20, "broombunny-small.png", false},
    {16, "broombunny-small.png", false},
};

static std::vector<unsigned char> readFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "open failed: %s\n", path.c_str());
        std::exit(1);
    }
    std::fseek(f, 0, SEEK_END);
    const long len = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        std::fprintf(stderr, "empty file: %s\n", path.c_str());
        std::exit(1);
    }
    std::vector<unsigned char> buf(static_cast<size_t>(len));
    if (std::fread(buf.data(), 1, buf.size(), f) != buf.size()) {
        std::fprintf(stderr, "read failed: %s\n", path.c_str());
        std::exit(1);
    }
    std::fclose(f);
    return buf;
}

static Image loadPng(const std::string& path) {
    const std::vector<unsigned char> file = readFile(path);
    int w = 0;
    int h = 0;
    int comp = 0;
    unsigned char* pixels = stbi_load_from_memory(
        file.data(), static_cast<int>(file.size()), &w, &h, &comp, 4);
    if (!pixels) {
        std::fprintf(stderr, "png decode failed: %s\n", path.c_str());
        std::exit(1);
    }
    Image img;
    img.w = w;
    img.h = h;
    img.rgba.assign(pixels, pixels + (w * h * 4));
    stbi_image_free(pixels);
    return img;
}

static unsigned char sampleNearest(const Image& src, int x, int y, int c) {
    if (x < 0 || y < 0 || x >= src.w || y >= src.h) {
        return 0;
    }
    return src.rgba[(y * src.w + x) * 4 + c];
}

static Image resizeNearest(const Image& src, int size) {
    Image dst;
    dst.w = size;
    dst.h = size;
    dst.rgba.resize(static_cast<size_t>(size * size * 4));
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int sx = (x * src.w) / size;
            const int sy = (y * src.h) / size;
            for (int c = 0; c < 4; ++c) {
                dst.rgba[(y * size + x) * 4 + c] = sampleNearest(src, sx, sy, c);
            }
        }
    }
    return dst;
}

static Image resizeBox(const Image& src, int size) {
    Image dst;
    dst.w = size;
    dst.h = size;
    dst.rgba.resize(static_cast<size_t>(size * size * 4));
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int x0 = (x * src.w) / size;
            const int y0 = (y * src.h) / size;
            const int x1 = ((x + 1) * src.w) / size;
            const int y1 = ((y + 1) * src.h) / size;
            const int xEnd = x1 < src.w ? x1 : src.w;
            const int yEnd = y1 < src.h ? y1 : src.h;
            const int count = (xEnd - x0) * (yEnd - y0);
            float acc[4] = {0, 0, 0, 0};
            if (count > 0) {
                for (int sy = y0; sy < yEnd; ++sy) {
                    for (int sx = x0; sx < xEnd; ++sx) {
                        const int idx = (sy * src.w + sx) * 4;
                        for (int c = 0; c < 4; ++c) {
                            acc[c] += src.rgba[idx + c];
                        }
                    }
                }
            }
            const int out = (y * size + x) * 4;
            for (int c = 0; c < 4; ++c) {
                dst.rgba[out + c] =
                    static_cast<unsigned char>(acc[c] / static_cast<float>(count > 0 ? count : 1) + 0.5f);
            }
        }
    }
    return dst;
}

static Image renderLayer(const Image& src, const Layer& layer) {
    if (src.w == layer.size && src.h == layer.size) {
        return src;
    }
    if (layer.pixelArt && src.w <= layer.size && src.h <= layer.size) {
        return resizeNearest(src, layer.size);
    }
    return resizeBox(src, layer.size);
}

static void pngWriteCallback(void* ctx, void* data, int len) {
    auto* out = static_cast<std::vector<unsigned char>*>(ctx);
    const auto* bytes = static_cast<const unsigned char*>(data);
    out->insert(out->end(), bytes, bytes + len);
}

static std::vector<unsigned char> encodePng(const Image& img) {
    std::vector<unsigned char> png;
    if (!stbi_write_png_to_func(
            pngWriteCallback, &png, img.w, img.h, 4, img.rgba.data(), img.w * 4)) {
        std::fprintf(stderr, "png encode failed\n");
        std::exit(1);
    }
    return png;
}

static void writeU16(std::vector<unsigned char>& out, uint16_t v) {
    out.push_back(static_cast<unsigned char>(v & 0xff));
    out.push_back(static_cast<unsigned char>((v >> 8) & 0xff));
}

static void writeU32(std::vector<unsigned char>& out, uint32_t v) {
    out.push_back(static_cast<unsigned char>(v & 0xff));
    out.push_back(static_cast<unsigned char>((v >> 8) & 0xff));
    out.push_back(static_cast<unsigned char>((v >> 16) & 0xff));
    out.push_back(static_cast<unsigned char>((v >> 24) & 0xff));
}

static std::vector<unsigned char> buildIco(const std::vector<std::vector<unsigned char>>& pngs,
                                           const std::vector<int>& sizes) {
    const int n = static_cast<int>(pngs.size());
    const uint32_t header = 6u + static_cast<uint32_t>(n) * 16u;
    uint32_t offset = header;
    std::vector<unsigned char> out;
    out.reserve(header + 1024);
    writeU16(out, 0);
    writeU16(out, 1);
    writeU16(out, static_cast<uint16_t>(n));
    for (int i = 0; i < n; ++i) {
        const int w = sizes[static_cast<size_t>(i)];
        unsigned char bw = static_cast<unsigned char>(w >= 256 ? 0 : w);
        out.push_back(bw);
        out.push_back(bw);
        out.push_back(0);
        out.push_back(0);
        writeU16(out, 1);
        writeU16(out, 32);
        writeU32(out, static_cast<uint32_t>(pngs[static_cast<size_t>(i)].size()));
        writeU32(out, offset);
        offset += static_cast<uint32_t>(pngs[static_cast<size_t>(i)].size());
    }
    for (const auto& png : pngs) {
        out.insert(out.end(), png.begin(), png.end());
    }
    return out;
}

static void writeBinary(const std::string& path, const std::vector<unsigned char>& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "write failed: %s\n", path.c_str());
        std::exit(1);
    }
    if (std::fwrite(data.data(), 1, data.size(), f) != data.size()) {
        std::fprintf(stderr, "write incomplete: %s\n", path.c_str());
        std::exit(1);
    }
    std::fclose(f);
}

static void writePngFile(const std::string& dir, int size, const Image& img) {
    char name[64];
    std::snprintf(name, sizeof(name), "app-%d.png", size);
    const std::string path = dir + "/" + name;
    const std::vector<unsigned char> png = encodePng(img);
    writeBinary(path, png);
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: %s <art-dir> <output.ico> <png-out-dir>\n", argv[0]);
        return 1;
    }
    const std::string artDir = argv[1];
    const std::string outIco = argv[2];
    const std::string pngDir = argv[3];

    std::vector<std::vector<unsigned char>> pngs;
    std::vector<int> sizes;
    for (const Layer& layer : kLayers) {
        const std::string path = artDir + "/" + layer.file;
        const Image src = loadPng(path);
        const Image scaled = renderLayer(src, layer);
        pngs.push_back(encodePng(scaled));
        sizes.push_back(layer.size);
        writePngFile(pngDir, layer.size, scaled);
        std::fprintf(stderr, "layer %d from %s (%dx%d -> %d)\n",
                     layer.size, layer.file, src.w, src.h, layer.size);
    }
    const std::vector<unsigned char> ico = buildIco(pngs, sizes);
    writeBinary(outIco, ico);
    std::fprintf(stderr, "wrote %s (%zu bytes)\n", outIco.c_str(), ico.size());
    return 0;
}
