#pragma once

#include "SupportsBoundingBox.h"
#include "stored_queries/StoredAtomQueryHandlerBase.h"
#include <boost/geometry/geometry.hpp>
#include <boost/shared_ptr.hpp>
#include <ogr_geometry.h>

namespace SmartMet
{
#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace Engine
{
namespace Geonames
{
class Engine;
}
namespace Querydata
{
class Engine;
struct MetaData;
}  // namespace Querydata
}  // namespace Engine
#endif

namespace Plugin
{
namespace WFS
{
class StoredQEDownloadQueryHandler : public StoredAtomQueryHandlerBase,
                                     protected SupportsBoundingBox
{
  typedef boost::geometry::model::point<float, 2, boost::geometry::cs::cartesian> point_t;
  typedef boost::geometry::model::box<point_t> box_t;
  typedef boost::geometry::model::ring<point_t> ring_t;

 public:
  StoredQEDownloadQueryHandler(SmartMet::Spine::Reactor* reactor,
                               boost::shared_ptr<StoredQueryConfig> config,
                               PluginImpl& plugin_impl,
                               boost::optional<std::string> template_file_name);

  virtual ~StoredQEDownloadQueryHandler();

  virtual void init_handler();

 protected:
  virtual void update_parameters(
      const RequestParameterMap& request_params,
      int seq_id,
      std::vector<boost::shared_ptr<RequestParameterMap> >& result) const;

 private:
  boost::shared_ptr<OGRPolygon> get_model_boundary(
      const SmartMet::Engine::Querydata::MetaData& meta_info,
      const std::string& crs_name,
      int num_side_points = 10) const;

  boost::shared_ptr<OGRGeometry> bbox_intersection(
      const SmartMet::Spine::BoundingBox& bbox,
      const SmartMet::Engine::Querydata::MetaData& meta_info) const;

  void add_bbox_info(RequestParameterMap* param_map,
                     const std::string& name,
                     OGRGeometry& geom) const;

  void add_boundary(RequestParameterMap* param_map,
                    const std::string& name,
                    OGRGeometry& polygon) const;

 private:
  SmartMet::Engine::Geonames::Engine* geo_engine;
  SmartMet::Engine::Querydata::Engine* q_engine;
  std::set<std::string> producers;
  std::set<std::string> formats;
  std::string default_format;
  const int debug_level;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
