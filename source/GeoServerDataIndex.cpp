#include "GeoServerDataIndex.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <gis/OGR.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <iostream>
#include <sstream>

namespace bw = SmartMet::Plugin::WFS;
namespace pt = boost::posix_time;

namespace
{
std::vector<double> get_coords(const OGRLineString* geom)
{
  try
  {
    const int num_points = geom->getNumPoints();
    const int point_dim = geom->getCoordinateDimension();
    std::vector<double> result;
    for (int i = 0; i < num_points; i++)
    {
      result.push_back(geom->getX(i));
      if (point_dim > 1)
      {
        result.push_back(geom->getY(i));
        if (point_dim > 2)
        {
          result.push_back(geom->getZ(i));
        }
      }
    }
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

static void check_geometry(OGRGeometry* geom, const std::string& location, const std::string& gml)
{
  try
  {
    if (geom == nullptr)
    {
      std::ostringstream msg;
      msg << location << ": failed to parse GML format geometry '" << gml << "'";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool find_srs(const std::string& gml, int& srs)
{
  try
  {
    boost::regex r_srs("srsName=[\"]EPSG:([0-9]+)[\"]", boost::regex::perl | boost::regex::icase);
    boost::smatch match_results;
    boost::regex_search(gml, match_results, r_srs);
    if (match_results.size() >= 1)
    {
      std::string match = match_results[1];
      srs = std::stoi(match);
      return true;
    }
    else
    {
      srs = -1;
      return false;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

bw::GeoServerDataIndex::LayerRec::LayerRec()
    : name(""),
      layer(""),
      orig_geom(nullptr),
      dest_geom(nullptr),
      orig_srs(-1),
      dest_srs(-1),
      orig_srs_swapped(false),
      dest_srs_swapped(false)
{
}

bw::GeoServerDataIndex::LayerRec::LayerRec(
    const SmartMet::Plugin::WFS::GeoServerDataIndex::LayerRec& rec)
    : name(rec.name),
      layer(rec.layer),
      orig_geom(rec.orig_geom ? &dynamic_cast<OGRLineString&>(*(rec.orig_geom->clone())) : nullptr),
      dest_geom(rec.dest_geom ? &dynamic_cast<OGRLineString&>(*(rec.dest_geom->clone())) : nullptr),
      orig_srs(rec.orig_srs),
      dest_srs(rec.dest_srs),
      orig_srs_swapped(rec.orig_srs_swapped),
      dest_srs_swapped(rec.dest_srs_swapped),
      data_map(rec.data_map)
{
}

bw::GeoServerDataIndex::LayerRec::~LayerRec()
{
  clear();
}

bw::GeoServerDataIndex::LayerRec& bw::GeoServerDataIndex::LayerRec::operator=(
    const SmartMet::Plugin::WFS::GeoServerDataIndex::LayerRec& rec)
{
  try
  {
    name = rec.name;
    layer = rec.layer;
    orig_geom = rec.orig_geom ? &dynamic_cast<OGRLineString&>(*(rec.orig_geom->clone())) : nullptr;
    dest_geom = rec.dest_geom ? &dynamic_cast<OGRLineString&>(*(rec.dest_geom->clone())) : nullptr;
    orig_srs = rec.orig_srs;
    dest_srs = rec.dest_srs;
    orig_srs_swapped = rec.orig_srs_swapped;
    dest_srs_swapped = rec.dest_srs_swapped;

    return *this;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::GeoServerDataIndex::LayerRec::clear()
{
  try
  {
    if (orig_geom)
    {
      OGRGeometryFactory::destroyGeometry(orig_geom);
      orig_geom = nullptr;
    }
    if (dest_geom)
    {
      OGRGeometryFactory::destroyGeometry(dest_geom);
      dest_geom = nullptr;
    }
    orig_srs = -1;
    dest_srs = -1;
    orig_srs_swapped = false;
    dest_srs_swapped = false;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<double> bw::GeoServerDataIndex::LayerRec::get_orig_coords() const
{
  try
  {
    return get_coords(orig_geom);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<double> bw::GeoServerDataIndex::LayerRec::get_dest_coords() const
{
  try
  {
    return get_coords(dest_geom);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::GeoServerDataIndex::GeoServerDataIndex(GeoServerDB& db, const std::string& db_table_name_format)
    : db(db), db_table_name_def(db_table_name_format)
{
}

bw::GeoServerDataIndex::GeoServerDataIndex(
    GeoServerDB& db, const std::map<std::string, std::string>& db_table_name_map)
    : db(db), db_table_name_def(db_table_name_map)
{
}

bw::GeoServerDataIndex::GeoServerDataIndex(
    GeoServerDB& db, boost::function1<std::string, std::string> db_table_name_cb)
    : db(db), db_table_name_def(db_table_name_cb), data(), debug_level(0)
{
  try
  {
    if (not db_table_name_cb)
    {
      throw SmartMet::Spine::Exception(BCP, "Undefined callback provided as constructor argument!");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::GeoServerDataIndex::~GeoServerDataIndex() {}

void bw::GeoServerDataIndex::query(const boost::posix_time::ptime& begin,
                                   const boost::posix_time::ptime& end,
                                   const std::vector<std::string>& layers,
                                   const double* boundingBox,
                                   int boundingBoxCRS,
                                   int destCRS)
{
  try
  {
    std::for_each(layers.begin(),
                  layers.end(),
                  boost::bind(&bw::GeoServerDataIndex::query_single_layer,
                              this,
                              begin,
                              end,
                              ::_1,
                              boundingBox,
                              boundingBoxCRS,
                              destCRS));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::GeoServerDataIndex::query_single_layer(const boost::posix_time::ptime& begin,
                                                const boost::posix_time::ptime& end,
                                                const std::string& layer,
                                                const double* boundingBox,
                                                int boundingBoxCRS,
                                                int destCRS)
{
  try
  {
    const std::string sql =
        create_sql_request(begin, end, layer, boundingBox, boundingBoxCRS, destCRS);
    if (debug_level > 2)
    {
      std::ostringstream msg;
      msg << METHOD_NAME << "GeoServer SQL request for layer '" << layer << "':\n" << sql << "\n";
      std::cout << msg.str() << std::flush;
    }
    boost::shared_ptr<pqxx::connection> conn = db.get_conn();
    pqxx::work work(*conn);
    pqxx::result result = work.exec(sql);
    process_sql_result(result, layer);
    work.commit();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string bw::GeoServerDataIndex::get_db_table_name(const std::string& layer_name) const
{
  try
  {
    const int which = db_table_name_def.which();
    if (which == 0)
    {
      boost::basic_format<char> fmt(boost::get<std::string>(db_table_name_def));
      return boost::str(fmt % layer_name);
    }
    else if (which == 1)
    {
      const auto& name_map = boost::get<std::map<std::string, std::string> >(db_table_name_def);
      auto pos = name_map.find(layer_name);
      if (pos == name_map.end())
      {
        std::string sep = " ";
        std::ostringstream msg;
        msg << "Available layers are";
        BOOST_FOREACH (const auto& item, name_map)
        {
          msg << sep << '\'' << item.first << '\'';
          sep = ", ";
        }
        SmartMet::Spine::Exception exception(BCP, "Unknown layer '" + layer_name + "'!");
        exception.addDetail(msg.str());
        throw exception;
      }
      else
      {
        return pos->second;
      }
    }
    else
    {
      auto& cb = boost::get<boost::function1<std::string, std::string> >(db_table_name_def);
      return cb(layer_name);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string bw::GeoServerDataIndex::create_sql_request(const boost::posix_time::ptime& begin,
                                                       const boost::posix_time::ptime& end,
                                                       const std::string& layer,
                                                       const double* boundingBox,
                                                       int boundingBoxCRS,
                                                       int destCRS)
{
  try
  {
    const std::string geometry_str = create_geometry_str(boundingBox, boundingBoxCRS);
    const std::string s_begin = Fmi::to_iso_extended_string(begin) + "Z";
    const std::string s_end = Fmi::to_iso_extended_string(end) + "Z";

    std::ostringstream q_fields;
    std::ostringstream q_tables;
    std::ostringstream q_where;

    std::ostringstream q_geom_check;

    q_geom_check << "ST_Intersects(St_Transform(the_geom, " << boundingBoxCRS << "),"
                 << geometry_str << ")";

    std::ostringstream query_str;
    query_str << "SELECT\n"
              << "    time\n"
              << "    ,location\n"
              << "    ,ST_AsGml(ST_Boundary(the_geom))\n"
              << "    ,ST_AsGml(ST_Boundary(ST_Transform(the_geom, " << destCRS << ")))\n"
              << "    ,*\n"
              << "FROM\n"
              << "    " << get_db_table_name(layer) << "\n"
              << "WHERE\n"
              << "    " << q_geom_check.str() << "\n"
              << "    AND time >= '" << s_begin << "'\n"
              << "    AND time <= '" << s_end << "'\n\n"
              << "ORDER BY time DESC\n";

    return query_str.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string bw::GeoServerDataIndex::create_geometry_str(const double* boundingBox,
                                                        int boundingBoxCRS)
{
  try
  {
    std::ostringstream result;
    result << "ST_GeomFromText('POLYGON((" << boundingBox[0] << " " << boundingBox[1] << ","
           << boundingBox[2] << " " << boundingBox[1] << "," << boundingBox[2] << " "
           << boundingBox[3] << "," << boundingBox[0] << " " << boundingBox[3] << ","
           << boundingBox[0] << " " << boundingBox[1] << "))', " << boundingBoxCRS << ")";
    return result.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::GeoServerDataIndex::process_sql_result(pqxx::result& result, const std::string& layer)
{
  try
  {
    const unsigned col_begin = 4;
    const unsigned num_columns = result.columns();
    std::map<unsigned, std::string> col_names;
    for (unsigned i = col_begin; i < num_columns; i++)
    {
      col_names[i] = result.column_name(i);
    }

    for (auto row = result.begin(); row != result.end(); ++row)
    {
      const std::string s_epoch = row[0].as<std::string>();
      pt::ptime epoch = pt::time_from_string(s_epoch);

      Item& item = data[epoch];
      item.epoch = epoch;

      OGRGeometry* geom = nullptr;
      try
      {
        LayerRec rec;
        rec.name = row[1].as<std::string>();
        rec.layer = layer;

        const std::string s_orig_geom = row[2].as<std::string>();
        const std::string s_dest_geom = row[3].as<std::string>();

        geom = OGRGeometryFactory::createFromGML(s_orig_geom.c_str());
        check_geometry(geom, __PRETTY_FUNCTION__, s_orig_geom);
        find_srs(s_orig_geom, rec.orig_srs);
        // Change EPSG axis order if needed.
        // Axis order in the data returned from PostGis is always Long,Lat or Easting,Northing.
        OGRSpatialReference orig_ogr_sr;
        orig_ogr_sr.importFromEPSGA(rec.orig_srs);
        if (orig_ogr_sr.EPSGTreatsAsLatLong() or orig_ogr_sr.EPSGTreatsAsNorthingEasting())
        {
          geom->swapXY();
          rec.orig_srs_swapped = true;
        }

        rec.orig_geom = &dynamic_cast<OGRLineString&>(*geom);

        geom = OGRGeometryFactory::createFromGML(s_dest_geom.c_str());
        check_geometry(geom, __PRETTY_FUNCTION__, s_dest_geom);
        find_srs(s_dest_geom, rec.dest_srs);
        // Change EPSG axis order if needed.
        // Axis order in the data returned from PostGis is always Long,Lat or Easting,Northing.
        OGRSpatialReference dest_ogr_sr;
        dest_ogr_sr.importFromEPSGA(rec.dest_srs);
        if (dest_ogr_sr.EPSGTreatsAsLatLong() or dest_ogr_sr.EPSGTreatsAsNorthingEasting())
        {
          geom->swapXY();
          rec.dest_srs_swapped = true;
        }

        rec.dest_geom = &dynamic_cast<OGRLineString&>(*geom);

        for (unsigned i = col_begin; i < num_columns; i++)
        {
          const auto& value = row[i];
          if (not value.is_null())
          {
            SmartMet::Spine::Value tmp(value.as<std::string>());
            rec.data_map[col_names[i]] = tmp;
          }
        }

        if (debug_level > 2)
        {
          std::ostringstream msg;
          msg << SmartMet::Spine::log_time_str() << ": [WFS] [DEBUG] [" << METHOD_NAME << "]:"
              << " name='" << rec.name << "'"
              << " layer='" << rec.layer << "'"
              << " epoch='" << epoch << "'"
              << " orig_geom='" << Fmi::OGR::exportToWkt(*rec.orig_geom) << "'"
              << " dest_geom='" << Fmi::OGR::exportToWkt(*rec.dest_geom) << "'" << '\n';
          std::cout << msg.str() << std::flush;
        }

        item.layers.push_back(rec);
      }
      catch (...)
      {
        OGRGeometryFactory::destroyGeometry(geom);
        throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
