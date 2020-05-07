#pragma once

#include "GeoServerDB.h"

#include <engines/gis/GdalUtils.h>
#include <spine/Value.h>

#include <gdal/ogr_geometry.h>

#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/format.hpp>
#include <boost/function.hpp>
#include <boost/variant.hpp>

#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class GeoServerDataIndex
{
 public:
  struct LayerRec
  {
    std::string name;
    std::string layer;
    OGRLineString* orig_geom;
    OGRLineString* dest_geom;
    int orig_srs;
    int dest_srs;
    bool orig_srs_swapped;
    bool dest_srs_swapped;
    std::map<std::string, SmartMet::Spine::Value> data_map;

   public:
    LayerRec();
    LayerRec(const LayerRec& rec);
    virtual ~LayerRec();
    LayerRec& operator=(const LayerRec& rec);
    void clear();
    std::vector<double> get_orig_coords() const;
    std::vector<double> get_dest_coords() const;
  };

  struct Item
  {
    boost::posix_time::ptime epoch;
    std::vector<LayerRec> layers;
  };

 public:
  GeoServerDataIndex(GeoServerDB& db, const std::string& db_table_name_format = "mosaic.%1%");

  GeoServerDataIndex(GeoServerDB& db, const std::map<std::string, std::string>& db_table_name_map);

  GeoServerDataIndex(GeoServerDB& db, boost::function1<std::string, std::string> db_table_name_cb);

  virtual ~GeoServerDataIndex();

  void query(const boost::posix_time::ptime& begin,
             const boost::posix_time::ptime& end,
             const std::vector<std::string>& layers,
             const double* boundingBox,
             int boundingBoxCRS = 4326,
             int destCRS = 4326);

  void query_single_layer(const boost::posix_time::ptime& begin,
                          const boost::posix_time::ptime& end,
                          const std::string& layers,
                          const double* boundingBox,
                          int boundingBoxCRS = 4326,
                          int destCRS = 4326);

  inline std::map<boost::posix_time::ptime, Item>::const_iterator begin() const
  {
    return data.begin();
  }

  inline std::map<boost::posix_time::ptime, Item>::const_iterator end() const { return data.end(); }
  inline const std::map<boost::posix_time::ptime, Item>& get_data() const { return data; }
  inline std::size_t size() const { return data.size(); }
  std::string get_db_table_name(const std::string& layer_name) const;

  inline void set_debug_level(int debug_level) { this->debug_level = debug_level; }

 private:
  std::string create_sql_request(const boost::posix_time::ptime& begin,
                                 const boost::posix_time::ptime& end,
                                 const std::string& layer,
                                 const double* boundingBox,
                                 int boundingBoxCRS,
                                 int destCRS = 4326);

  static std::string create_geometry_str(const double* boundingBox, int boundingBoxCRS);

  void process_sql_result(pqxx::result& result, const std::string& layers);

 private:
  GeoServerDB& db;
  boost::variant<std::string,
                 std::map<std::string, std::string>,
                 boost::function1<std::string, std::string> >
      db_table_name_def;
  std::map<boost::posix_time::ptime, Item> data;
  int debug_level;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
