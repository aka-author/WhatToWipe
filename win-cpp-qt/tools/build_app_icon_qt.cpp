// Rasterize assets/icons/app.svg (FS main-window icon) to PNG layers and app.ico.
// Built with the shared Qt MinGW kit (see build.ps1 Generate-AppIcon).

#include <QBuffer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static const int kSizes[] = {16, 20, 24, 32, 48, 64, 256};

static QByteArray renderSvgLayer(QSvgRenderer& renderer, int size) {
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter, QRectF(0, 0, size, size));
    painter.end();

    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly)) {
        std::fprintf(stderr, "png buffer open failed for size %d\n", size);
        std::exit(1);
    }
    if (!image.save(&buffer, "PNG")) {
        std::fprintf(stderr, "png encode failed for size %d\n", size);
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

static std::vector<unsigned char> buildIco(const std::vector<QByteArray>& pngs,
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
    for (const QByteArray& png : pngs) {
        out.insert(out.end(), png.begin(), png.end());
    }
    return out;
}

static bool writeBinaryFile(const QString& path, const QByteArray& data) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        std::fprintf(stderr, "write failed: %s\n", path.toLocal8Bit().constData());
        return false;
    }
    if (file.write(data) != data.size()) {
        std::fprintf(stderr, "write incomplete: %s\n", path.toLocal8Bit().constData());
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: %s <app.svg> <output.ico> <png-out-dir>\n", argv[0]);
        return 1;
    }

    QGuiApplication app(argc, argv);

    const QString svgPath = QString::fromLocal8Bit(argv[1]);
    const QString outIco = QString::fromLocal8Bit(argv[2]);
    const QString pngDir = QString::fromLocal8Bit(argv[3]);

    QSvgRenderer renderer(svgPath);
    if (!renderer.isValid()) {
        std::fprintf(stderr, "invalid svg: %s\n", svgPath.toLocal8Bit().constData());
        return 1;
    }

    std::vector<QByteArray> pngs;
    std::vector<int> sizes;
    for (int size : kSizes) {
        const QByteArray png = renderSvgLayer(renderer, size);
        const QString pngPath = pngDir + QStringLiteral("/app-%1.png").arg(size);
        if (!writeBinaryFile(pngPath, png)) {
            return 1;
        }
        pngs.push_back(png);
        sizes.push_back(size);
        std::fprintf(stderr, "layer %d from app.svg\n", size);
    }

    const std::vector<unsigned char> ico = buildIco(pngs, sizes);
    if (!writeBinaryFile(outIco, QByteArray(reinterpret_cast<const char*>(ico.data()),
                                            static_cast<int>(ico.size())))) {
        return 1;
    }
    std::fprintf(stderr, "wrote %s (%zu bytes)\n", outIco.toLocal8Bit().constData(), ico.size());
    return 0;
}
