/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "inpututils.h"

#include <QWindow>
#include <QScreen>
#include <QApplication>

#include "qcoreapplication.h"
#include "qgsgeometrycollection.h"
#include "qgslinestring.h"
#include "qgspolygon.h"
#include "qgsvectorlayer.h"
#include "qgsquickutils.h"
#include "qgsquickmaptransform.h"
#include "coreutils.h"
#include "qgis.h"
#include "qgscoordinatereferencesystem.h"
#include "qgscoordinatetransform.h"
#include "qgsdistancearea.h"
#include "qgslogger.h"
#include "qgsvectorlayer.h"
#include "qgsfeature.h"
#include "qgsapplication.h"
#include "qgsvaluerelationfieldformatter.h"
#include "qgsdatetimefieldformatter.h"
#include "qgslayertree.h"

#include "featurelayerpair.h"
#include "qgsquickmapsettings.h"
#include "qgsquickutils.h"
#include "qgsunittypes.h"
#include "qgsfeatureid.h"

#include <Qt>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <limits>
#include <math.h>

static const QString DATE_TIME_FORMAT = QStringLiteral( "yyMMdd-hhmmss" );
static const QString INVALID_DATETIME_STR = QStringLiteral( "Invalid datetime" );

InputUtils::InputUtils( QObject *parent ): QObject( parent )
{
  mAndroidUtils = std::unique_ptr<AndroidUtils>( new AndroidUtils() );
}

bool InputUtils::removeFile( const QString &filePath )
{
  QFile file( filePath );
  return file.remove( filePath );
}

bool InputUtils::copyFile( const QString &srcPath, const QString &dstPath )
{
  QString modSrcPath = srcPath;
  if ( srcPath.startsWith( "file://" ) )
  {
    modSrcPath = modSrcPath.replace( "file://", "" );
  }

  // https://github.com/lutraconsulting/input/issues/418
  // does not work for iOS files with format
  // file:assets-library://asset/asset.PNG%3Fid=A53AB989-6354-433A-9CB9-958179B7C14D&ext=PNG

  return QFile::copy( modSrcPath, dstPath );
}

QString InputUtils::getFileName( const QString &filePath )
{
  QFileInfo fileInfo( sanitizeName( filePath ) );
  return fileInfo.fileName();
}

QString InputUtils::sanitizeName( const QString &path )
{
#ifdef Q_OS_IOS
  QRegularExpression reAbs( "(.+)\\/asset\\.PNG%.Fid=(\\S+)&ext=" );
  QRegularExpressionMatch matchAbs = reAbs.match( path );
  if ( matchAbs.hasMatch() )
  {
    QString base = matchAbs.captured( 1 );
    QString name = matchAbs.captured( 2 );
    return base + "/" + name + ".png";
  }

  QRegularExpression reRel( "asset\\.PNG%.Fid=(\\S+)&ext=" );
  QRegularExpressionMatch matchRel = reRel.match( path );
  if ( matchRel.hasMatch() )
  {
    QString matched = matchRel.captured( 1 );
    return matched + ".png";
  }
#endif
  return path;
}

QString InputUtils::formatProjectName( const QString &fullProjectName )
{
  if ( fullProjectName.contains( "/" ) )
  {
    QStringList list = fullProjectName.split( "/" );
    if ( list.at( 0 ).isEmpty() )
    {
      return QString( "<b>%1</b>" ).arg( list.at( 1 ) );
    }
    else
    {
      return QString( "%1/<b>%2</b>" ).arg( list.at( 0 ) ).arg( list.at( 1 ) );
    }
  }
  else
  {
    return QString( "<b>%1</b>" ).arg( fullProjectName );
  }
}

QString InputUtils::formatNumber( const double number, int precision )
{
  return QString::number( number, 'f', precision );
}

QString InputUtils::formatDateTimeDiff( const QDateTime &tMin, const QDateTime &tMax )
{
  qint64 daysDiff = tMin.daysTo( tMax );

  // datetime is invalid
  if ( daysDiff < 0 )
  {
    return INVALID_DATETIME_STR;
  }

  // diff is maximum one day
  // Note that difference from 23:55 to 0:05 the next day counts as one day
  if ( daysDiff == 0 || daysDiff == 1 )
  {
    qint64 secsDiff = tMin.secsTo( tMax );
    if ( secsDiff < 0 )
    {
      return INVALID_DATETIME_STR;
    }
    else if ( secsDiff < 60 )
    {
      return tr( "just now" );
    }
    else if ( secsDiff < 60 * 60 )
    {
      int period = secsDiff / 60 ;
      return ( period > 1 ) ? tr( "%1 minutes ago" ).arg( period ) : tr( "%1 minute ago" ).arg( period );
    }
    else if ( secsDiff < 60 * 60 * 24 )
    {
      int period = secsDiff / ( 60 * 60 );
      return ( period > 1 ) ? tr( "%1 hours ago" ).arg( period ) : tr( "%1 hour ago" ).arg( period );
    }
    else
    {
      return ( daysDiff > 1 ) ? tr( "%1 days ago" ).arg( daysDiff ) : tr( "%1 day ago" ).arg( daysDiff );
    }
  }
  else if ( daysDiff < 7 )
  {
    return ( daysDiff > 1 ) ? tr( "%1 days ago" ).arg( daysDiff ) : tr( "%1 day ago" ).arg( daysDiff );
  }
  else if ( daysDiff < 31 )
  {
    int period = daysDiff / 7;
    return ( period > 1 ) ? tr( "%1 weeks ago" ).arg( period ) : tr( "%1 week ago" ).arg( period );
  }
  else if ( daysDiff < 365 )
  {
    int period = daysDiff / 31;
    return ( period > 1 ) ? tr( "%1 months ago" ).arg( period ) : tr( "%1 month ago" ).arg( period );
  }
  else
  {
    int period = daysDiff / 365;
    return ( period > 1 ) ? tr( "%1 years ago" ).arg( period ) : tr( "%1 year ago" ).arg( period );
  }

  return INVALID_DATETIME_STR;
}

