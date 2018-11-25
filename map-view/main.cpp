#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QVariantMap>
#include <QQmlContext>
#include <QtLocation/private/qgeojson_p.h>
#include <QGeoCircle>
#include <QGeoPath>
#include <QGeoPolygon>
#include <QtLocation/private/qdeclarativegeomapitemview_p.h>
#include <QtLocation/private/qdeclarativegeomapquickitem_p.h>
#include <QtLocation/private/qdeclarativecirclemapitem_p.h>
#include <QtLocation/private/qdeclarativepolylinemapitem_p.h>
#include <QtLocation/private/qdeclarativepolygonmapitem_p.h>
#include <QtLocation/private/qdeclarativerectanglemapitem_p.h>
#include <QJsonObject>
#include <QJsonArray>

/*
 *  prettyprint functions start. this funtion will become a method of QGeoJson Class
 *
 *  This function works on 2 levels: geometry is on an "upper level" and builds
 *  the "type: *** / data: ***" structure on an  object, then it calls the other export functions (Point, LineString,
 *  Polygon, MultiPoint, MultiLineString, MultiPolygon, GeometryCollection)
 *  feature takes a geometry and adds properties and id.
 *  featurecollection simply builds the structure and the array of features
 */

// !!!TODO - check wether the bbox member is correctly exported, verify how it works on RFC.

// bbox it's worng

static QJsonValue printPointCoordinates(const QGeoCoordinate &obtainedCoordinates)
{
    QJsonValue geoLat = obtainedCoordinates.latitude();
    QJsonValue geoLong = obtainedCoordinates.longitude();
    QJsonArray array = {geoLong, geoLat};
    QJsonValue geoAlt;

    if (!qIsNaN(obtainedCoordinates.altitude())) {
        geoAlt = obtainedCoordinates.altitude();
        array.append(geoAlt);
    }
    return QJsonValue(array);
}

static QJsonValue printLineStringCoordinates(const QList<QGeoCoordinate> &obtainedCoordinatesList)
{
    QJsonValue lineCoordinates;
    QJsonValue multiPosition;
    QJsonArray arrayPosition;

    for (const QGeoCoordinate &pointCoordinates: obtainedCoordinatesList) {
        multiPosition = printPointCoordinates(pointCoordinates);
        arrayPosition.append(multiPosition);
    }
    lineCoordinates = arrayPosition;
    return lineCoordinates;
}

static QJsonValue printPolygonCoordinates(const QList<QList<QGeoCoordinate>> &obtainedCoordinates)
{
    QJsonValue lineCoordinates;
    QJsonArray arrayPath;

    for (const QList<QGeoCoordinate> &parsedPath: obtainedCoordinates) {
        lineCoordinates = printLineStringCoordinates(parsedPath);
        arrayPath.append(lineCoordinates);
    }
    return QJsonValue(arrayPath);
}

static QJsonObject printPoint(const QVariantMap &pointMap)
{
    QJsonObject parsedPoint;
    QGeoCircle circle = pointMap.value(QStringLiteral("data")).value<QGeoCircle>();
    QGeoCoordinate center = circle.center();

    parsedPoint.insert(QStringLiteral("QGeoShape"), QJsonValue(QStringLiteral("QGeoCircle")));
    parsedPoint.insert(QStringLiteral("center coordinates"), printPointCoordinates(center));
    return parsedPoint;
}

static QJsonObject printLineString(const QVariantMap &lineStringMap)
{
    QJsonObject parsedLineString;
    QGeoPath parsedPath = lineStringMap.value(QStringLiteral("data")).value<QGeoPath>();
    QList <QGeoCoordinate> path = parsedPath.path();

    parsedLineString.insert(QStringLiteral("QGeoShape"), QJsonValue(QStringLiteral("QGeoPath")));
    parsedLineString.insert(QStringLiteral("Vertexes coordinates"), printLineStringCoordinates(path));
    return parsedLineString;
}

static QJsonObject printPolygon(const QVariantMap &polygonMap)
{
    QGeoPolygon parsedPoly = polygonMap.value(QStringLiteral("data")).value<QGeoPolygon>();
    QJsonObject parsedPolygon;
    QList<QList<QGeoCoordinate>> obtainedCoordinatesPoly;

    obtainedCoordinatesPoly << parsedPoly.path();
    if (parsedPoly.holesCount()!=0)
        for (int i = 0; i<parsedPoly.holesCount(); i++) {
            obtainedCoordinatesPoly << parsedPoly.holePath(i);
        }

    parsedPolygon.insert(QStringLiteral("QGeoShape"), QJsonValue(QStringLiteral("Polygon")));
    parsedPolygon.insert(QStringLiteral("Vertexes coordinates arrays"), printPolygonCoordinates(obtainedCoordinatesPoly));
    return parsedPolygon;
}

