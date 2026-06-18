// Command-line test harness for the tilemap rendering module.
//
// It reads a list of points from a file, renders them as a connected,
// labelled route over locally-stored map tiles, and writes the result to an
// image file. The actual rendering lives in the reusable `tilemap` library
// (TileMapRenderer); this file only deals with argument parsing and I/O.

#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QTextStream>

#include "pointsio.h"
#include "tilemaprenderer.h"

namespace {

QTextStream &err()
{
    static QTextStream s(stderr);
    return s;
}

QTextStream &out()
{
    static QTextStream s(stdout);
    return s;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("mapgen"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.2.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Render a labelled, connected route of geographic points "
                       "over locally-stored {z}/{x}/{y} map tiles."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption tilesOpt({"t", "tiles"},
        QStringLiteral("Directory containing the {z}/{x}/{y}.png tile pyramid."),
        QStringLiteral("dir"));
    const QCommandLineOption pointsOpt({"p", "points"},
        QStringLiteral("Input points file (.csv/.txt, .json or .geojson)."),
        QStringLiteral("file"));
    const QCommandLineOption outOpt({"o", "out"},
        QStringLiteral("Output image path (format inferred from extension)."),
        QStringLiteral("file"), QStringLiteral("route.png"));
    const QCommandLineOption zoomOpt({"z", "zoom"},
        QStringLiteral("Zoom level to render. Omitted or -1 = auto-select."),
        QStringLiteral("n"), QStringLiteral("-1"));
    const QCommandLineOption tileSizeOpt("tile-size",
        QStringLiteral("Tile edge size in px (0 = auto-detect)."),
        QStringLiteral("n"), QStringLiteral("0"));
    const QCommandLineOption tmsOpt("tms",
        QStringLiteral("Tiles use the TMS Y convention (origin bottom-left)."));
    const QCommandLineOption lonLatOpt("lon-lat",
        QStringLiteral("CSV columns are 'lon,lat,name' instead of the default 'lat,lon,name'."));
    const QCommandLineOption marginOpt("margin",
        QStringLiteral("Empty margin around the content in px."),
        QStringLiteral("n"), QStringLiteral("24"));
    const QCommandLineOption maxSizeOpt("max-size",
        QStringLiteral("Max per-axis size (px) used for auto zoom selection."),
        QStringLiteral("n"), QStringLiteral("4096"));
    const QCommandLineOption qualityOpt("quality",
        QStringLiteral("Output quality 0-100 for lossy formats (e.g. JPEG)."),
        QStringLiteral("n"), QStringLiteral("92"));

    parser.addOptions({tilesOpt, pointsOpt, outOpt, zoomOpt, tileSizeOpt, tmsOpt,
                       lonLatOpt, marginOpt, maxSizeOpt, qualityOpt});
    parser.process(app);

    if (!parser.isSet(tilesOpt) || !parser.isSet(pointsOpt)) {
        err() << "error: both --tiles and --points are required.\n\n";
        err() << parser.helpText();
        err().flush();
        return 2;
    }

    // --- Load points -----------------------------------------------------
    QString loadError;
    const auto csvOrder = parser.isSet(lonLatOpt) ? pointsio::CsvOrder::LonLat
                                                  : pointsio::CsvOrder::LatLon;
    const GeoPath points = pointsio::load(parser.value(pointsOpt), &loadError,
                                          pointsio::Format::Auto, csvOrder);
    if (points.isEmpty()) {
        err() << "error: could not load points: "
              << (loadError.isEmpty() ? QStringLiteral("file is empty") : loadError) << '\n';
        err().flush();
        return 1;
    }

    // --- Configure renderer ---------------------------------------------
    RenderOptions options;
    options.tilesRoot = parser.value(tilesOpt);
    options.zoom = parser.value(zoomOpt).toInt();
    options.tileSize = parser.value(tileSizeOpt).toInt();
    options.tms = parser.isSet(tmsOpt);
    options.margin = parser.value(marginOpt).toInt();
    options.maxAutoOutputSize = parser.value(maxSizeOpt).toInt();

    TileMapRenderer renderer;
    renderer.setOptions(options);

    // --- Render ----------------------------------------------------------
    QImage image;
    const RenderResult r = renderer.render(points, &image);
    if (!r.ok) {
        err() << "error: " << r.error << '\n';
        err().flush();
        return 1;
    }

    const QString outPath = parser.value(outOpt);
    const int quality = parser.value(qualityOpt).toInt();
    if (!image.save(outPath, nullptr, quality)) {
        err() << "error: failed to write image to " << outPath << '\n';
        err().flush();
        return 1;
    }

    out() << "Wrote " << outPath << " ("
          << r.imageSize.width() << "x" << r.imageSize.height() << " px)\n"
          << "  zoom level   : " << r.zoom << " (tile size " << r.tileSize << " px)\n"
          << "  points       : " << r.pointsRendered << " rendered";
    if (r.pointsSkipped > 0)
        out() << ", " << r.pointsSkipped << " skipped (invalid)";
    out() << '\n'
          << "  tiles        : " << r.tilesDrawn << " drawn";
    if (r.tilesMissing > 0)
        out() << ", " << r.tilesMissing << " missing";
    out() << '\n';
    out().flush();
    return 0;
}
