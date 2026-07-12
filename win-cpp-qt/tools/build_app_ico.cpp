// Pack native-size broombunny icon masters into app.ico (BMP <=64, PNG 256).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "third_party/stb_image.h"

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
    {256, "broombunny-256.png"},
    {64, "broombunny-64.png"},
    {48, "broombunny-48.png"},
    {40, "broombunny-40.png"},
    {32, "broombunny-32.png"},
    {24, "broombunny-24.png"},
    {20, "broombunny-20.png"},
    {16, "broombunny-16.png"},
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

static Image loadPng(const std::string& path, int expectedSize) {
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
    if (w != expectedSize || h != expectedSize) {
        std::fprintf(stderr,
                     "icon master has wrong dimensions: %s is %dx%d, expected %dx%d\n",
                     path.c_str(),
                     w,
                     h,
                     expectedSize,
                     expectedSize);
        std::exit(1);
    }
    Image img;
    img.w = w;
    img.h = h;
    img.rgba.assign(pixels, pixels + (w * h * 4));
    stbi_image_free(pixels);
    return img;
}

static std::vector<unsigned char> readPngBytes(const std::string& path) {
    return readFile(path);
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

    std::vector<unsigned char> out;
    out.reserve(static_cast<size_t>(40 + xorSize + maskSize));

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
};

static std::vector<unsigned char> buildIco(const std::vector<IcoEntry>& entries) {
    const int n = static_cast<int>(entries.size());
    const uint32_t header = 6u + static_cast<uint32_t>(n) * 16u;
    uint32_t offset = header;
    std::vector<unsigned char> out;
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

static void copyBinary(const std::string& from, const std::string& to) {
    writeBinary(to, readFile(from));
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: %s <masters-dir> <output.ico> <qrc-icons-dir>\n", argv[0]);
        return 1;
    }
    const std::string mastersDir = argv[1];
    const std::string outIco = argv[2];
    const std::string qrcDir = argv[3];

    std::vector<IcoEntry> icoEntries;
    for (const Layer& layer : kLayers) {
        const std::string path = mastersDir + "/" + layer.file;
        const Image img = loadPng(path, layer.size);

        char qrcName[64];
        std::snprintf(qrcName, sizeof(qrcName), "app-%d.png", layer.size);
        copyBinary(path, qrcDir + "/" + qrcName);

        IcoEntry entry;
        entry.size = layer.size;
        if (layer.size >= 256) {
            entry.data = readPngBytes(path);
        } else {
            entry.data = encodeIcoBmp(img);
        }
        icoEntries.push_back(std::move(entry));
        std::fprintf(stderr, "packed %s\n", path.c_str());
    }

    writeBinary(outIco, buildIco(icoEntries));
    std::fprintf(stderr, "wrote %s\n", outIco.c_str());
    return 0;
}