static QJsonObject printMultiPoint(const QVariantMap &multiPointMap)
{
    QVariantList multiCircleVariantList = multiPointMap.value(QStringLiteral("data")).value<QVariantList>();
    QJsonObject parsedMultiPoint;
    QJsonArray pointsArray;

    for (const QVariant &exCircleVariantMap: multiCircleVariantList) {
        pointsArray.append(QJsonValue(printPoint(exCircleVariantMap.value<QVariantMap>())));
    }

    parsedMultiPoint.insert(QStringLiteral("Multiple QGeoCircle array:"), QJsonValue(pointsArray));
    return parsedMultiPoint;
}

static QJsonObject printMultiLineString(const QVariantMap &multiLineStringMap)
{
    QVariantList multiPathVariantList = multiLineStringMap.value(QStringLiteral("data")).value<QVariantList>();
    QJsonObject parsedMultiLineString;
    QJsonArray lineStringArray;

    for (const QVariant &exPathVariantMap: multiPathVariantList) {
        lineStringArray.append(QJsonValue(printLineString(exPathVariantMap.value<QVariantMap>())));
    }

    parsedMultiLineString.insert(QStringLiteral("Multiple QGeoPath array:"), QJsonValue(lineStringArray));
    return parsedMultiLineString;
}

static QJsonObject printMultiPolygon(const QVariantMap &multiPolygonMap)
{
    QVariantList multiPathVariantList = multiPolygonMap.value(QStringLiteral("data")).value<QVariantList>();
    QJsonObject parsedMultiPolygon;
    QJsonArray polygonArray;

    for (const QVariant &exPathVariantMap: multiPathVariantList) {
        polygonArray.append(QJsonValue(printPolygon(exPathVariantMap.value<QVariantMap>())));
    }

    parsedMultiPolygon.insert(QStringLiteral("Multiple QGeoPolygon array:"), QJsonValue(polygonArray));
    return parsedMultiPolygon;
}

static QJsonObject printGeometry(const QVariantMap &geometryMap); // Function prototype

static QJsonObject printGeometryCollection(const QVariantMap &geometryCollection)
{
    QVariantList geometriesList = geometryCollection.value(QStringLiteral("data")).value<QVariantList>();
    QJsonObject returnedObject;
    QJsonArray parsedGeometries;

    for (const QVariant &extractedGeometry: geometriesList) {
        parsedGeometries.append(QJsonValue(printGeometry(extractedGeometry.value<QVariantMap>())));
    }

    returnedObject.insert(QStringLiteral("Multiple Geometry Object array:"), QJsonValue(parsedGeometries));
    return returnedObject;
}

/*
 * the following function is the CORE prettyprint function.
 * it builds the object in the forms we do want : 2 members, 1st: member = "type", value = string
 * with the type name inside, 2nd: member = "data", value = the custome objects returned by the six gemetries
 *
 * TODO: check wheter it is possible to make a single function with ALL the stuff (all the functions) inside (unlikely)
 * in any case push to GitHub and ask Paolo by 19:30
*/

static QJsonObject printGeometry(const QVariantMap &geometryMap)
{
    QJsonObject returnedObject;
    // Check if the map contains the "Point" value
    if (geometryMap.value(QStringLiteral("type")) == QStringLiteral("Point")) {
        returnedObject.insert(QStringLiteral("type"), QJsonValue(QStringLiteral("Point")));
        returnedObject.insert(QStringLiteral("data"), printPoint(geometryMap));
    }
    if (geometryMap.value(QStringLiteral("type")) == QStringLiteral("MultiPoint")){
            returnedObject.insert(QStringLiteral("type"), QJsonValue(QStringLiteral("MultiPoint")));
            returnedObject.insert(QStringLiteral("data"), printMultiPoint(geometryMap));
        }
    if (geometryMap.value(QStringLiteral("type")) == QStringLiteral("LineString")){
            returnedObject.insert(QStringLiteral("type"), QJsonValue(QStringLiteral("LineString")));
            returnedObject.insert(QStringLiteral("data"), printLineString(geometryMap));
        }
    if (geometryMap.value(QStringLiteral("type")) == QStringLiteral("MultiLineString")){
            returnedObject.insert(QStringLiteral("type"), QJsonValue(QStringLiteral("MultiLineString")));
            returnedObject.insert(QStringLiteral("data"), printMultiLineString(geometryMap));
        }
    if (geometryMap.value(QStringLiteral("type")) == QStringLiteral("Polygon")){
            returnedObject.insert(QStringLiteral("type"), QJsonValue(QStringLiteral("Polygon")));
            returnedObject.insert(QStringLiteral("data"), printPolygon(geometryMap));
        }
    if (geometryMap.value(QStringLiteral("type")) == QStringLiteral("MultiPolygon")){
            returnedObject.insert(QStringLiteral("type"), QJsonValue(QStringLiteral("MultiPolygon")));
            returnedObject.insert(QStringLiteral("data"), printMultiPolygon(geometryMap));
        }
    if (geometryMap.value(QStringLiteral("type")) == QStringLiteral("GeometryCollection")){
            returnedObject.insert(QStringLiteral("type"), QJsonValue(QStringLiteral("GeometryCollection")));
            returnedObject.insert(QStringLiteral("data"), printGeometryCollection(geometryMap));
        }
    return returnedObject;
}

