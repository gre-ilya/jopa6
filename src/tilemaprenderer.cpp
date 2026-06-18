#include "tilemaprenderer.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <limits>

#include "mercator.h"

namespace {

// Candidate extensions tried when loading a tile whose exact extension is
// unknown or whose primary file is absent.
const QStringList kKnownExtensions = {"png", "jpg", "jpeg", "webp", "gif", "bmp"};

/// Returns true if @p name is a non-negative integer (a valid z / x / y dir).
bool isNonNegativeInteger(const QString &name)
{
    if (name.isEmpty())
        return false;
    for (const QChar c : name) {
        if (!c.isDigit())
            return false;
    }
    return true;
}

/// Discover the zoom levels present under @p tilesRoot, ascending.
QList<int> discoverZoomLevels(const QString &tilesRoot)
{
    QList<int> zooms;
    QDir root(tilesRoot);
    const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        if (isNonNegativeInteger(entry))
            zooms.append(entry.toInt());
    }
    std::sort(zooms.begin(), zooms.end());
    return zooms;
}

/// The font that labels will be drawn with, honouring the style overrides.
QFont labelFontFor(const RenderStyle &style)
{
    QFont font = style.labelFont;
    if (font.family().isEmpty() && style.labelFontPointSize > 0)
        font.setPointSize(style.labelFontPointSize);
    return font;
}

} // namespace

/// Build an absolute path to a tile, probing extensions when needed.
/// Returns an empty string if no candidate file exists.
static QString resolveTilePath(const QString &tilesRoot, int zoom, int x, int y,
                               const QString &preferredExt)
{
    const QString dir = QStringLiteral("%1/%2/%3").arg(tilesRoot).arg(zoom).arg(x);

    QStringList exts;
    if (!preferredExt.isEmpty())
        exts << preferredExt;
    for (const QString &e : kKnownExtensions) {
        if (!exts.contains(e, Qt::CaseInsensitive))
            exts << e;
    }

    for (const QString &ext : exts) {
        const QString path = QStringLiteral("%1/%2.%3").arg(dir).arg(y).arg(ext);
        if (QFileInfo::exists(path))
            return path;
    }
    return QString();
}

/// Detect tile size and extension from the first readable tile under the root.
/// @p tileSize / @p extension are only written when successfully detected.
static void detectTileProperties(const QString &tilesRoot, int *tileSize, QString *extension)
{
    QDirIterator it(tilesRoot, QStringList{"*.png", "*.jpg", "*.jpeg", "*.webp", "*.gif", "*.bmp"},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        QImageReader reader(path);
        const QSize size = reader.size();
        if (size.isValid()) {
            if (tileSize && *tileSize <= 0 && size.width() > 0)
                *tileSize = size.width();
            if (extension && extension->isEmpty())
                *extension = QFileInfo(path).suffix().toLower();
            return;
        }
    }
}

