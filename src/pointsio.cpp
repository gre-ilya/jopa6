#include "pointsio.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QTextStream>

namespace pointsio {

namespace {

Format detectFormat(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("geojson"))
        return Format::GeoJson;
    if (suffix == QLatin1String("json"))
        return Format::Json;
    return Format::Csv; // csv, txt and anything else
}

/// First present value among @p keys, or an undefined value.
QJsonValue pick(const QJsonObject &obj, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const auto it = obj.constFind(QLatin1String(key));
        if (it != obj.constEnd())
            return it.value();
    }
    return QJsonValue(QJsonValue::Undefined);
}

bool toNumber(const QJsonValue &v, double *out)
{
    if (v.isDouble()) { *out = v.toDouble(); return true; }
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().trimmed().toDouble(&ok);
        if (ok) { *out = d; return true; }
    }
    return false;
}

GeoPath parseCsv(const QString &text, CsvOrder order, QString *error)
{
    GeoPath path;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                         Qt::SkipEmptyParts);
    int lineNo = 0;
    for (const QString &raw : lines) {
        ++lineNo;
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        // Split into at most 3 fields so the name may itself contain commas.
        const int firstComma = line.indexOf(QLatin1Char(','));
        if (firstComma < 0)
            continue;
        const int secondComma = line.indexOf(QLatin1Char(','), firstComma + 1);

        const QString aStr = line.left(firstComma).trimmed();
        QString bStr, name;
        if (secondComma < 0) {
            bStr = line.mid(firstComma + 1).trimmed();
        } else {
            bStr = line.mid(firstComma + 1, secondComma - firstComma - 1).trimmed();
            name = line.mid(secondComma + 1).trimmed();
        }

        bool okA = false, okB = false;
        const double a = aStr.toDouble(&okA);
        const double b = bStr.toDouble(&okB);
        if (!okA || !okB) {
            // Tolerate a header row (e.g. "lon,lat,name") appearing before any
            // data; once real points have been read, a bad line is an error.
            if (path.isEmpty())
                continue;
            if (error)
                *error = QStringLiteral("CSV parse error on line %1: '%2'").arg(lineNo).arg(raw);
            return GeoPath();
        }

        // Strip surrounding quotes from names exported by spreadsheets.
        if (name.size() >= 2 && name.startsWith(QLatin1Char('"')) && name.endsWith(QLatin1Char('"')))
            name = name.mid(1, name.size() - 2);

        GeoPoint p;
        if (order == CsvOrder::LonLat) { p.lon = a; p.lat = b; }
        else { p.lat = a; p.lon = b; }
        p.name = name;
        path.append(p);
    }
    return path;
}

bool readPointObject(const QJsonObject &obj, GeoPoint *out)
{
    const QJsonValue lonV = pick(obj, {"lon", "lng", "longitude", "x"});
    const QJsonValue latV = pick(obj, {"lat", "latitude", "y"});
    double lon = 0.0, lat = 0.0;
    if (!toNumber(lonV, &lon) || !toNumber(latV, &lat))
        return false;
    out->lon = lon;
    out->lat = lat;
    out->name = pick(obj, {"name", "title", "label", "id"}).toString();
    return true;
}

GeoPath parseJsonArray(const QJsonArray &array, QString *error)
{
    GeoPath path;
    for (const QJsonValue &v : array) {
        if (v.isObject()) {
            GeoPoint p;
            if (readPointObject(v.toObject(), &p))
                path.append(p);
        } else if (v.isArray()) {
            // Also accept [lat, lon] or [lat, lon, name] tuples (lat first, to
            // match the project convention). Note this differs from GeoJSON,
            // whose coordinate arrays are always [lon, lat] per the standard.
            const QJsonArray tup = v.toArray();
            if (tup.size() >= 2) {
                double lat = 0.0, lon = 0.0;
                if (toNumber(tup.at(0), &lat) && toNumber(tup.at(1), &lon)) {
                    GeoPoint p;
                    p.lat = lat; p.lon = lon;
                    if (tup.size() >= 3)
                        p.name = tup.at(2).toString();
                    path.append(p);
                }
            }
        }
    }
    if (path.isEmpty() && error)
        *error = QStringLiteral("No usable points found in JSON array.");
    return path;
}

GeoPath parseGeoJson(const QJsonObject &root, QString *error)
{
    GeoPath path;
    const QJsonArray features = root.value(QStringLiteral("features")).toArray();
    for (const QJsonValue &fv : features) {
        const QJsonObject feature = fv.toObject();
        const QJsonObject geom = feature.value(QStringLiteral("geometry")).toObject();
        if (geom.value(QStringLiteral("type")).toString() != QLatin1String("Point"))
            continue;
        const QJsonArray coords = geom.value(QStringLiteral("coordinates")).toArray();
        if (coords.size() < 2)
            continue;
        double lon = 0.0, lat = 0.0;
        if (!toNumber(coords.at(0), &lon) || !toNumber(coords.at(1), &lat))
            continue;
        GeoPoint p;
        p.lon = lon; p.lat = lat; // GeoJSON is always [longitude, latitude]
        const QJsonObject props = feature.value(QStringLiteral("properties")).toObject();
        p.name = pick(props, {"name", "title", "label", "id"}).toString();
        path.append(p);
    }
    if (path.isEmpty() && error)
        *error = QStringLiteral("No Point features found in GeoJSON.");
    return path;
}

} // namespace

GeoPath load(const QString &path, QString *error, Format forcedFormat, CsvOrder csvOrder)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Cannot open '%1': %2").arg(path, file.errorString());
        return GeoPath();
    }
    const QByteArray data = file.readAll();
    file.close();

    const Format fmt = (forcedFormat == Format::Auto) ? detectFormat(path) : forcedFormat;

    if (fmt == Format::Csv)
        return parseCsv(QString::fromUtf8(data), csvOrder, error);

    QJsonParseError jsonErr{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &jsonErr);
    if (jsonErr.error != QJsonParseError::NoError) {
        if (error)
            *error = QStringLiteral("JSON parse error in '%1': %2")
                         .arg(path, jsonErr.errorString());
        return GeoPath();
    }

    if (fmt == Format::GeoJson)
        return parseGeoJson(doc.object(), error);

    // Plain JSON: accept either a bare array or an object wrapping a "points"
    // / "features" array (the latter falls through to the GeoJSON parser).
    if (doc.isArray())
        return parseJsonArray(doc.array(), error);
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.contains(QStringLiteral("points")))
            return parseJsonArray(obj.value(QStringLiteral("points")).toArray(), error);
        if (obj.contains(QStringLiteral("features")))
            return parseGeoJson(obj, error);
    }
    if (error)
        *error = QStringLiteral("Unrecognised JSON structure in '%1'.").arg(path);
    return GeoPath();
}

} // namespace pointsio
