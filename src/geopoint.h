#ifndef GEOPOINT_H
#define GEOPOINT_H

#include <QList>
#include <QString>

/**
 * @brief A single named geographic point.
 *
 * Coordinates are expressed in WGS84 degrees, the same convention used by
 * GPS, GeoJSON and OpenStreetMap:
 *   - @c lon (longitude) in the range [-180, 180], positive east;
 *   - @c lat (latitude)  in the range [-90, 90],  positive north.
 *
 * Note that the Web-Mercator projection used for tiled maps can only
 * represent latitudes in approximately [-85.0511, 85.0511]; values outside
 * that band are clamped during rendering (see mercator::clampLat).
 */
struct GeoPoint
{
    double lon = 0.0;   ///< Longitude in degrees (positive east).
    double lat = 0.0;   ///< Latitude in degrees (positive north).
    QString name;       ///< Human readable label (may be empty).

    GeoPoint() = default;
    GeoPoint(double longitude, double latitude, QString label = QString())
        : lon(longitude), lat(latitude), name(std::move(label))
    {
    }
};

/// An ordered sequence of points; consecutive points are joined by the route line.
using GeoPath = QList<GeoPoint>;

#endif // GEOPOINT_H