RenderResult TileMapRenderer::render(const GeoPath &points, QImage *outImage) const
{
    RenderResult result;

    if (!outImage) {
        result.error = QStringLiteral("Output image pointer is null.");
        return result;
    }
    if (points.isEmpty()) {
        result.error = QStringLiteral("No points were supplied.");
        return result;
    }

    const QString tilesRoot = QDir::cleanPath(m_options.tilesRoot);
    if (tilesRoot.isEmpty() || !QFileInfo(tilesRoot).isDir()) {
        result.error = QStringLiteral("Tiles directory does not exist: %1").arg(m_options.tilesRoot);
        return result;
    }

    // --- Resolve tile size and extension --------------------------------
    int tileSize = m_options.tileSize;
    QString extension = m_options.tileExtension.toLower();
    if (tileSize <= 0 || extension.isEmpty())
        detectTileProperties(tilesRoot, &tileSize, &extension);
    if (tileSize <= 0)
        tileSize = 256; // Universal slippy-map default.
    result.tileSize = tileSize;

    // --- Filter to points with usable coordinates -----------------------
    GeoPath valid;
    valid.reserve(points.size());
    for (const GeoPoint &p : points) {
        const bool finite = std::isfinite(p.lon) && std::isfinite(p.lat);
        const bool inRange = p.lon >= -180.0 && p.lon <= 180.0 && p.lat >= -90.0 && p.lat <= 90.0;
        if (finite && inRange)
            valid.append(p);
        else
            ++result.pointsSkipped;
    }
    if (valid.isEmpty()) {
        result.error = QStringLiteral("All supplied points had invalid coordinates.");
        return result;
    }

    // --- Choose a zoom level --------------------------------------------
    const QList<int> available = discoverZoomLevels(tilesRoot);
    int zoom = m_options.zoom;
    if (zoom < 0) {
        if (available.isEmpty()) {
            result.error = QStringLiteral(
                "Could not find any numeric zoom directories under %1, and no "
                "explicit zoom was given.").arg(tilesRoot);
            return result;
        }
        // Pick the most detailed zoom whose framed extent fits the auto budget.
        // A coarse label/marker allowance is added so the heuristic is conservative.
        const double allowance = 2.0 * m_options.margin + 256.0;
        zoom = available.first();
        for (auto i = available.crbegin(); i != available.crend(); ++i) {
            const int z = *i;
            double minX = std::numeric_limits<double>::max(), minY = minX;
            double maxX = std::numeric_limits<double>::lowest(), maxY = maxX;
            for (const GeoPoint &p : valid) {
                minX = std::min(minX, mercator::lonToPixelX(p.lon, z, tileSize));
                maxX = std::max(maxX, mercator::lonToPixelX(p.lon, z, tileSize));
                minY = std::min(minY, mercator::latToPixelY(p.lat, z, tileSize));
                maxY = std::max(maxY, mercator::latToPixelY(p.lat, z, tileSize));
            }
            if ((maxX - minX) + allowance <= m_options.maxAutoOutputSize
                && (maxY - minY) + allowance <= m_options.maxAutoOutputSize) {
                zoom = z;
                break;
            }
            zoom = available.first(); // keep the coarsest as the fallback
        }
    }
    result.zoom = zoom;

    // --- Project points into global pixel space at the chosen zoom -------
    QVector<QPointF> globalPts;
    globalPts.reserve(valid.size());
    for (const GeoPoint &p : valid) {
        globalPts.append(QPointF(mercator::lonToPixelX(p.lon, zoom, tileSize),
                                 mercator::latToPixelY(p.lat, zoom, tileSize)));
    }

    // --- Lay out labels (also needed to size the canvas) -----------------
    const QFont font = labelFontFor(m_style);
    const QFontMetricsF fm(font);
    const qreal pad = m_style.labelPadding;
    const qreal anchorGap = m_style.markerRadius + m_style.labelOffset;

    struct LabelBox { bool visible = false; QRectF rect; QString text; };
    QVector<LabelBox> labels(globalPts.size());
    QVector<QRectF> placed; // already-placed label rects, for overlap avoidance

    if (m_style.drawLabels) {
        for (int i = 0; i < globalPts.size(); ++i) {
            const QString text = valid[i].name;
            if (text.isEmpty())
                continue;
            const QRectF tb = fm.boundingRect(text);
            const qreal w = tb.width() + 2 * pad;
            const qreal h = fm.height() + 2 * pad;
            const QPointF c = globalPts[i];

            // Candidate anchors in priority order: right, left, above, below.
            const QVector<QRectF> candidates = {
                QRectF(c.x() + anchorGap, c.y() - h / 2.0, w, h),
                QRectF(c.x() - anchorGap - w, c.y() - h / 2.0, w, h),
                QRectF(c.x() - w / 2.0, c.y() - anchorGap - h, w, h),
                QRectF(c.x() - w / 2.0, c.y() + anchorGap, w, h),
            };

            QRectF chosen = candidates.first();
            if (m_style.avoidLabelOverlap) {
                for (const QRectF &cand : candidates) {
                    bool clash = false;
                    for (const QRectF &pr : placed) {
                        if (cand.intersects(pr)) { clash = true; break; }
                    }
                    if (!clash) { chosen = cand; break; }
                }
            }
            placed.append(chosen);
            labels[i] = LabelBox{true, chosen, text};
        }
    }

    // --- Determine the content bounding box in global pixel space --------
    double minX = std::numeric_limits<double>::max(), minY = minX;
    double maxX = std::numeric_limits<double>::lowest(), maxY = maxX;
    auto expand = [&](double x, double y) {
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
    };

    const qreal markerReach = m_style.markerRadius + m_style.markerOutlineWidth
                              + (m_style.lineHalo ? m_style.lineHaloWidth : m_style.lineWidth);
    for (const QPointF &p : globalPts) {
        expand(p.x() - markerReach, p.y() - markerReach);
        expand(p.x() + markerReach, p.y() + markerReach);
    }
    for (const LabelBox &lb : labels) {
        if (lb.visible) {
            expand(lb.rect.left(), lb.rect.top());
            expand(lb.rect.right(), lb.rect.bottom());
        }
    }

    // Apply the configured margin and snap to integers.
    minX = std::floor(minX - m_options.margin);
    minY = std::floor(minY - m_options.margin);
    maxX = std::ceil(maxX + m_options.margin);
    maxY = std::ceil(maxY + m_options.margin);

    const QPointF origin(minX, minY); // global pixel -> canvas offset
    const int canvasW = static_cast<int>(maxX - minX);
    const int canvasH = static_cast<int>(maxY - minY);

    if (canvasW <= 0 || canvasH <= 0) {
        result.error = QStringLiteral("Computed an empty canvas (%1x%2).").arg(canvasW).arg(canvasH);
        return result;
    }
    if (canvasW > m_options.hardMaxOutputSize || canvasH > m_options.hardMaxOutputSize) {
        result.error = QStringLiteral(
            "Refusing to render: canvas %1x%2 exceeds the safety limit of %3 px "
            "(try a lower zoom level).")
            .arg(canvasW).arg(canvasH).arg(m_options.hardMaxOutputSize);
        return result;
    }

    // --- Allocate the canvas and paint -----------------------------------
    QImage image(canvasW, canvasH, QImage::Format_ARGB32_Premultiplied);
    image.fill(m_style.backgroundColor);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 1) Composite the background tiles that intersect the canvas.
    const int tilesAxis = static_cast<int>(mercator::tilesPerAxis(zoom));
    int tileX0 = static_cast<int>(std::floor(origin.x() / tileSize));
    int tileY0 = static_cast<int>(std::floor(origin.y() / tileSize));
    int tileX1 = static_cast<int>(std::floor((origin.x() + canvasW - 1) / tileSize));
    int tileY1 = static_cast<int>(std::floor((origin.y() + canvasH - 1) / tileSize));
    tileX0 = std::max(0, tileX0);
    tileY0 = std::max(0, tileY0);
    tileX1 = std::min(tilesAxis - 1, tileX1);
    tileY1 = std::min(tilesAxis - 1, tileY1);

    for (int tx = tileX0; tx <= tileX1; ++tx) {
        for (int ty = tileY0; ty <= tileY1; ++ty) {
            const int fileY = m_options.tms ? mercator::flipTileY(ty, zoom) : ty;
            const QString path = resolveTilePath(tilesRoot, zoom, tx, fileY, extension);
            const QPointF dest(tx * tileSize - origin.x(), ty * tileSize - origin.y());
            if (path.isEmpty()) {
                ++result.tilesMissing;
                continue;
            }
            QImage tile(path);
            if (tile.isNull()) {
                ++result.tilesMissing;
                qWarning("TileMapRenderer: failed to decode tile %s", qUtf8Printable(path));
                continue;
            }
            painter.drawImage(dest, tile);
            ++result.tilesDrawn;
        }
    }

    // 2) Route line (halo first, then the coloured stroke on top).
    if (m_style.drawLine && globalPts.size() >= 2) {
        QPolygonF poly;
        poly.reserve(globalPts.size());
        for (const QPointF &p : globalPts)
            poly << (p - origin);

        if (m_style.lineHalo) {
            QPen halo(m_style.lineHaloColor, m_style.lineHaloWidth);
            halo.setCapStyle(Qt::RoundCap);
            halo.setJoinStyle(Qt::RoundJoin);
            painter.setPen(halo);
            painter.drawPolyline(poly);
        }
        QPen pen(m_style.lineColor, m_style.lineWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.drawPolyline(poly);
    }

    // 3) Point markers.
    if (m_style.drawMarkers) {
        QPen outline(m_style.markerOutline, m_style.markerOutlineWidth);
        painter.setPen(m_style.markerOutlineWidth > 0 ? outline : Qt::NoPen);
        painter.setBrush(m_style.markerFill);
        for (const QPointF &p : globalPts) {
            const QPointF c = p - origin;
            painter.drawEllipse(c, m_style.markerRadius, m_style.markerRadius);
        }
    }

    // 4) Labels (pill background + text).
    if (m_style.drawLabels) {
        painter.setFont(font);
        for (const LabelBox &lb : labels) {
            if (!lb.visible)
                continue;
            const QRectF rect = lb.rect.translated(-origin);
            QPainterPath pill;
            pill.addRoundedRect(rect, m_style.labelCornerRadius, m_style.labelCornerRadius);
            painter.setPen(m_style.labelBorder.alpha() > 0 ? QPen(m_style.labelBorder, 1.0)
                                                           : QPen(Qt::NoPen));
            painter.setBrush(m_style.labelBackground);
            painter.drawPath(pill);

            painter.setPen(m_style.labelTextColor);
            painter.drawText(rect, Qt::AlignCenter, lb.text);
        }
    }

    painter.end();

    *outImage = image;
    result.imageSize = image.size();
    result.pointsRendered = valid.size();
    result.ok = true;
    return result;
}