void InputUtils::setExtentToFeature( const FeatureLayerPair &pair, QgsQuickMapSettings *mapSettings, double panelOffsetRatio )
{

  if ( !mapSettings )
    return;

  if ( !pair.layer() )
    return;

  if ( !pair.feature().isValid() )
    return;

  QgsGeometry geom = pair.feature().geometry();
  if ( geom.isNull() || !geom.constGet() )
    return;

  QgsRectangle bbox = mapSettings->mapSettings().layerExtentToOutputExtent( pair.layer(), geom.boundingBox() );
  QgsRectangle currentExtent = mapSettings->mapSettings().extent();
  QgsPointXY currentExtentCenter = currentExtent.center();
  QgsPointXY featureCenter = bbox.center();

  double panelOffset = ( currentExtent.yMaximum() - currentExtent.yMinimum() ) * panelOffsetRatio / 2;
  double offsetX = currentExtentCenter.x() - featureCenter.x();
  double offsetY = currentExtentCenter.y() - featureCenter.y();
  currentExtent.setXMinimum( currentExtent.xMinimum() - offsetX );
  currentExtent.setXMaximum( currentExtent.xMaximum() - offsetX );
  currentExtent.setYMinimum( currentExtent.yMinimum() - offsetY - panelOffset );
  currentExtent.setYMaximum( currentExtent.yMaximum() - offsetY - panelOffset );
  mapSettings->setExtent( currentExtent );
}

double InputUtils::convertCoordinateString( const QString &rationalValue )
{
  QStringList values = rationalValue.split( "," );
  if ( values.size() != 3 ) return 0;

  double degrees = ratherZeroThanNaN( convertRationalNumber( values.at( 0 ) ) );
  double minutes = ratherZeroThanNaN( convertRationalNumber( values.at( 1 ) ) );
  double seconds = ratherZeroThanNaN( convertRationalNumber( values.at( 2 ) ) );

  double result = degrees + minutes / 60 + seconds / 3600;
  return result;
}

double InputUtils::convertRationalNumber( const QString &rationalValue )
{
  if ( rationalValue.isEmpty() )
    return std::numeric_limits<double>::quiet_NaN();

  QStringList number = rationalValue.split( "/" );
  if ( number.size() != 2 )
    return std::numeric_limits<double>::quiet_NaN();

  double numerator = number.at( 0 ).toDouble();
  double denominator = number.at( 1 ).toDouble();
  if ( denominator == 0 )
    return denominator;

  return numerator / denominator;
}

double InputUtils::mapSettingsScale( QgsQuickMapSettings *ms )
{
  if ( !ms ) return 1;
  return 1 / ms->mapUnitsPerPixel();
}

double InputUtils::mapSettingsOffsetX( QgsQuickMapSettings *ms )
{
  if ( !ms ) return 0;
  return -ms->visibleExtent().xMinimum();
}

double InputUtils::mapSettingsOffsetY( QgsQuickMapSettings *ms )
{
  if ( !ms ) return 0;
  return -ms->visibleExtent().yMaximum();
}


static void addLineString( const QgsLineString *line, QVector<double> &data )
{
  data << line->numPoints();
  const double *x = line->xData();
  const double *y = line->yData();
  for ( int i = 0; i < line->numPoints(); ++i )
  {
    data << x[i] << y[i];
  }
}

