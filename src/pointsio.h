#ifndef POINTSIO_H
#define POINTSIO_H

#include <QString>

#include "geopoint.h"

/**
 * @brief Loading of point lists for the command-line test harness.
 *
 * The renderer itself takes a GeoPath directly; these helpers exist only so
 * the CLI can read points from a file. Three formats are recognised, chosen
 * by the file extension (override with the @c forcedFormat argument):
 *
 *   - CSV      (.csv, .txt): one point per line, "a,b,name". Lines starting
 *              with '#' and blank lines are ignored. A header line naming the
 *              columns (e.g. "lon,lat,name") is detected and skipped.
 *   - JSON     (.json):      an array of objects, e.g.
 *              [{"lon": 37.6, "lat": 55.7, "name": "A"}, ...].
 *              Common key aliases (lng/longitude, latitude) are accepted.
 *   - GeoJSON  (.geojson):   a FeatureCollection of Point features; the label
 *              is taken from properties.name / .title / .label.
 */
namespace pointsio {

enum class Format { Auto, Csv, Json, GeoJson };

/// Column order for CSV input (JSON/GeoJSON are unaffected).
enum class CsvOrder { LonLat, LatLon };

/**
 * @brief Load points from @p path.
 * @param path         File to read.
 * @param error        Receives an error message on failure.
 * @param forcedFormat Force a specific parser instead of guessing by extension.
 * @param csvOrder     Coordinate order for CSV files.
 * @return The parsed points, or an empty path on error (see @p error).
 */
GeoPath load(const QString &path, QString *error,
             Format forcedFormat = Format::Auto,
             CsvOrder csvOrder = CsvOrder::LonLat);

} // namespace pointsio

#endif // POINTSIO_H
