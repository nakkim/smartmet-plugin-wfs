#include "XmlGmlTypes.h"
#include "WfsConst.h"
#include "WfsException.h"
#include "XmlUtils.h"
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/optional/optional_io.hpp>
#include <macgyver/TypeName.h>
#include <macgyver/Exception.h>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/util/Janitor.hpp>
#include <map>

namespace ba = boost::algorithm;

//#define CURR_FUNCT std::string("SmartMet::Plugin::WFS::Xml::") + __FUNCTION__

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Xml
{
boost::optional<GmlSRSInformationGroup> read_gml_srs_info_group(const xercesc::DOMElement& element)
{
  try
  {
    boost::optional<GmlSRSInformationGroup> result;
    auto x_axis_labels = get_attr(element, GML_NAMESPACE_URI, "axisLabels");
    auto x_uom_labels = get_attr(element, GML_NAMESPACE_URI, "uomLabels");

    if (x_axis_labels.second and x_uom_labels.second)
    {
      GmlSRSInformationGroup group;
      const std::string s_axis_labels = ba::trim_copy(x_axis_labels.first);
      const std::string s_uom_labels = ba::trim_copy(x_uom_labels.first);
      ba::split(group.axisLabels, s_axis_labels, ba::is_any_of(" \t"), ba::token_compress_on);
      ba::split(group.uomLabels, s_uom_labels, ba::is_any_of(" \t"), ba::token_compress_on);
      // FIXME: verify/ensure that vectors sizes are equal if needed
      result = group;
      return result;
    }
    else if (not(x_axis_labels.second or x_uom_labels.second))
    {
      return result;
    }
    else
    {
      Fmi::Exception exception(BCP, "Invalid attribute usage!");
      exception.addDetail(
          "Both attributes 'axisLabels' and 'uomLabels' or non of them must be specified.");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      exception.addParameter("XML", xml2string(&element));
      throw exception;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

GmlSRSReferenceGroup read_gml_srs_reference_group(const xercesc::DOMElement& element)
{
  try
  {
    GmlSRSReferenceGroup srs_reference;

    auto x_srs_name = get_attr(element, GML_NAMESPACE_URI, "srsName");
    auto x_srs_dim = get_attr(element, GML_NAMESPACE_URI, "srsDimension");

    srs_reference.srs_info = read_gml_srs_info_group(element);

    if (x_srs_name.second)
    {
      srs_reference.srs_name = x_srs_name.first;
    }

    if (x_srs_dim.second)
    {
      srs_reference.srs_dimension = std::stoul(x_srs_dim.first);
    }

    return srs_reference;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

GmlDirectPositionType read_gml_direct_position_type(const xercesc::DOMElement& element)
{
  try
  {
    GmlDirectPositionType result;
    result.srs_reference = read_gml_srs_reference_group(element);
    const std::string content = ba::trim_copy(extract_text(element));
    std::vector<std::string> items;
    ba::split(items, content, ba::is_any_of(" \t\r\n"), ba::token_compress_on);
    BOOST_FOREACH (const std::string& item, items)
    {
      result.data.push_back(std::stod(item));
    }

    if (result.srs_reference.srs_dimension and
        (*result.srs_reference.srs_dimension != result.data.size()))
    {
      std::ostringstream msg;
      msg << "The actual number " << result.data.size() << " of values does not match the value "
          << result.srs_reference.srs_dimension << " of the attribute srsDimension"
          << " in '" << xml2string(&element) << "'";

      Fmi::Exception exception(BCP, "Invalid number of values!");
      exception.addDetail(msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

GmlEnvelopeType read_gml_envelope_type(const xercesc::DOMElement& element)
{
  try
  {
    GmlEnvelopeType result;

    // NOTE: only lookup for correct combinations is performed as
    //       it is assumed that XML document has been validated

    std::vector<xercesc::DOMElement*> v1, v2;

    v1 = get_child_elements(element, GML_NAMESPACE_URI, "lowerCorner");
    v2 = get_child_elements(element, GML_NAMESPACE_URI, "upperCorner");
    if (v1.size() == 1 and v2.size() == 1)
    {
      result.lower_corner = read_gml_direct_position_type(*v1.at(0));
      result.upper_corner = read_gml_direct_position_type(*v2.at(0));
    }
    else
    {
      v1 = get_child_elements(element, GML_NAMESPACE_URI, "pos");
      if (v1.size() == 2)
      {
        result.lower_corner = read_gml_direct_position_type(*v1.at(0));
        result.upper_corner = read_gml_direct_position_type(*v1.at(1));
      }
      else
      {
        v1 = get_child_elements(element, GML_NAMESPACE_URI, "coordinates");
        if (v1.size() == 1)
        {
          Fmi::Exception exception(BCP, "Feature not implemented!");
          exception.addDetail(
              "Support of deprecated gml::coordinates in gml::EnvelopeType is not yet "
              "implemented.");
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
          throw exception;
        }
        else
        {
          Fmi::Exception exception(BCP,
                                               "Failed to read 'gml::EnvelopeType' from the XML!");
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
          exception.addParameter("XML", xml2string(&element));
          throw exception;
        }
      }
    }

    result.srs_reference = read_gml_srs_reference_group(element);

    if ((result.lower_corner.data.size() == result.upper_corner.data.size()) and
        (not result.srs_reference.srs_dimension or
         ((*result.srs_reference.srs_dimension == result.lower_corner.data.size()) and
          ((*result.srs_reference.srs_dimension == result.upper_corner.data.size())))))
    {
      // Assign value if srsDimension is nowhere specified and data are consistent
      result.srs_reference.srs_dimension = result.lower_corner.data.size();
      return result;
    }
    else
    {
      Fmi::Exception exception(BCP, "Conflicting data provided in the XML!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      exception.addParameter("XML", xml2string(&element));
      throw exception;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Xml
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