static void addSingleGeometry( const QgsAbstractGeometry *geom, QgsWkbTypes::GeometryType type, QVector<double> &data )
{
  switch ( type )
  {
    case QgsWkbTypes::PointGeometry:
    {
      const QgsPoint *point = qgsgeometry_cast<const QgsPoint *>( geom );
      if ( point )
      {
        data << 0 << point->x() << point->y();
      }
      break;
    }

    case QgsWkbTypes::LineGeometry:
    {
      const QgsLineString *line = qgsgeometry_cast<const QgsLineString *>( geom );
      if ( line )
      {
        data << 1;
        addLineString( line, data );
      }
      break;
    }

    case QgsWkbTypes::PolygonGeometry:
    {
      const QgsPolygon *poly = qgsgeometry_cast<const QgsPolygon *>( geom );
      if ( poly )
      {
        if ( const QgsLineString *line = qgsgeometry_cast<const QgsLineString *>( poly->exteriorRing() ) )
        {
          data << 2;
          addLineString( line, data );
        }
        for ( int i = 0; i < poly->numInteriorRings(); ++i )
        {
          if ( const QgsLineString *line = qgsgeometry_cast<const QgsLineString *>( poly->interiorRing( i ) ) )
          {
            data << 2;
            addLineString( line, data );
          }
        }
      }
      break;
    }

    case QgsWkbTypes::UnknownGeometry:
    case QgsWkbTypes::NullGeometry:
      break;
  }
}


QVector<double> InputUtils::extractGeometryCoordinates( const FeatureLayerPair &pair, QgsQuickMapSettings *mapSettings )
{
  if ( !mapSettings || !pair.isValid() )
    return QVector<double>();

  QgsGeometry g = pair.feature().geometry();

  QgsCoordinateTransform ct( pair.layer()->crs(), mapSettings->destinationCrs(), mapSettings->transformContext() );
  if ( !ct.isShortCircuited() )
  {
    try
    {
      g.transform( ct );
    }
    catch ( QgsCsException &e )
    {
      Q_UNUSED( e )
      return QVector<double>();
    }
  }

  QVector<double> data;

  const QgsAbstractGeometry *geom = g.constGet();
  QgsWkbTypes::GeometryType geomType = g.type();
  const QgsGeometryCollection *collection = qgsgeometry_cast<const QgsGeometryCollection *>( geom );
  if ( collection && !collection->isEmpty() )
  {
    for ( int i = 0; i < collection->numGeometries(); ++i )
    {
      addSingleGeometry( collection->geometryN( i ), geomType, data );
    }
  }
  else
  {
    addSingleGeometry( geom, geomType, data );
  }

  return data;
}

QString InputUtils::filesToString( QList<MerginFile> files )
{
  QStringList resultList;
  for ( MerginFile file : files )
  {
    resultList << file.path;
  }
  return resultList.join( ", " );
}

QString InputUtils::bytesToHumanSize( double bytes )
{
  const int precision = 1;
  if ( bytes < 1e-5 )
  {
    return "0.0";
  }
  else if ( bytes < 1024.0 * 1024.0 )
  {
    return QString::number( bytes / 1024.0, 'f', precision ) + " KB";
  }
  else if ( bytes < 1024.0 * 1024.0 * 1024.0 )
  {
    return QString::number( bytes / 1024.0 / 1024.0, 'f', precision ) + " MB";
  }
  else if ( bytes < 1024.0 * 1024.0 * 1024.0 * 1024.0 )
  {
    return QString::number( bytes / 1024.0 / 1024.0 / 1024.0, 'f', precision ) + " GB";
  }
  else
  {
    return QString::number( bytes / 1024.0 / 1024.0 / 1024.0 / 1024.0, 'f', precision ) + " TB";
  }
}

bool InputUtils::hasStoragePermission()
{
  if ( appPlatform() == QStringLiteral( "android" ) )
  {
    return mAndroidUtils->checkPermission( "android.permission.WRITE_EXTERNAL_STORAGE" );
  }
  return true;
}

bool InputUtils::acquireStoragePermission()
{
  if ( appPlatform() == QStringLiteral( "android" ) )
  {
    return mAndroidUtils->requestStoragePermission();
  }
  return true;
}

bool InputUtils::acquireCameraPermission()
{
  if ( appPlatform() == QStringLiteral( "android" ) )
  {
    return mAndroidUtils->requestCameraPermission();
  }
  return true;
}

void InputUtils::quitApp()
{
  QCoreApplication::quit();
}

QString InputUtils::appPlatform()
{
#if defined( ANDROID )
  const QString platform = "android";
#elif defined( Q_OS_IOS )
  const QString platform = "ios";
#elif defined( Q_OS_WIN32 )
  const QString platform = "win";
#elif defined( Q_OS_LINUX )
  const QString platform = "linux";
#elif defined( Q_OS_MAC )
  const QString platform = "macos";
#else
  const QString platform = "unknown";
#endif
  return platform;
}

void InputUtils::onQgsLogMessageReceived( const QString &message, const QString &tag, Qgis::MessageLevel level )
{
  QString levelStr;
  switch ( level )
  {
    case Qgis::MessageLevel::Warning:
      levelStr = "Warning";
      break;
    case Qgis::MessageLevel::Critical:
      levelStr = "Error";
      break;
    default:
      break;
  }

  CoreUtils::log( "QGIS " + tag, levelStr + ": " + message );
}

