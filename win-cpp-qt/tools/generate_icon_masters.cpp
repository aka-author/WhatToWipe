// Generate native-size broombunny icon masters from high-res source art.
// Crop content on the source image BEFORE downscaling (fixes build 0027 bug).

#include <algorithm>
#include <cmath>
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

struct LayerSpec {
    int size;
    int fillPx;
};

static const LayerSpec kLayers[] = {
    {16, 15},
    {20, 18},
    {24, 22},
    {32, 28},
    {40, 36},
    {48, 42},
    {64, 56},
    {256, 240},
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

static void writePng(const std::string& path, const Image& img) {
    if (!stbi_write_png(path.c_str(), img.w, img.h, 4, img.rgba.data(), img.w * 4)) {
        std::fprintf(stderr, "png write failed: %s\n", path.c_str());
        std::exit(1);
    }
}

static void sampleBackground(const Image& img, unsigned char bg[3]) {
    long long r = 0;
    long long g = 0;
    long long b = 0;
    int count = 0;
    for (int y = 0; y < img.h; ++y) {
        for (int x = 0; x < img.w; ++x) {
            const unsigned char* px = &img.rgba[(y * img.w + x) * 4];
            if (px[3] < 200) {
                continue;
            }
            if (px[2] <= px[0] || px[2] <= px[1] + 20) {
                continue;
            }
            r += px[0];
            g += px[1];
            b += px[2];
            ++count;
        }
    }
    if (count == 0) {
        bg[0] = 106;
        bg[1] = 76;
        bg[2] = 219;
        return;
    }
    bg[0] = static_cast<unsigned char>(r / count);
    bg[1] = static_cast<unsigned char>(g / count);
    bg[2] = static_cast<unsigned char>(b / count);
}

static bool isPurpleBackground(const unsigned char* px, const unsigned char bg[3]) {
    const int dr = static_cast<int>(px[0]) - static_cast<int>(bg[0]);
    const int dg = static_cast<int>(px[1]) - static_cast<int>(bg[1]);
    const int db = static_cast<int>(px[2]) - static_cast<int>(bg[2]);
    return std::abs(dr) + std::abs(dg) + std::abs(db) < 56;
}

static bool isForegroundPixel(const unsigned char* px, const unsigned char bg[3]) {
    if (px[3] < 32) {
        return false;
    }
    if (isPurpleBackground(px, bg)) {
        return false;
    }
    const int lum = (static_cast<int>(px[0]) + static_cast<int>(px[1]) + static_cast<int>(px[2])) / 3;
    if (lum > 180) {
        return true;
    }
    if (px[0] > 120 && px[1] > 90) {
        return true;
    }
    return lum > 60;
}

static bool findForegroundBounds(const Image& img, const unsigned char bg[3], int* x0, int* y0, int* x1, int* y1) {
    int minX = img.w;
    int minY = img.h;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < img.h; ++y) {
        for (int x = 0; x < img.w; ++x) {
            const unsigned char* px = &img.rgba[(y * img.w + x) * 4];
            if (!isForegroundPixel(px, bg)) {
                continue;
            }
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
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
            const float fx0 = (x * static_cast<float>(src.w)) / dstW;
            const float fy0 = (y * static_cast<float>(src.h)) / dstH;
            const float fx1 = ((x + 1) * static_cast<float>(src.w)) / dstW;
            const float fy1 = ((y + 1) * static_cast<float>(src.h)) / dstH;
            const int x0 = static_cast<int>(fx0);
            const int y0 = static_cast<int>(fy0);
            const int x1 = std::min(src.w, static_cast<int>(std::ceil(fx1)));
            const int y1 = std::min(src.h, static_cast<int>(std::ceil(fy1)));
            const int count = std::max(1, (x1 - x0) * (y1 - y0));
            float acc[4] = {0, 0, 0, 0};
            for (int sy = y0; sy < y1; ++sy) {
                for (int sx = x0; sx < x1; ++sx) {
                    const int idx = (sy * src.w + sx) * 4;
                    for (int c = 0; c < 4; ++c) {
                        acc[c] += src.rgba[idx + c];
                    }
                }
            }
            const int out = (y * dstW + x) * 4;
            for (int c = 0; c < 4; ++c) {
                dst.rgba[out + c] = static_cast<unsigned char>(acc[c] / count + 0.5f);
            }
        }
    }
    return dst;
}

