// Build a Windows .ico from size-specific broombunny PNG art (PNG-in-ICO format).
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
    bool smooth;
};

static const Layer kLayers[] = {
    {256, "broombunny.png", true},
    {64, "broombunny-small.png", true},
    {48, "broombunny-small.png", true},
    {32, "broombunny-32x32.png", false},
    {16, "broombunny-16x16.png", false},
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

static unsigned char sample(const Image& src, float x, float y, int c) {
    const int ix = static_cast<int>(x);
    const int iy = static_cast<int>(y);
    if (ix < 0 || iy < 0 || ix >= src.w || iy >= src.h) {
        return 0;
    }
    return src.rgba[(iy * src.w + ix) * 4 + c];
}

static unsigned char lerpByte(float a, float b, float t) {
    return static_cast<unsigned char>(a + (b - a) * t + 0.5f);
}

static unsigned char sampleSmooth(const Image& src, float x, float y, int c) {
    const float fx = x - 0.5f;
    const float fy = y - 0.5f;
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);
    const float c00 = sample(src, static_cast<float>(x0), static_cast<float>(y0), c);
    const float c10 = sample(src, static_cast<float>(x1), static_cast<float>(y0), c);
    const float c01 = sample(src, static_cast<float>(x0), static_cast<float>(y1), c);
    const float c11 = sample(src, static_cast<float>(x1), static_cast<float>(y1), c);
    const float c0 = c00 + (c10 - c00) * tx;
    const float c1 = c01 + (c11 - c01) * tx;
    return lerpByte(c0, c1, ty);
}

static Image resize(const Image& src, int size, bool smooth) {
    Image dst;
    dst.w = size;
    dst.h = size;
    dst.rgba.resize(static_cast<size_t>(size * size * 4));
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float sx = (x + 0.5f) * static_cast<float>(src.w) / static_cast<float>(size);
            const float sy = (y + 0.5f) * static_cast<float>(src.h) / static_cast<float>(size);
            for (int c = 0; c < 4; ++c) {
                const unsigned char v =
                    smooth ? sampleSmooth(src, sx, sy, c) : sample(src, sx, sy, c);
                dst.rgba[(y * size + x) * 4 + c] = v;
            }
        }
    }
    return dst;
}

static void pngWriteCallback(void* ctx, void* data, int len) {
    auto* out = static_cast<std::vector<unsigned char>*>(ctx);
    const auto* bytes = static_cast<const unsigned char*>(data);
    out->insert(out->end(), bytes, bytes + len);
}

static std::vector<unsigned char> encodePng(const Image& img) {
    std::vector<unsigned char> png;
    if (!stbi_write_png_to_func(pngWriteCallback, &png, img.w, img.h, 4, img.rgba.data(), img.w * 4)) {
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

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <art-dir> <output.ico>\n", argv[0]);
        return 1;
    }
    const std::string artDir = argv[1];
    const std::string outIco = argv[2];

    std::vector<std::vector<unsigned char>> pngs;
    std::vector<int> sizes;
    for (const Layer& layer : kLayers) {
        const std::string path = artDir + "/" + layer.file;
        const Image src = loadPng(path);
        const Image scaled = resize(src, layer.size, layer.smooth);
        pngs.push_back(encodePng(scaled));
        sizes.push_back(layer.size);
        std::fprintf(stderr, "layer %d from %s\n", layer.size, layer.file);
    }
    const std::vector<unsigned char> ico = buildIco(pngs, sizes);
    writeBinary(outIco, ico);
    std::fprintf(stderr, "wrote %s (%zu bytes)\n", outIco.c_str(), ico.size());
    return 0;
}