bool InputUtils::cpDir( const QString &srcPath, const QString &dstPath, bool onlyDiffable )
{
  bool result  = true;
  QDir parentDstDir( QFileInfo( dstPath ).path() );
  if ( !parentDstDir.mkpath( dstPath ) )
  {
    CoreUtils::log( "cpDir", QString( "Cannot make path %1" ).arg( dstPath ) );
    return false;
  }

  QDir srcDir( srcPath );
  const QFileInfoList fileInfoList = srcDir.entryInfoList( QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden );
  foreach ( const QFileInfo &info, fileInfoList )
  {
    QString srcItemPath = srcPath + "/" + info.fileName();
    QString dstItemPath = dstPath + "/" + info.fileName();
    if ( info.isDir() )
    {
      if ( !cpDir( srcItemPath, dstItemPath ) )
      {
        CoreUtils::log( "cpDir", QString( "Cannot copy a dir from %1 to %2" ).arg( srcItemPath ).arg( dstItemPath ) );
        result = false;
      }
    }
    else if ( info.isFile() )
    {
      if ( onlyDiffable && !MerginApi::isFileDiffable( info.fileName() ) )
        continue;

      if ( !QFile::copy( srcItemPath, dstItemPath ) )
      {
        if ( !QFile::remove( dstItemPath ) )
        {
          CoreUtils::log( "cpDir", QString( "Cannot remove a file from %1" ).arg( dstItemPath ) );
          result =  false;
        }
        if ( !QFile::copy( srcItemPath, dstItemPath ) )
        {
          CoreUtils::log( "cpDir", QString( "Cannot overwrite a file %1 with %2" ).arg( dstItemPath ).arg( dstItemPath ) );
          result =  false;
        }
      }
      QFile::setPermissions( dstItemPath, QFile::ReadUser | QFile::WriteUser | QFile::ReadOwner | QFile::WriteOwner );
    }
    else
    {
      CoreUtils::log( "cpDir", QString( "Unhandled item %1 in cpDir" ).arg( info.filePath() ) );
    }
  }
  return result;
}

QString InputUtils::renameWithDateTime( const QString &srcPath, const QDateTime &dateTime )
{
  if ( QFile::exists( srcPath ) )
  {
    QFileInfo info( srcPath );
    QString timestamp = ( dateTime.isValid() ) ? dateTime.toString( DATE_TIME_FORMAT ) : QDateTime::currentDateTime().toString( DATE_TIME_FORMAT );
    QString newFilename = QString( "%1.%2" ).arg( timestamp ).arg( info.suffix() );
    QString newPath( info.absolutePath() + "/" + newFilename );

    if ( QFile::rename( srcPath, newPath ) ) return newPath;
  }

  return QString();
}

void InputUtils::showNotification( const QString &message )
{
  emit showNotificationRequested( message );
}

qreal InputUtils::groundSpeedFromSource( PositionKit *positionKit )
{
  if ( positionKit == nullptr ) return 0;

  if ( positionKit->source()->lastKnownPosition().isValid() )
  {
    return positionKit->source()->lastKnownPosition().attribute( QGeoPositionInfo::Attribute::GroundSpeed );
  }
  return 0;
}

double InputUtils::ratherZeroThanNaN( double d )
{
  return ( isnan( d ) ) ? 0.0 : d;
}

/**
 * Makes QgsCoordinateReferenceSystem::fromEpsgId accessible for QML components
 */
QgsCoordinateReferenceSystem InputUtils::coordinateReferenceSystemFromEpsgId( long epsg )
{
  return QgsCoordinateReferenceSystem::fromEpsgId( epsg );
}

QgsPointXY InputUtils::pointXY( double x, double y )
{
  return QgsPointXY( x, y );
}

QgsPoint InputUtils::point( double x, double y, double z, double m )
{
  return QgsPoint( x, y, z, m );
}

QgsPoint InputUtils::coordinateToPoint( const QGeoCoordinate &coor )
{
  return QgsPoint( coor.longitude(), coor.latitude(), coor.altitude() );
}

QgsPointXY InputUtils::transformPoint( const QgsCoordinateReferenceSystem &srcCrs,
                                       const QgsCoordinateReferenceSystem &destCrs,
                                       const QgsCoordinateTransformContext &context,
                                       const QgsPointXY &srcPoint )
{
  try
  {
    QgsCoordinateTransform ct( srcCrs, destCrs, context );
    if ( ct.isValid() )
    {
      const QgsPointXY pt = ct.transform( srcPoint );
      return pt;
    }
  }
  catch ( QgsCsException &cse )
  {
    Q_UNUSED( cse )
  }
  return srcPoint;
}