static QJsonObject printFeature(const QVariantMap &feature)
{
    QJsonObject parsedFeature = printGeometry(feature);
    parsedFeature.insert(QStringLiteral("properties"),QJsonValue(feature.value(QStringLiteral("properties")).value<QVariant>().toJsonValue()));
    parsedFeature.insert(QStringLiteral("id"), QJsonValue(feature.value(QStringLiteral("id")).value<QVariant>().toJsonValue()));
    return parsedFeature;
}

static QJsonObject printFeatureCollection(const QVariantMap &featureCollectionMap)
{
    QVariantMap featureMap;
    QJsonObject singleFeature;
    QJsonArray featureArray;
    QJsonObject returnedObject;
    QVariantList extractedFeatureVariantList = featureCollectionMap.value(QStringLiteral("data")).value<QVariantList>();

    for (const QVariant &singleVariantFeature: extractedFeatureVariantList) {
        singleFeature = printFeature(singleVariantFeature.value<QVariantMap>());
        featureArray.append(QJsonValue(singleFeature));
    }
    returnedObject.insert(QStringLiteral("type"), QStringLiteral("FeatureCollection"));
    returnedObject.insert(QStringLiteral("data"), QJsonValue(featureArray));
    return returnedObject;
}

QByteArray prettyPrint(const QVariantList &customList)
{
            QVariantMap objectMap = customList.at(0).value<QVariantMap>();
            QJsonObject newObject;
            QJsonDocument newDocument;
            QByteArray printList;

            // Check the value correspomding to the key "Feature"
            if (objectMap.value(QStringLiteral("type")) == QStringLiteral("Feature"))
                newObject = printFeature(objectMap);
            else if (objectMap.value(QStringLiteral("type")) == QStringLiteral("FeatureCollection"))
                newObject = printFeatureCollection(objectMap);
            else newObject = printGeometry(objectMap);

            newDocument.setObject(newObject);
            printList = newDocument.toJson(QJsonDocument::JsonFormat());
            return printList;
}

// prettyprint end

class GeoJsoner: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant model MEMBER m_importedGeoJson NOTIFY modelChanged)

public:
    GeoJsoner(QObject *parent = nullptr) : QObject(parent)
    {

    }

