#pragma once

#include "StoredQueryConfig.h"
#include "StoredQueryParamRegistry.h"
#include "SupportsExtraHandlerParams.h"
#include <engines/gis/CRSRegistry.h>
#include <ogr_geometry.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *  @brief Base class for adding bounding box support to stored query handler
 */
class SupportsBoundingBox : protected virtual SupportsExtraHandlerParams,
                            protected virtual StoredQueryParamRegistry
{
 public:
  SupportsBoundingBox(boost::shared_ptr<StoredQueryConfig> config,
                      SmartMet::Engine::Gis::CRSRegistry& crs_registry,
                      bool mandatory = false);

  virtual ~SupportsBoundingBox();

  bool get_bounding_box(const RequestParameterMap& param_values,
                        const std::string& default_crs,
                        SmartMet::Spine::BoundingBox* bbox) const;

  static void bbox2polygon(const SmartMet::Spine::BoundingBox& src,
                           OGRPolygon* dest,
                           const int npoints = 10);

  SmartMet::Spine::BoundingBox transform_bounding_box(const SmartMet::Spine::BoundingBox& src,
                                                      const std::string& dest_crs,
                                                      const int npoints = 10) const;

  /**
   * Test if a EPSG code use inverse coordinate order
   * @param crs EPSG code (eg. EPSG::4326)
   * @retval true The EPSG code is using Lat/Long or Northing/Easting order.
   * @retval false The EPSG code is using Long/Lat or Easting/Northing order.
   */
  bool isInverseAxisOrderPossible(const std::string& crs) const;

  /**
   * Swap XY coordinate order to YX or YX order to XY if BoundingBox crs (EPSG) allows it.
   * @param boundingBox Input bounding box.
   * @return Input bounding box with or without swapping.
   */
  SmartMet::Spine::BoundingBox swapXYByBoundingBoxCrs(
      const SmartMet::Spine::BoundingBox& boundingBox) const;

 protected:
  SmartMet::Engine::Gis::CRSRegistry& crs_registry;
  static const char* P_BOUNDING_BOX;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