double InputUtils::screenUnitsToMeters( QgsQuickMapSettings *mapSettings, int baseLengthPixels )
{
  if ( mapSettings == nullptr ) return 0.0;

  QgsDistanceArea mDistanceArea;
  mDistanceArea.setEllipsoid( QStringLiteral( "WGS84" ) );
  mDistanceArea.setSourceCrs( mapSettings->destinationCrs(), mapSettings->transformContext() );

  // calculate the geographic distance from the central point of extent
  // to the specified number of points on the right side
  QSize s = mapSettings->outputSize();
  QPoint pointCenter( s.width() / 2, s.height() / 2 );
  QgsPointXY p1 = mapSettings->screenToCoordinate( pointCenter );
  QgsPointXY p2 = mapSettings->screenToCoordinate( pointCenter + QPoint( baseLengthPixels, 0 ) );
  return mDistanceArea.measureLine( p1, p2 );
}

bool InputUtils::fileExists( const QString &path )
{
  QFileInfo check_file( path );
  // check if file exists and if yes: Is it really a file and no directory?
  return ( check_file.exists() && check_file.isFile() );
}

QString InputUtils::resolveTargetDir( const QString &homePath, const QVariantMap &config, const FeatureLayerPair &pair, QgsProject *activeProject )
{
  QString expression;
  QMap<QString, QVariant> collection = config.value( QStringLiteral( "PropertyCollection" ) ).toMap();
  QMap<QString, QVariant> props = collection.value( QStringLiteral( "properties" ) ).toMap();

  if ( !props.isEmpty() )
  {
    QMap<QString, QVariant> propertyRootPath = props.value( QStringLiteral( "propertyRootPath" ) ).toMap();
    expression = propertyRootPath.value( QStringLiteral( "expression" ), QString() ).toString();
  }

  if ( !expression.isEmpty() )
  {
    return evaluateExpression( pair, activeProject, expression );
  }
  else
  {
    QString defaultRoot = config.value( QStringLiteral( "DefaultRoot" ) ).toString();
    if ( defaultRoot.isEmpty() )
    {
      return homePath;
    }
    else
    {
      return defaultRoot;
    }
  }


}

QString InputUtils::resolvePrefixForRelativePath( int relativeStorageMode, const QString &homePath, const QString &targetDir )
{
  if ( relativeStorageMode == 1 )
  {
    return homePath;
  }
  else if ( relativeStorageMode == 2 )
  {
    return targetDir;
  }
  else
  {
    return QString();
  }
}

QString InputUtils::getAbsolutePath( const QString &path, const QString &prefixPath )
{
  return ( prefixPath.isEmpty() ) ? path : QStringLiteral( "%1/%2" ).arg( prefixPath ).arg( path );
}

QString InputUtils::resolvePath( const QString &path, const QString &homePath, const QVariantMap &config, const FeatureLayerPair &pair, QgsProject *activeProject )
{
  int relativeStorageMode = config.value( QStringLiteral( "RelativeStorage" ) ).toInt();
  QString targetDir = resolveTargetDir( homePath, config, pair, activeProject );
  QString prefixToRelativePath = resolvePrefixForRelativePath( relativeStorageMode, homePath, targetDir );

  return getAbsolutePath( path, prefixToRelativePath );
}

QString InputUtils::getRelativePath( const QString &path, const QString &prefixPath )
{
  QString modPath = path;
  QString filePrefix( "file://" );

  if ( path.startsWith( filePrefix ) )
  {
    modPath = modPath.replace( filePrefix, QString() );
  }

  if ( prefixPath.isEmpty() ) return modPath;

  // Do not use a canonical path for non-existing path
  if ( !QFileInfo( path ).exists() )
  {
    if ( !prefixPath.isEmpty() && modPath.startsWith( prefixPath ) )
    {
      return modPath.replace( prefixPath, QString() );
    }
  }
  else
  {
    QDir absoluteDir( modPath );
    QDir prefixDir( prefixPath );
    QString canonicalPath = absoluteDir.canonicalPath();
    QString prefixCanonicalPath = prefixDir.canonicalPath() + "/";

    if ( prefixCanonicalPath.length() > 1 && canonicalPath.startsWith( prefixCanonicalPath ) )
    {
      return canonicalPath.replace( prefixCanonicalPath, QString() );
    }
  }

  return QString();
}

void InputUtils::logMessage( const QString &message, const QString &tag, Qgis::MessageLevel level )
{
  QgsMessageLog::logMessage( message, tag, level );
}

FeatureLayerPair InputUtils::featureFactory( const QgsFeature &feature, QgsVectorLayer *layer )
{
  return FeatureLayerPair( feature, layer );
}

const QUrl InputUtils::getThemeIcon( const QString &name )
{
  QString path = QStringLiteral( "qrc:/%1.svg" ).arg( name );
  QgsDebugMsg( QStringLiteral( "Using icon %1 from %2" ).arg( name, path ) );
  return QUrl( path );
}

