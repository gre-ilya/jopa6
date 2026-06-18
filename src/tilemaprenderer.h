#ifndef TILEMAPRENDERER_H
#define TILEMAPRENDERER_H

#include <QColor>
#include <QFont>
#include <QImage>
#include <QString>

#include "geopoint.h"

/**
 * @brief Visual styling for the rendered route.
 *
 * Every value has a sensible default, so a caller only overrides what it
 * cares about. Colours support alpha, which is used (for example) for the
 * white "halo" drawn behind the route line and the semi-transparent label
 * backgrounds that keep text readable over busy map imagery.
 */
struct RenderStyle
{
    // --- Route line -------------------------------------------------------
    bool drawLine = true;
    QColor lineColor = QColor(0xE5, 0x3E, 0x3E);        ///< Main line colour.
    qreal lineWidth = 4.0;                              ///< Main line width (px).
    bool lineHalo = true;                               ///< Draw a contrasting outline behind the line.
    QColor lineHaloColor = QColor(255, 255, 255, 220);  ///< Halo colour.
    qreal lineHaloWidth = 8.0;                          ///< Total width of the halo stroke (px).

    // --- Point markers ----------------------------------------------------
    bool drawMarkers = true;
    qreal markerRadius = 6.0;                           ///< Marker disc radius (px).
    QColor markerFill = QColor(0xE5, 0x3E, 0x3E);       ///< Marker fill colour.
    QColor markerOutline = QColor(255, 255, 255);       ///< Marker outline colour.
    qreal markerOutlineWidth = 2.0;                     ///< Marker outline width (px).

    // --- Labels -----------------------------------------------------------
    bool drawLabels = true;
    int labelFontPointSize = 11;                        ///< Font size if @c labelFont is unset.
    QFont labelFont;                                    ///< Optional explicit font (overrides size).
    QColor labelTextColor = QColor(0x20, 0x20, 0x20);   ///< Label text colour.
    QColor labelBackground = QColor(255, 255, 255, 220);///< Label pill background.
    QColor labelBorder = QColor(0, 0, 0, 40);           ///< Label pill border.
    qreal labelPadding = 4.0;                           ///< Padding inside the label pill (px).
    qreal labelOffset = 8.0;                            ///< Gap between marker and label (px).
    qreal labelCornerRadius = 4.0;                      ///< Rounding of the label pill corners.
    bool avoidLabelOverlap = true;                      ///< Try alternate anchors to reduce overlap.

    // --- Background -------------------------------------------------------
    QColor backgroundColor = QColor(0xE8, 0xE8, 0xE8);  ///< Shown where tiles are missing.
};

/**
 * @brief Configuration describing where the tiles live and how to frame them.
 */
struct RenderOptions
{
    QString tilesRoot;          ///< Directory holding the {z}/{x}/{y}.png pyramid.

    /// Zoom level to render. If < 0 the renderer picks the most detailed zoom
    /// that is available on disk and whose framed extent fits @c maxAutoOutputSize.
    int zoom = -1;

    /// Tile edge size in pixels. If <= 0 it is auto-detected from a sample tile
    /// (falling back to 256, the near-universal default).
    int tileSize = 0;

    /// Tile image extension without the dot (e.g. "png"). If empty it is
    /// auto-detected; loading also falls back to a list of common extensions.
    QString tileExtension;

    /// If true, tile Y indices follow the TMS convention (origin bottom-left).
    bool tms = false;

    /// Extra empty margin kept around the content, in pixels.
    int margin = 24;

    /// Upper bound (px, per axis) used when auto-selecting the zoom level.
    int maxAutoOutputSize = 4096;

    /// Hard safety cap (px, per axis). Rendering aborts above this to avoid
    /// accidentally allocating gigantic images from an unreasonable zoom.
    int hardMaxOutputSize = 16384;
};

/**
 * @brief Outcome of a render call, including diagnostics useful for logging.
 */
struct RenderResult
{
    bool ok = false;        ///< Whether a usable image was produced.
    QString error;          ///< Human readable error when @c ok is false.
    int zoom = -1;          ///< Zoom level actually used.
    int tileSize = 0;       ///< Tile size actually used.
    QSize imageSize;        ///< Dimensions of the produced image.
    int tilesDrawn = 0;     ///< Tiles successfully composited.
    int tilesMissing = 0;   ///< Tiles that were expected but could not be loaded.
    int pointsRendered = 0; ///< Valid points that were drawn.
    int pointsSkipped = 0;  ///< Points dropped for invalid coordinates.
};

/**
 * @brief Renders a labelled, connected route over pre-downloaded map tiles.
 *
 * Typical use as an embedded module:
 * @code
 *   TileMapRenderer renderer;
 *   RenderOptions opt; opt.tilesRoot = "/data/tiles";
 *   renderer.setOptions(opt);
 *
 *   QImage image;
 *   RenderResult r = renderer.render(points, &image);
 *   if (r.ok) image.save("route.png");
 * @endcode
 *
 * @note A @c QGuiApplication instance must exist before calling render(),
 *       because QPainter text rendering relies on the platform font stack.
 *       The host application normally provides this; the bundled CLI creates
 *       one explicitly (and works fine under the "offscreen" platform).
 *
 * The class keeps no per-render state, so a single instance may be reused
 * for many routes.
 */
class TileMapRenderer
{
public:
    TileMapRenderer() = default;

    void setOptions(const RenderOptions &options) { m_options = options; }
    const RenderOptions &options() const { return m_options; }

    void setStyle(const RenderStyle &style) { m_style = style; }
    const RenderStyle &style() const { return m_style; }

    /**
     * @brief Render @p points onto map tiles.
     * @param points    Ordered points to connect and label.
     * @param outImage  Receives the rendered image (untouched on failure).
     * @return A RenderResult describing the outcome.
     */
    RenderResult render(const GeoPath &points, QImage *outImage) const;

private:
    RenderOptions m_options;
    RenderStyle m_style;
};

#endif // TILEMAPRENDERER_H
