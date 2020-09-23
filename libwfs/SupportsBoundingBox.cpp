#include "SupportsBoundingBox.h"
#include <boost/algorithm/string.hpp>
#include <gdal/ogr_geometry.h>
#include <macgyver/Exception.h>

namespace bw = SmartMet::Plugin::WFS;
namespace ba = boost::algorithm;
using SmartMet::Spine::BoundingBox;
using SmartMet::Spine::Value;

const char* bw::SupportsBoundingBox::P_BOUNDING_BOX = "boundingBox";

bw::SupportsBoundingBox::SupportsBoundingBox(StoredQueryConfig::Ptr config,
                                             SmartMet::Spine::CRSRegistry& crs_registry,
                                             bool mandatory)
    : StoredQueryParamRegistry(config)
    , SupportsExtraHandlerParams(config, false), crs_registry(crs_registry)
{
  try
  {
    register_scalar_param<BoundingBox>(P_BOUNDING_BOX, mandatory);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::SupportsBoundingBox::~SupportsBoundingBox() {}

bool bw::SupportsBoundingBox::get_bounding_box(const RequestParameterMap& param_values,
                                               const std::string& default_crs,
                                               SmartMet::Spine::BoundingBox* bbox) const
{
  try
  {
    assert(bbox != nullptr);

    if (param_values.count(P_BOUNDING_BOX))
    {
      *bbox = param_values.get_single<SmartMet::Spine::BoundingBox>(P_BOUNDING_BOX);

      // Bounding box without crs use always XY notation.
      // Swap the coords to XY order if order is YX.
      if (ba::trim_copy(bbox->crs) == "")
      {
        bbox->crs = default_crs;
      }
// else
//{
//      *bbox = swapXYByBoundingBoxCrs(*bbox);
//}
#if 0
        // CRS with height included are not supported here for bounding box
        // Additionally it is verified that CRS is known
        bool bb_show_height = false;
        crs_registry.get_attribute(bbox->crs, "showHeight", &bb_show_height);
        if (bb_show_height)
          {
            std::ostringstream msg;
            msg << METHOD_NAME << ": CRS " << bbox->crs << " is not supported"
                << " for specifying bounding box";
            Fmi::Exception exception(BCP,msg.str());
            exception.addParameter(WFS_EXCEPTION_CODE,WFS_OPERATION_PROCESSING_FAILED);
            throw exception;
          }
#endif
      return true;
    }
    else
    {
      return false;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::SupportsBoundingBox::bbox2polygon(const SmartMet::Spine::BoundingBox& src,
                                           OGRPolygon* polygon,
                                           const int npoints)
{
  try
  {
    assert(polygon != nullptr);

    OGRLinearRing boundary;

    double xStep = (src.xMax - src.xMin) / (npoints - 1);
    double yStep = (src.yMax - src.yMin) / (npoints - 1);

    for (int i = 0; i < npoints; i++)
      boundary.addPoint(src.xMin, src.yMin + i * yStep);
    for (int i = 1; i < npoints; i++)
      boundary.addPoint(src.xMin + i * xStep, src.yMax);
    for (int i = 1; i < npoints; i++)
      boundary.addPoint(src.xMax, src.yMax - i * yStep);
    for (int i = 1; i < npoints; i++)
      boundary.addPoint(src.xMax - i * xStep, src.yMin);

    boundary.closeRings();

    polygon->empty();
    polygon->addRing(&boundary);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

SmartMet::Spine::BoundingBox bw::SupportsBoundingBox::transform_bounding_box(
    const BoundingBox& src, const std::string& dest_crs, const int npoints) const
{
  try
  {
    auto transformation = crs_registry.create_transformation(src.crs, dest_crs);

    OGRPolygon polygon;
    bbox2polygon(src, &polygon, npoints);

    transformation->transform(polygon);

    OGREnvelope envelope;
    polygon.getEnvelope(&envelope);

    SmartMet::Spine::BoundingBox result;
    result.xMin = envelope.MinX;
    result.yMin = envelope.MinY;
    result.xMax = envelope.MaxX;
    result.yMax = envelope.MaxY;
    result.crs = transformation->get_dest_name();

    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool bw::SupportsBoundingBox::isInverseAxisOrderPossible(const std::string& crs) const
{
  try
  {
    int epsg = 0;
    if (crs_registry.get_attribute(crs, "epsg", &epsg))
    {
      OGRSpatialReference sr;
      sr.importFromEPSGA(epsg);
      if (sr.EPSGTreatsAsLatLong() or sr.EPSGTreatsAsNorthingEasting())
        return true;
    }
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

SmartMet::Spine::BoundingBox bw::SupportsBoundingBox::swapXYByBoundingBoxCrs(
    const SmartMet::Spine::BoundingBox& boundingBox) const
{
  try
  {
    SmartMet::Spine::BoundingBox bbox = boundingBox;
    if (isInverseAxisOrderPossible(bbox.crs))
    {
      bbox.xMin = boundingBox.yMin;
      bbox.yMin = boundingBox.xMin;
      bbox.xMax = boundingBox.yMax;
      bbox.yMax = boundingBox.xMax;
    }
    return bbox;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

/**

@page WFS_SQ_PARAM_BBOX Bounding box

@section WFS_SQ_PARAM_BBOX_INTRO Introduction

Deriving stored query handler C++ class from SmartMet::Plugin::WFS::SupportsBoundingBox additionally
to
SmartMet::Plugin::WFS::StoredQueryHandlerBase adds bounding box support to handler. Bounding box
support
is
implemented separately from other @ref WFS_SQ_LOCATION_PARAMS as it may be needed separatelly and
also
@ref WFS_SQ_LOCATION_PARAMS may be needed without bounding box support.

@section WFS_SQ_PARAM_BBOX_PARAM Built-in stored query handler parameters

Adds scalar stored query handler parameter @b boundingBox which has to map to either corresponding
stored query parameter or constant.

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Data type</th>
<th>Description</th>
</tr>

<tr>
  <td>boundingBox</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL "cfgScalarParameterTemplate"</td>
  <td>BoundingBox</td>
  <td>Specifies bounding box of an area</td>
</tr>

</table>

Bounding box can be specified in one of two possible ways (the second one is for XML requests only):
- string in format <b>MinX,MinY,MaxX,MaxY[,CRS]</b>, where
    - MinX and MinY is coordinates of left lower corner of the box
    - MaxX and MaxY is coordiantes of upper right cornet of the box
    - optional value @b CRS specifies the projection name (for example <b>EPSG:3067</b>). The
default
      is empty string. In that case stored query handler is required to interpret it correctly.
- XML type gml:Envelope (when this type provided as xmlType for the actual stored query parameter)

@section WFS_SQ_PARAM_BBOX_EXAMPLES Examples

<table boundary="1">

<tr>
<td>
@verbatim
boundingBox = "19,59,29,72,EPSG::4326";
@endverbatim
</td>
<td>
Fixed bounding box given in the configuration
</td>
</tr>

<tr>
<td>
@verbatim
boundingBox = "${bbox : 19,59,29,72,EPSG::4326}";
@endverbatim
</td>
<td>
Maps to stored query parameter @b bbox with the provided default value
</td>
</tr>

<tr>
<td>
@verbatim
boundingBox = ${bbox > defaultBbox}
@endverbatim
</td>
<td>
Maps to stored query parameter @b bbox. The value of named parameter @b defaultBbox
is used if parameter @b bbox is not provided in the query
</td>
</tr>

</table>

*/