const QUrl InputUtils::getEditorComponentSource( const QString &widgetName, const QVariantMap &config, const QgsField &field )
{
  QString path( "../editor/input%1.qml" );

  if ( widgetName == QStringLiteral( "range" ) )
  {
    if ( config.contains( "Style" ) )
    {
      if ( config["Style"] == QStringLiteral( "Slider" ) )
      {
        return QUrl( path.arg( QLatin1String( "rangeslider" ) ) );
      }
      else if ( config["Style"] == QStringLiteral( "SpinBox" ) )
      {
        return QUrl( path.arg( QLatin1String( "rangeeditable" ) ) );
      }
    }
    return QUrl( path.arg( QLatin1String( "textedit" ) ) );
  }

  if ( field.name().contains( "qrcode", Qt::CaseInsensitive ) || field.alias().contains( "qrcode", Qt::CaseInsensitive ) )
  {
    return QUrl( path.arg( QStringLiteral( "qrcodereader" ) ) );
  }

  if ( widgetName == QStringLiteral( "textedit" ) )
  {
    if ( config.value( "IsMultiline" ).toBool() )
    {
      return QUrl( path.arg( QStringLiteral( "texteditmultiline" ) ) );
    }
    return QUrl( path.arg( QLatin1String( "textedit" ) ) );
  }

  QStringList supportedWidgets = { QStringLiteral( "textedit" ),
                                   QStringLiteral( "valuemap" ),
                                   QStringLiteral( "valuerelation" ),
                                   QStringLiteral( "checkbox" ),
                                   QStringLiteral( "externalresource" ),
                                   QStringLiteral( "datetime" ),
                                   QStringLiteral( "range" ),
                                   QStringLiteral( "relation" ),
                                   QStringLiteral( "relationreference" )
                                 };
  if ( supportedWidgets.contains( widgetName ) )
  {
    return QUrl( path.arg( widgetName ) );
  }
  else
  {
    return QUrl( path.arg( QLatin1String( "textedit" ) ) );
  }
}

const QgsEditorWidgetSetup InputUtils::getEditorWidgetSetup( const QgsField &field )
{
  if ( field.isNumeric() )
    return getEditorWidgetSetup( field, QStringLiteral( "Range" ) );
  else if ( field.isDateOrTime() )
    return getEditorWidgetSetup( field, QStringLiteral( "DateTime" ) );
  else if ( field.type() == QVariant::Bool )
    return getEditorWidgetSetup( field, QStringLiteral( "CheckBox" ) );
  else
    return getEditorWidgetSetup( field, QStringLiteral( "TextEdit" ) );
}

const QgsEditorWidgetSetup InputUtils::getEditorWidgetSetup( const QgsField &field, const QString &widgetType, const QVariantMap &additionalArgs )
{
  if ( field.name() == QStringLiteral( "fid" ) )
    return QgsEditorWidgetSetup( QStringLiteral( "Hidden" ), QVariantMap() );

  if ( widgetType.isEmpty() )
  {
    return QgsEditorWidgetSetup( QStringLiteral( "TextEdit" ), QVariantMap() );
  }
  else
  {
    QVariantMap config;
    config = config.unite( additionalArgs );

    if ( widgetType == QStringLiteral( "TextEdit" ) )
    {
      config.insert( QStringLiteral( "isMultiline" ), false );
      config.insert( QStringLiteral( "UseHtml" ), false );
    }
    else if ( widgetType == QStringLiteral( "DateTime" ) )
    {
      config.insert( QStringLiteral( "field_format" ), QgsDateTimeFieldFormatter::DATETIME_FORMAT );
      config.insert( QStringLiteral( "display_format" ), QgsDateTimeFieldFormatter::DATETIME_FORMAT );
    }
    else if ( widgetType == QStringLiteral( "Range" ) )
    {
      config.insert( QStringLiteral( "Style" ), QStringLiteral( "SpinBox" ) );
      config.insert( QStringLiteral( "Precision" ), QStringLiteral( "0" ) );
      config.insert( QStringLiteral( "Min" ), QString::number( INT_MIN ) );
      config.insert( QStringLiteral( "Max" ), QString::number( INT_MAX ) );
      config.insert( QStringLiteral( "Step" ), 1 );
    }
    else if ( widgetType == QStringLiteral( "ExternalResource" ) )
    {
      config.insert( QStringLiteral( "RelativeStorage" ), QStringLiteral( "1" ) );
      config.insert( QStringLiteral( "StorageMode" ), QStringLiteral( "0" ) );
      config.insert( QStringLiteral( "PropertyCollection" ), QVariantMap() );
      QgsPropertyCollection collection;
      config.insert( QStringLiteral( "PropertyCollection" ), collection.toVariant( QgsPropertiesDefinition() ) );
    }
    else if ( widgetType == QStringLiteral( "RelationReference" ) )
    {
      config.insert( QStringLiteral( "AllowNULL" ), true );
    }

    return QgsEditorWidgetSetup( widgetType, config );
  }
}

QString InputUtils::geometryFromLayer( QgsVectorLayer *layer )
{
  if ( layer )
  {
    switch ( layer->geometryType() )
    {
      case QgsWkbTypes::PointGeometry: return QStringLiteral( "point" );
      case QgsWkbTypes::LineGeometry: return QStringLiteral( "linestring" );
      case QgsWkbTypes::PolygonGeometry: return QStringLiteral( "polygon" );
      case QgsWkbTypes::NullGeometry: return QStringLiteral( "nullGeo" );
      default: return QString();
    }
  }
  return QString();
}

