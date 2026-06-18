#ifndef MERCATOR_H
#define MERCATOR_H

#include <cmath>

/**
 * @brief Spherical Web-Mercator (EPSG:3857) helpers for the "slippy map"
 *        tiling scheme used by OpenStreetMap, Google Maps, MapTiler, etc.
 *
 * The world is projected onto a square that is subdivided into 2^z * 2^z
 * tiles at zoom level @c z. A "tile coordinate" is therefore a real number
 * in [0, 2^z]; its integer part is the index of the tile that contains the
 * location and its fractional part is the position inside that tile.
 *
 * Multiplying a tile coordinate by the tile size in pixels (commonly 256)
 * yields a "global pixel coordinate": an absolute pixel position on the
 * full world image at that zoom level. All rendering in this project happens
 * in that global pixel space and is only translated to the output canvas at
 * the very end, which keeps the maths simple and numerically stable.
 *
 * Everything here is intentionally header-only, inline and free of Qt so it
 * can be unit-tested and reused without dragging in the GUI module.
 */
namespace mercator {

inline constexpr double kPi = 3.14159265358979323846;

/// Maximum latitude representable in Web-Mercator (where the projection -> infinity).
inline constexpr double kMaxLatitude = 85.05112877980659;

/// Number of tiles along one axis at the given zoom level (2^z).
inline double tilesPerAxis(int zoom)
{
    return static_cast<double>(1u << static_cast<unsigned>(zoom));
}

/// Clamp a latitude to the valid Web-Mercator band to avoid infinities.
inline double clampLat(double lat)
{
    if (lat < -kMaxLatitude)
        return -kMaxLatitude;
    if (lat > kMaxLatitude)
        return kMaxLatitude;
    return lat;
}

/// Convert longitude (degrees) to a fractional tile X coordinate at @p zoom.
inline double lonToTileX(double lon, int zoom)
{
    return (lon + 180.0) / 360.0 * tilesPerAxis(zoom);
}

/// Convert latitude (degrees) to a fractional tile Y coordinate at @p zoom.
inline double latToTileY(double lat, int zoom)
{
    const double latRad = clampLat(lat) * kPi / 180.0;
    // asinh(tan(phi)) == ln(tan(phi) + sec(phi)); the standard slippy-map formula.
    return (1.0 - std::asinh(std::tan(latRad)) / kPi) / 2.0 * tilesPerAxis(zoom);
}

/// Global pixel X coordinate (tile coordinate scaled by the tile size).
inline double lonToPixelX(double lon, int zoom, int tileSize)
{
    return lonToTileX(lon, zoom) * tileSize;
}

/// Global pixel Y coordinate (tile coordinate scaled by the tile size).
inline double latToPixelY(double lat, int zoom, int tileSize)
{
    return latToTileY(lat, zoom) * tileSize;
}

/**
 * @brief Translate an XYZ tile Y index to the TMS convention (or vice-versa).
 *
 * XYZ/OSM number tiles from the top (north) down; TMS numbers them from the
 * bottom (south) up. The two are related by @c y_tms = (2^z - 1) - y_xyz.
 * The transform is its own inverse, so the same function flips either way.
 */
inline int flipTileY(int y, int zoom)
{
    return static_cast<int>(tilesPerAxis(zoom)) - 1 - y;
}

} // namespace mercator

#endif // MERCATOR_H