public slots:

    Q_INVOKABLE bool load(QUrl url)
    {
        // Reading GeoJSON file
        QFile loadFile(url.toLocalFile());
        if (!loadFile.open(QIODevice::ReadOnly)) {
            qWarning() << "Error while opening the file: " << url;
            qWarning() << loadFile.error() <<  loadFile.errorString();
            return false;
        }

        // Load the GeoJSON file using Qt's API
        QJsonParseError err;
        QJsonDocument loadDoc(QJsonDocument::fromJson(loadFile.readAll(), &err));
        if (err.error) {
             qWarning() << "Error parsing while importing the JSON document:\n" << err.errorString();
             return false;
        }

        // Import geographic data to a QVariantList
        QVariantList modelList = QGeoJson::importGeoJson(loadDoc);
        dumpGeoJSON(modelList);
        prettyPrint(modelList);

        m_importedGeoJson =  modelList;
        emit modelChanged();
        return true;
    }

    // Used by the MapItemView Extractor to identify a Feature
    static bool hasProperties(QQuickItem *item)
    {
        QVariant props = item->property("props");
        return !props.isNull();
    }

    static bool isFeatureCollection(QQuickItem *item)
    {
        QVariant geoJsonType = item->property("geojsonType");
        return geoJsonType.toString() == QStringLiteral("FeatureCollection");
    }

    static bool isGeoJsonEntry(QQuickItem *item)
    {
        QVariant geoJsonType = item->property("geojsonType");
        return !geoJsonType.toString().isEmpty();
    }

    QVariantMap toVariant(QDeclarativePolygonMapItem *mapPolygon)
    {
        QVariantMap ls;
        ls["type"] = "Polygon";
        ls["data"] = QVariant::fromValue(mapPolygon->geoShape());
        if (hasProperties(mapPolygon))
            ls["properties"] = mapPolygon->property("props").toMap();
        return ls;
    }
    QVariantMap toVariant(QDeclarativePolylineMapItem *mapPolyline)
    {
        QVariantMap ls;
        ls["type"] = "LineString";
        ls["data"] = QVariant::fromValue(mapPolyline->geoShape());
        if (hasProperties(mapPolyline))
            ls["properties"] = mapPolyline->property("props").toMap();
        return ls;
    }
    QVariantMap toVariant(QDeclarativeCircleMapItem *mapCircle)
    {
        QVariantMap pt;
        pt["type"] = "Point";
        pt["data"] = QVariant::fromValue(mapCircle->geoShape());
        if (hasProperties(mapCircle))
            pt["properties"] = mapCircle->property("props").toMap();
        return pt;
    }

    QVariantMap toVariant(QDeclarativeGeoMapItemView *mapItemView)
    {
        bool featureCollecton = isFeatureCollection(mapItemView);
        // if not a feature collection, this must be a geometry collection,
        // or a multilinestring/multipoint/multipolygon.
        // To disambiguate, one could check for heterogeneity.
        // For simplicity, in this example, we expect the property "geojsonType" to be injected in the mapItemView
        // by the delegate, and to be correct.

        QString nodeType = mapItemView->property("geojsonType").toString();
        QVariantMap root;
        if (!nodeType.isEmpty())  // empty nodeType can happen only for the root MIV
            root["type"] = nodeType;
        if (hasProperties(mapItemView)) // Features are converted to regular types w properties.
            root["properties"] = mapItemView->property("props").toMap();

        QVariantList features;
        const QList<QQuickItem *> &quickChildren = mapItemView->childItems();
        for (auto kid : quickChildren) {
            QVariant entry;
            if (QDeclarativeGeoMapItemView *miv = qobject_cast<QDeclarativeGeoMapItemView *>(kid)) {
                // handle nested miv
                entry = toVariant(miv);
                if (nodeType.isEmpty()) // dirty hack to handle (=skip) the first MIV used to process the fictitious list with 1 element
                    return entry.toMap();
            } else if (QDeclarativePolylineMapItem *polyline = qobject_cast<QDeclarativePolylineMapItem *>(kid)) {
                entry = toVariant(polyline);
            } else if (QDeclarativePolygonMapItem *polygon = qobject_cast<QDeclarativePolygonMapItem *>(kid)) {
                entry = toVariant(polygon);
            } else if (QDeclarativeCircleMapItem *circle = qobject_cast<QDeclarativeCircleMapItem *>(kid)) {
                entry = toVariant(circle); // if GeoJSON Point type is visualized in other ways, handle those types here instead.
            }
            features.append(entry);
        }
        root["data"] = features;
        return root;
    }
    Q_INVOKABLE QVariantList toGeoJson(QDeclarativeGeoMapItemView *mapItemView)
    {
        QVariantList res;
        QDeclarativeGeoMapItemView *root = mapItemView;
        QVariantMap miv = toVariant(root);
        if (!miv.isEmpty())
            res.append(miv);
//        printQVariant(res);
        return res;
    }

    Q_INVOKABLE void dumpGeoJSON(QVariantList geoJson)
    {
        QJsonDocument json = QGeoJson::exportGeoJson(geoJson);
        QFile jsonFile("/tmp/dumpedGeoJson.json");
        jsonFile.open(QFile::WriteOnly);
        jsonFile.write(json.toJson());
        jsonFile.close();
    }

signals:
    void modelChanged();

public:
    QVariant m_importedGeoJson;
};

#include "main.moc"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    // Switch to QML app
    QQmlApplicationEngine engine;

    // engine.rootContext()->setContextProperty("GeoJsonr", new GeoJsoner(&engine));

    qmlRegisterType<GeoJsoner>("Qt.GeoJson", 1, 0, "GeoJsoner");
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