QString InputUtils::formatPoint(
  const QgsPoint &point,
  QgsCoordinateFormatter::Format format,
  int decimals,
  QgsCoordinateFormatter::FormatFlags flags )
{
  return QgsCoordinateFormatter::format( point, format, decimals, flags );
}

QString InputUtils::formatDistance( double distance,
                                    QgsUnitTypes::DistanceUnit units,
                                    int decimals,
                                    QgsUnitTypes::SystemOfMeasurement destSystem )
{
  double destDistance;
  QgsUnitTypes::DistanceUnit destUnits;

  humanReadableDistance( distance, units, destSystem, destDistance, destUnits );

  return QStringLiteral( "%1 %2" )
         .arg( QString::number( destDistance, 'f', decimals ) )
         .arg( QgsUnitTypes::toAbbreviatedString( destUnits ) );
}

void InputUtils::humanReadableDistance( double srcDistance, QgsUnitTypes::DistanceUnit srcUnits,
                                        QgsUnitTypes::SystemOfMeasurement destSystem,
                                        double &destDistance, QgsUnitTypes::DistanceUnit &destUnits )
{
  if ( ( destSystem == QgsUnitTypes::MetricSystem ) || ( destSystem == QgsUnitTypes::UnknownSystem ) )
  {
    return formatToMetricDistance( srcDistance, srcUnits, destDistance, destUnits );
  }
  else if ( destSystem == QgsUnitTypes::ImperialSystem )
  {
    return formatToImperialDistance( srcDistance, srcUnits, destDistance, destUnits );
  }
  else if ( destSystem == QgsUnitTypes::USCSSystem )
  {
    return formatToUSCSDistance( srcDistance, srcUnits, destDistance, destUnits );
  }
  else
  {
    Q_ASSERT( false ); //should never happen
  }
}

void InputUtils::formatToMetricDistance( double srcDistance,
    QgsUnitTypes::DistanceUnit srcUnits,
    double &destDistance,
    QgsUnitTypes::DistanceUnit &destUnits )
{
  double dist = srcDistance * QgsUnitTypes::fromUnitToUnitFactor( srcUnits, QgsUnitTypes::DistanceMillimeters );
  if ( dist < 0 )
  {
    destDistance = 0;
    destUnits = QgsUnitTypes::DistanceMillimeters;
    return;
  }

  double mmToKm = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceKilometers, QgsUnitTypes::DistanceMillimeters );
  if ( dist > mmToKm )
  {
    destDistance = dist / mmToKm;
    destUnits = QgsUnitTypes::DistanceKilometers;
    return;
  }

  double mmToM = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceMeters, QgsUnitTypes::DistanceMillimeters );
  if ( dist > mmToM )
  {
    destDistance = dist / mmToM;
    destUnits = QgsUnitTypes::DistanceMeters;
    return;
  }

  double mmToCm = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceCentimeters, QgsUnitTypes::DistanceMillimeters );
  if ( dist > mmToCm )
  {
    destDistance = dist / mmToCm;
    destUnits = QgsUnitTypes::DistanceCentimeters;
    return;
  }

  destDistance = dist;
  destUnits = QgsUnitTypes::DistanceMillimeters;
}

void InputUtils::formatToImperialDistance( double srcDistance,
    QgsUnitTypes::DistanceUnit srcUnits,
    double &destDistance,
    QgsUnitTypes::DistanceUnit &destUnits )
{
  double dist = srcDistance * QgsUnitTypes::fromUnitToUnitFactor( srcUnits, QgsUnitTypes::DistanceFeet );
  if ( dist < 0 )
  {
    destDistance = 0;
    destUnits = QgsUnitTypes::DistanceFeet;
    return;
  }

  double feetToMile = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceMiles, QgsUnitTypes::DistanceFeet );
  if ( dist > feetToMile )
  {
    destDistance = dist / feetToMile;
    destUnits = QgsUnitTypes::DistanceMiles;
    return;
  }

  double feetToYard = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceYards, QgsUnitTypes::DistanceFeet );
  if ( dist > feetToYard )
  {
    destDistance = dist / feetToYard;
    destUnits = QgsUnitTypes::DistanceYards;
    return;
  }

  destDistance = dist;
  destUnits = QgsUnitTypes::DistanceFeet;
  return;
}

