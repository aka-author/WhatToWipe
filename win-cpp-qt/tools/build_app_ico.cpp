// Build a Windows .ico and per-size PNGs from broombunny art.
// Small layers (<=64) use classic BMP+DIB ICO entries; 256 uses PNG.
// Compiled with the same MinGW toolchain as EraseAndRewrite (see build.ps1).

#include <algorithm>
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
};

static const Layer kLayers[] = {
    {256, "broombunny.png"},
    {64, "broombunny-small.png"},
    {48, "broombunny-small.png"},
    {40, "broombunny-small.png"},
    {36, "broombunny-small.png"},
    {32, "broombunny-small.png"},
    {30, "broombunny-small.png"},
    {24, "broombunny-small.png"},
    {20, "broombunny-small.png"},
    {16, "broombunny-small.png"},
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

static Image resizeBox(const Image& src, int dstW, int dstH) {
    if (src.w == dstW && src.h == dstH) {
        return src;
    }
    Image dst;
    dst.w = dstW;
    dst.h = dstH;
    dst.rgba.resize(static_cast<size_t>(dstW * dstH * 4));
    for (int y = 0; y < dstH; ++y) {
        for (int x = 0; x < dstW; ++x) {
            const int x0 = (x * src.w) / dstW;
            const int y0 = (y * src.h) / dstH;
            const int x1 = ((x + 1) * src.w) / dstW;
            const int y1 = ((y + 1) * src.h) / dstH;
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
            const int out = (y * dstW + x) * 4;
            for (int c = 0; c < 4; ++c) {
                dst.rgba[out + c] =
                    static_cast<unsigned char>(acc[c] / static_cast<float>(count > 0 ? count : 1) + 0.5f);
            }
        }
    }
    return dst;
}

static Image resizeSquare(const Image& src, int size) {
    return resizeBox(src, size, size);
}

static int targetFillPx(int size) {
    switch (size) {
        case 16: return 14;
        case 20: return 18;
        case 24: return 22;
        case 30: return 27;
        case 32: return 28;
        case 36: return 32;
        case 40: return 36;
        case 48: return 42;
        case 64: return 56;
        default: return static_cast<int>(size * 0.9f + 0.5f);
    }
}

static bool findOpaqueBounds(const Image& img, int threshold, int* x0, int* y0, int* x1, int* y1) {
    int minX = img.w;
    int minY = img.h;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < img.h; ++y) {
        for (int x = 0; x < img.w; ++x) {
            if (img.rgba[(y * img.w + x) * 4 + 3] >= threshold) {
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
    }
    if (maxX < minX) {
        return false;
    }
    *x0 = minX;
    *y0 = minY;
    *x1 = maxX;
    *y1 = maxY;
    return true;
}

static Image extractCrop(const Image& src, int x0, int y0, int x1, int y1) {
    const int cw = x1 - x0 + 1;
    const int ch = y1 - y0 + 1;
    Image crop;
    crop.w = cw;
    crop.h = ch;
    crop.rgba.resize(static_cast<size_t>(cw * ch * 4));
    for (int y = 0; y < ch; ++y) {
        for (int x = 0; x < cw; ++x) {
            const int srcIdx = ((y0 + y) * src.w + (x0 + x)) * 4;
            const int dstIdx = (y * cw + x) * 4;
            std::memcpy(crop.rgba.data() + dstIdx, src.rgba.data() + srcIdx, 4);
        }
    }
    return crop;
}

static void blit(Image* dst, const Image& src, int ox, int oy) {
    for (int y = 0; y < src.h; ++y) {
        const int dy = oy + y;
        if (dy < 0 || dy >= dst->h) {
            continue;
        }
        for (int x = 0; x < src.w; ++x) {
            const int dx = ox + x;
            if (dx < 0 || dx >= dst->w) {
                continue;
            }
            const int srcIdx = (y * src.w + x) * 4;
            const float srcA = src.rgba[srcIdx + 3] / 255.0f;
            if (srcA <= 0.0f) {
                continue;
            }
            const int dstIdx = (dy * dst->w + dx) * 4;
            const float dstA = dst->rgba[dstIdx + 3] / 255.0f;
            const float outA = srcA + dstA * (1.0f - srcA);
            if (outA <= 0.0f) {
                continue;
            }
            for (int c = 0; c < 3; ++c) {
                const float srcC = src.rgba[srcIdx + c] / 255.0f;
                const float dstC = dst->rgba[dstIdx + c] / 255.0f;
                const float outC = (srcC * srcA + dstC * dstA * (1.0f - srcA)) / outA;
                dst->rgba[dstIdx + c] = static_cast<unsigned char>(outC * 255.0f + 0.5f);
            }
            dst->rgba[dstIdx + 3] = static_cast<unsigned char>(outA * 255.0f + 0.5f);
        }
    }
}

static Image composeForIcon(const Image& src, int size) {
    if (size >= 256) {
        return resizeSquare(src, size);
    }

    const Image base = resizeSquare(src, size);
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    if (!findOpaqueBounds(base, 48, &x0, &y0, &x1, &y1)) {
        return base;
    }

    const int cw = x1 - x0 + 1;
    const int ch = y1 - y0 + 1;
    const int fill = targetFillPx(size);
    const float scale = std::min(fill / static_cast<float>(cw), fill / static_cast<float>(ch));
    const int nw = std::max(1, static_cast<int>(cw * scale + 0.5f));
    const int nh = std::max(1, static_cast<int>(ch * scale + 0.5f));

    Image crop = extractCrop(base, x0, y0, x1, y1);
    Image scaled = resizeBox(crop, nw, nh);

    Image out;
    out.w = size;
    out.h = size;
    out.rgba.assign(static_cast<size_t>(size * size * 4), 0);
    const int ox = (size - nw) / 2;
    const int oy = (size - nh) / 2;
    blit(&out, scaled, ox, oy);
    return out;
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

static std::vector<unsigned char> encodeIcoBmp(const Image& img) {
    const int w = img.w;
    const int h = img.h;
    const int xorRowBytes = w * 4;
    const int xorSize = xorRowBytes * h;
    const int maskRowBytes = ((w + 31) / 32) * 4;
    const int maskSize = maskRowBytes * h;
    const int headerSize = 40;

    std::vector<unsigned char> out;
    out.reserve(static_cast<size_t>(headerSize + xorSize + maskSize));

    writeU32(out, 40);
    writeU32(out, static_cast<uint32_t>(w));
    writeU32(out, static_cast<uint32_t>(h * 2));
    writeU16(out, 1);
    writeU16(out, 32);
    writeU32(out, 0);
    writeU32(out, static_cast<uint32_t>(xorSize + maskSize));
    writeU32(out, 0);
    writeU32(out, 0);
    writeU32(out, 0);
    writeU32(out, 0);

    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            const int idx = (y * w + x) * 4;
            out.push_back(img.rgba[idx + 2]);
            out.push_back(img.rgba[idx + 1]);
            out.push_back(img.rgba[idx + 0]);
            out.push_back(img.rgba[idx + 3]);
        }
    }
    out.insert(out.end(), static_cast<size_t>(maskSize), 0);
    return out;
}

struct IcoEntry {
    int size = 0;
    std::vector<unsigned char> data;
    bool png = false;
};

static std::vector<unsigned char> buildIco(const std::vector<IcoEntry>& entries) {
    const int n = static_cast<int>(entries.size());
    const uint32_t header = 6u + static_cast<uint32_t>(n) * 16u;
    uint32_t offset = header;
    std::vector<unsigned char> out;
    out.reserve(header + 4096);
    writeU16(out, 0);
    writeU16(out, 1);
    writeU16(out, static_cast<uint16_t>(n));
    for (const IcoEntry& entry : entries) {
        const int w = entry.size;
        const unsigned char bw = static_cast<unsigned char>(w >= 256 ? 0 : w);
        out.push_back(bw);
        out.push_back(bw);
        out.push_back(0);
        out.push_back(0);
        writeU16(out, 1);
        writeU16(out, 32);
        writeU32(out, static_cast<uint32_t>(entry.data.size()));
        writeU32(out, offset);
        offset += static_cast<uint32_t>(entry.data.size());
    }
    for (const IcoEntry& entry : entries) {
        out.insert(out.end(), entry.data.begin(), entry.data.end());
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
    if (argc != 4) {
        std::fprintf(stderr, "usage: %s <art-dir> <output.ico> <png-out-dir>\n", argv[0]);
        return 1;
    }
    const std::string artDir = argv[1];
    const std::string outIco = argv[2];
    const std::string pngDir = argv[3];

    std::vector<IcoEntry> icoEntries;
    for (const Layer& layer : kLayers) {
        const std::string path = artDir + "/" + layer.file;
        const Image src = loadPng(path);
        const Image composed = composeForIcon(src, layer.size);
        const std::vector<unsigned char> png = encodePng(composed);

        char name[64];
        std::snprintf(name, sizeof(name), "app-%d.png", layer.size);
        writeBinary(pngDir + "/" + name, png);
        std::fprintf(stderr, "layer %d from %s (%dx%d)\n", layer.size, layer.file, src.w, src.h);

        IcoEntry entry;
        entry.size = layer.size;
        if (layer.size >= 256) {
            entry.png = true;
            entry.data = png;
        } else {
            entry.png = false;
            entry.data = encodeIcoBmp(composed);
        }
        icoEntries.push_back(std::move(entry));
    }

    const std::vector<unsigned char> ico = buildIco(icoEntries);
    writeBinary(outIco, ico);
    std::fprintf(stderr, "wrote %s (%zu bytes)\n", outIco.c_str(), ico.size());
    return 0;
}
