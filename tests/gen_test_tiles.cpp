// Helper that synthesises a {z}/{x}/{y}.png tile pyramid covering a set of
// points, so the renderer can be exercised end-to-end without a real tile
// cache. Each tile is a light checkerboard cell stamped with its z/x/y index
// and a grid, which makes tile alignment and the XYZ/TMS axis easy to verify
// visually. This is a TEST helper, not part of the shipped product.

#include <QColor>
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <limits>

#include "mercator.h"
#include "pointsio.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Generate synthetic test tiles."));
    parser.addHelpOption();
    parser.addOption({{"p", "points"}, QStringLiteral("Points file."), QStringLiteral("file")});
    parser.addOption({{"o", "tiles"}, QStringLiteral("Output tiles dir."), QStringLiteral("dir")});
    parser.addOption({{"z", "zoom"}, QStringLiteral("Zoom level."), QStringLiteral("n"), QStringLiteral("15")});
    parser.addOption({"tile-size", QStringLiteral("Tile size px."), QStringLiteral("n"), QStringLiteral("256")});
    parser.addOption({"pad", QStringLiteral("Extra tile rings around the bbox."), QStringLiteral("n"), QStringLiteral("1")});
    parser.addOption({"tms", QStringLiteral("Write filenames with the TMS Y convention.")});
    parser.process(app);

    QTextStream errs(stderr);
    if (!parser.isSet("points") || !parser.isSet("tiles")) {
        errs << "error: --points and --tiles are required.\n";
        return 2;
    }

    QString loadError;
    const GeoPath points = pointsio::load(parser.value("points"), &loadError);
    if (points.isEmpty()) {
        errs << "error: " << (loadError.isEmpty() ? QStringLiteral("no points") : loadError) << '\n';
        return 1;
    }

    const int zoom = parser.value("zoom").toInt();
    const int tileSize = parser.value("tile-size").toInt();
    const int pad = parser.value("pad").toInt();
    const bool tms = parser.isSet("tms");
    const QString root = parser.value("tiles");

    int minX = std::numeric_limits<int>::max(), minY = minX;
    int maxX = std::numeric_limits<int>::min(), maxY = maxX;
    for (const GeoPoint &p : points) {
        const int tx = static_cast<int>(std::floor(mercator::lonToTileX(p.lon, zoom)));
        const int ty = static_cast<int>(std::floor(mercator::latToTileY(p.lat, zoom)));
        minX = std::min(minX, tx); maxX = std::max(maxX, tx);
        minY = std::min(minY, ty); maxY = std::max(maxY, ty);
    }
    minX -= pad; minY -= pad; maxX += pad; maxY += pad;

    const int axis = static_cast<int>(mercator::tilesPerAxis(zoom));
    minX = std::max(0, minX); minY = std::max(0, minY);
    maxX = std::min(axis - 1, maxX); maxY = std::min(axis - 1, maxY);

    QFont font;
    font.setPointSize(std::max(8, tileSize / 22));
    int written = 0;
    for (int tx = minX; tx <= maxX; ++tx) {
        for (int ty = minY; ty <= maxY; ++ty) {
            QImage img(tileSize, tileSize, QImage::Format_RGB32);
            const bool even = ((tx + ty) % 2) == 0;
            img.fill(even ? QColor(0xF2, 0xF0, 0xE8) : QColor(0xE5, 0xE9, 0xDE));

            QPainter pr(&img);
            pr.setRenderHint(QPainter::Antialiasing, true);
            pr.setPen(QPen(QColor(0xC4, 0xC4, 0xBE), 1));
            for (int g = 0; g <= tileSize; g += tileSize / 4)
                { pr.drawLine(g, 0, g, tileSize); pr.drawLine(0, g, tileSize, g); }
            pr.setPen(QPen(QColor(0x9A, 0x9A, 0x92), 2));
            pr.drawRect(0, 0, tileSize - 1, tileSize - 1);
            pr.setFont(font);
            pr.setPen(QColor(0x70, 0x70, 0x68));
            pr.drawText(img.rect(), Qt::AlignCenter,
                        QStringLiteral("%1/%2/%3").arg(zoom).arg(tx).arg(ty));
            pr.end();

            const int fileY = tms ? mercator::flipTileY(ty, zoom) : ty;
            const QString dir = QStringLiteral("%1/%2/%3").arg(root).arg(zoom).arg(tx);
            QDir().mkpath(dir);
            const QString path = QStringLiteral("%1/%2.png").arg(dir).arg(fileY);
            if (img.save(path))
                ++written;
        }
    }

    QTextStream(stdout) << "Generated " << written << " tiles at zoom " << zoom
                        << " under " << root << '\n';
    return 0;
}
