#pragma once

#include "PluginImpl.h"
#include "StoredContourQueryStructs.h"
#include "StoredQueryConfig.h"
#include "StoredQueryHandlerBase.h"
#include "StoredQueryHandlerFactoryDef.h"
#include "SupportsBoundingBox.h"
#include "SupportsExtraHandlerParams.h"
#include "SupportsTimeParameters.h"
#include "SupportsTimeZone.h"
#include "RequiresGridEngine.h"
#include "RequiresContourEngine.h"
#include "RequiresQEngine.h"
#include "RequiresGeoEngine.h"

#include <gis/OGR.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *   @brief Handler for StoredContourQuery stored query
 */
class StoredContourQueryHandler : public StoredQueryHandlerBase,
                                  protected virtual SupportsExtraHandlerParams,
                                  protected virtual RequiresGridEngine,
                                  protected virtual RequiresContourEngine,
                                  protected virtual RequiresQEngine,
                                  protected virtual RequiresGeoEngine,
                                  protected SupportsBoundingBox,
                                  protected SupportsTimeParameters,
                                  protected SupportsTimeZone
{
 public:
  StoredContourQueryHandler(SmartMet::Spine::Reactor* reactor,
                            boost::shared_ptr<StoredQueryConfig> config,
                            PluginImpl& plugin_impl,
                            boost::optional<std::string> template_file_name);
  virtual ~StoredContourQueryHandler();
  virtual void query(const StoredQuery& query,
                     const std::string& language,
		     const boost::optional<std::string>& hostname,
                     std::ostream& output) const;

 protected:

  virtual void  query_qEngine(
                    const StoredQuery& query,
                    const std::string& language,
                    std::ostream& output) const;

  virtual void  query_gridEngine(
                    const StoredQuery& query,
                    const std::string& language,
                    std::ostream& output) const;

  virtual void clipGeometry(OGRGeometryPtr& geom, Fmi::Box& bbox) const = 0;
  virtual std::vector<ContourQueryResultPtr> processQuery(
      ContourQueryParameter& queryParameter) const = 0;
  virtual SmartMet::Engine::Contour::Options getContourEngineOptions(
      const boost::posix_time::ptime& time, const ContourQueryParameter& queryParameter) const = 0;
  virtual boost::shared_ptr<ContourQueryParameter> getQueryParameter(
      const SmartMet::Spine::Parameter& parameter,
      const SmartMet::Engine::Querydata::Q& q,
      OGRSpatialReference& sr) const = 0;
  virtual void setResultHashValue(CTPP::CDT& resultHash,
                                  const ContourQueryResult& resultItem) const = 0;

  ContourQueryResultSet getContours(const ContourQueryParameter& queryParameter) const;
  ContourQueryResultSet getContours_qEngine(const ContourQueryParameter& queryParameter) const;
  ContourQueryResultSet getContours_gridEngine(const ContourQueryParameter& queryParameter) const;

  std::string name;
  FmiParameterName id;

 private:
  std::string formatCoordinates(const OGRGeometry* geom,
                                bool latLonOrder,
                                unsigned int precision) const;

  void parseQueryResults(const std::vector<ContourQueryResultPtr>& query_results,
                         const SmartMet::Spine::BoundingBox& bbox,
                         const std::string& language,
                         SmartMet::Spine::CRSRegistry& crsRegistry,
                         const std::string& requestedCRS,
                         const boost::posix_time::ptime& origintime,
                         const boost::posix_time::ptime& modificationtime,
                         const std::string& tz_name,
                         CTPP::CDT& hash) const;
  void parsePolygon(OGRPolygon* polygon,
                    bool latLonOrder,
                    unsigned int precision,
                    CTPP::CDT& polygon_patch) const;
  void parseGeometry(OGRGeometry* geom,
                     bool latLonOrder,
                     unsigned int precision,
                     CTPP::CDT& result) const;
  void handleGeometryCollection(OGRGeometryCollection* pGeometryCollection,
                                bool latLonOrder,
                                unsigned int precision,
                                std::vector<CTPP::CDT>& results) const;
  std::string double2string(double d, unsigned int precision) const;
  std::string bbox2string(const SmartMet::Spine::BoundingBox& bbox,
                          OGRSpatialReference& targetSRS) const;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