static Image resizeMultiStep(const Image& src, int dstW, int dstH) {
    Image current = src;
    while (current.w > dstW * 2 || current.h > dstH * 2) {
        const int nextW = std::max(dstW, current.w / 2);
        const int nextH = std::max(dstH, current.h / 2);
        current = resizeBox(current, nextW, nextH);
    }
    return resizeBox(current, dstW, dstH);
}

static Image makeSolidCanvas(int size, const unsigned char bg[3]) {
    Image canvas;
    canvas.w = size;
    canvas.h = size;
    canvas.rgba.assign(static_cast<size_t>(size * size * 4), 255);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int idx = (y * size + x) * 4;
            canvas.rgba[idx + 0] = bg[0];
            canvas.rgba[idx + 1] = bg[1];
            canvas.rgba[idx + 2] = bg[2];
            canvas.rgba[idx + 3] = 255;
        }
    }
    return canvas;
}

static void blitOpaque(Image* dst, const Image& src, int ox, int oy) {
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
            if (src.rgba[srcIdx + 3] < 16) {
                continue;
            }
            const int dstIdx = (dy * dst->w + dx) * 4;
            dst->rgba[dstIdx + 0] = src.rgba[srcIdx + 0];
            dst->rgba[dstIdx + 1] = src.rgba[srcIdx + 1];
            dst->rgba[dstIdx + 2] = src.rgba[srcIdx + 2];
            dst->rgba[dstIdx + 3] = 255;
        }
    }
}

static Image composeLayer(const Image& foregroundCrop, int size, int fillPx, const unsigned char bg[3]) {
    const float scale = std::min(fillPx / static_cast<float>(foregroundCrop.w),
                                 fillPx / static_cast<float>(foregroundCrop.h));
    const int nw = std::max(1, static_cast<int>(foregroundCrop.w * scale + 0.5f));
    const int nh = std::max(1, static_cast<int>(foregroundCrop.h * scale + 0.5f));
    Image scaled = resizeMultiStep(foregroundCrop, nw, nh);
    Image canvas = makeSolidCanvas(size, bg);
    const int ox = (size - nw) / 2;
    const int oy = (size - nh) / 2;
    blitOpaque(&canvas, scaled, ox, oy);
    return canvas;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <art-dir> <icons-out-dir>\n", argv[0]);
        return 1;
    }
    const std::string artDir = argv[1];
    const std::string outDir = argv[2];
    const std::string sourcePath = artDir + "/broombunny.png";

    const Image source = loadPng(sourcePath);
    unsigned char bg[3] = {};
    sampleBackground(source, bg);

    int x0 = 0;
    int y0 = 0;
    int x1 = source.w - 1;
    int y1 = source.h - 1;
    if (!findForegroundBounds(source, bg, &x0, &y0, &x1, &y1)) {
        std::fprintf(stderr, "no foreground content detected in %s\n", sourcePath.c_str());
        return 1;
    }

    const int marginX = std::max(1, (x1 - x0 + 1) / 40);
    const int marginY = std::max(1, (y1 - y0 + 1) / 40);
    x0 = std::max(0, x0 - marginX);
    y0 = std::max(0, y0 - marginY);
    x1 = std::min(source.w - 1, x1 + marginX);
    y1 = std::min(source.h - 1, y1 + marginY);

    const Image foreground = extractCrop(source, x0, y0, x1, y1);
    std::fprintf(stderr,
                 "foreground crop %dx%d from %dx%d source (bg %d,%d,%d)\n",
                 foreground.w,
                 foreground.h,
                 source.w,
                 source.h,
                 bg[0],
                 bg[1],
                 bg[2]);

    for (const LayerSpec& layer : kLayers) {
        Image composed = composeLayer(foreground, layer.size, layer.fillPx, bg);
        char name[64];
        std::snprintf(name, sizeof(name), "broombunny-%d.png", layer.size);
        const std::string path = outDir + "/" + name;
        writePng(path, composed);
        std::fprintf(stderr, "wrote %s (fill %d)\n", path.c_str(), layer.fillPx);
    }
    return 0;
}