void InputUtils::formatToUSCSDistance( double srcDistance,
                                       QgsUnitTypes::DistanceUnit srcUnits,
                                       double &destDistance,
                                       QgsUnitTypes::DistanceUnit &destUnits )
{
  double dist = srcDistance * QgsUnitTypes::fromUnitToUnitFactor( srcUnits, QgsUnitTypes::DistanceFeet );
  if ( dist < 0 )
  {
    destDistance = 0;
    destUnits = QgsUnitTypes::DistanceFeet;
    return;
  }

  double feetToMile = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceNauticalMiles, QgsUnitTypes::DistanceFeet );
  if ( dist > feetToMile )
  {
    destDistance = dist / feetToMile;
    destUnits = QgsUnitTypes::DistanceNauticalMiles;
    return;
  }

  double feetToYard = QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceYards, QgsUnitTypes::DistanceFeet );
  if ( dist > feetToYard )
  {
    destDistance = dist / feetToYard;
    destUnits = QgsUnitTypes::DistanceYards;
    return;
  }

  destDistance = dist;
  destUnits = QgsUnitTypes::DistanceFeet;
  return;
}

QString InputUtils::dumpScreenInfo() const
{
  QString msg;
  // take the first top level window
  const QWindowList windows = QGuiApplication::topLevelWindows();
  if ( !windows.isEmpty() )
  {
    QScreen *screen = windows.at( 0 )->screen();
    double dpiX = screen->physicalDotsPerInchX();
    double dpiY = screen->physicalDotsPerInchY();
    int height = screen->geometry().height();
    int width = screen->geometry().width();
    double sizeX = static_cast<double>( width ) / dpiX * 25.4;
    double sizeY = static_cast<double>( height ) / dpiY * 25.4;

    msg += tr( "screen resolution: %1x%2 px\n" ).arg( width ).arg( height );
    msg += tr( "screen DPI: %1x%2\n" ).arg( dpiX ).arg( dpiY );
    msg += tr( "screen size: %1x%2 mm\n" ).arg( QString::number( sizeX, 'f', 0 ), QString::number( sizeY, 'f', 0 ) );
    msg += tr( "screen density: %1" ).arg( QgsQuickUtils().screenDensity() );
  }
  else
  {
    msg += QLatin1String( "screen info: application is not initialized!" );
  }
  return msg;
}

QVariantMap InputUtils::createValueRelationCache( const QVariantMap &config, const QgsFeature &formFeature )
{
  QVariantMap valueMap;
  const QgsValueRelationFieldFormatter::ValueRelationCache cache = QgsValueRelationFieldFormatter::createCache( config, formFeature );

  for ( const QgsValueRelationFieldFormatter::ValueRelationItem &item : cache )
  {
    valueMap.insert( item.key.toString(), item.value );
  }
  return valueMap;
}

QString InputUtils::evaluateExpression( const FeatureLayerPair &pair, QgsProject *activeProject, const QString &expression )
{
  QList<QgsExpressionContextScope *> scopes;
  scopes << QgsExpressionContextUtils::globalScope();
  scopes << QgsExpressionContextUtils::projectScope( activeProject );
  scopes << QgsExpressionContextUtils::layerScope( pair.layer() );

  QgsExpressionContext context( scopes );
  context.setFeature( pair.feature() );
  QgsExpression expr( expression );
  return expr.evaluate( &context ).toString();
}

void InputUtils::selectFeaturesInLayer( QgsVectorLayer *layer, const QList<int> &fids, QgsVectorLayer::SelectBehavior behavior )
{
  QgsFeatureIds qgsFids;
  for ( const int &fid : fids )
    qgsFids << fid;
  layer->selectByIds( qgsFids, behavior );
}

QString InputUtils::fieldType( const QgsField &field )
{
  return QVariant( field.type() ).typeName();
}

QString InputUtils::dateTimeFieldFormat( const QString &fieldFormat )
{
  if ( QgsDateTimeFieldFormatter::DATE_FORMAT == fieldFormat )
  {
    return QString( "Date" );
  }
  else if ( QgsDateTimeFieldFormatter::TIME_FORMAT == fieldFormat )
  {
    return QString( "Time" );
  }
  else if ( QgsDateTimeFieldFormatter::DATETIME_FORMAT == fieldFormat )
  {
    return QString( "Date Time" );
  }
  else
  {
    return QString( "Date Time" );
  }
}

QModelIndex InputUtils::invalidIndex()
{
  return QModelIndex();
}

bool InputUtils::isFeatureIdValid( qint64 featureId )
{
  return !FID_IS_NEW( featureId ) && !FID_IS_NULL( featureId );
}

QgsQuickMapSettings *InputUtils::setupMapSettings( QgsProject *project, QgsQuickMapSettings *settings )
{
  if ( !project || !settings )
  {
    return nullptr;
  }

  QgsLayerTree *root = project->layerTreeRoot();

  // Get list of all visible and valid layers in the project
  QList< QgsMapLayer * > allLayers;
  foreach ( QgsLayerTreeLayer *nodeLayer, root->findLayers() )
  {
    if ( nodeLayer->isVisible() )
    {
      QgsMapLayer *layer = nodeLayer->layer();
      if ( layer && layer->isValid() )
      {
        allLayers << layer;
      }
    }
  }

  settings->setLayers( allLayers );
  settings->setTransformContext( project->transformContext() );

  return settings;
}
