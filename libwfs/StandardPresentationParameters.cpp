#include "StandardPresentationParameters.h"
#include "WfsConst.h"
#include "WfsConvenience.h"
#include "WfsException.h"
#include "XmlUtils.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/TypeName.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <limits>
#include <sstream>

namespace bw = SmartMet::Plugin::WFS;
namespace bwx = SmartMet::Plugin::WFS::Xml;
namespace ba = boost::algorithm;

const char* bw::StandardPresentationParameters::DEFAULT_OUTPUT_FORMAT =
    "application/gml+xml; version=3.2";

const char* bw::StandardPresentationParameters::DEBUG_OUTPUT_FORMAT = "debug";

std::vector<std::string> SUPPORTED_FORMATS = {
    "text/xml; subtype=gml/3.2",
    "text/xml; version=3.2",
    "application/gml+xml; subtype=gml/3.2",
    bw::StandardPresentationParameters::DEFAULT_OUTPUT_FORMAT};

bw::StandardPresentationParameters::StandardPresentationParameters()
    : have_counts(false),
      start_index(1),
      count(0x00FFFFFFU),
      output_format(DEFAULT_OUTPUT_FORMAT),
      result_type(SPP_RESULTS)
{
}

bw::StandardPresentationParameters::~StandardPresentationParameters() {}

void bw::StandardPresentationParameters::read_from_kvp(
    const SmartMet::Spine::HTTP::Request& request)
{
  try
  {
    have_counts =
        ((bool)request.getParameter("startindex")) or ((bool)request.getParameter("count"));

    start_index = get_param<spp_index_t>(request, "startindex", 0);
    count = get_param<spp_index_t>(request, "count", 0x00FFFFFFU);

    auto tmp = request.getParameter("outputformat");
    set_output_format(tmp ? *tmp : std::string(DEFAULT_OUTPUT_FORMAT));

    tmp = request.getParameter("resulttype");
    set_result_type(tmp ? *tmp : std::string("results"));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StandardPresentationParameters::read_from_xml(const xercesc::DOMElement& element)
{
  try
  {
    have_counts = false;
    auto tmp = bwx::get_attr(element, WFS_NAMESPACE_URI, "startIndex");
    if (tmp.second)
    {
      start_index = Fmi::stoul(tmp.first);
      have_counts = start_index != 0;
    }
    else
    {
      start_index = 0;
    }

    tmp = bwx::get_attr(element, WFS_NAMESPACE_URI, "count");
    if (tmp.second)
    {
      count = Fmi::stoul(tmp.first);
      have_counts = true;
    }
    else
    {
      count = 0x00FFFFFFU;
    }

    set_output_format(
        bwx::get_optional_attr(element, WFS_NAMESPACE_URI, "outputFormat", DEFAULT_OUTPUT_FORMAT));
    set_result_type(bwx::get_optional_attr(element, WFS_NAMESPACE_URI, "resultType", "results"));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StandardPresentationParameters::set_output_format(const std::string& str)
{
  try
  {
    const std::string lstr = Fmi::ascii_tolower_copy(str);

    auto iter = std::find(SUPPORTED_FORMATS.cbegin(), SUPPORTED_FORMATS.cend(), lstr);

    if (iter == SUPPORTED_FORMATS.cend())
    {
      // Handle debug
      if (lstr == bw::StandardPresentationParameters::DEBUG_OUTPUT_FORMAT)
      {
        output_format = bw::StandardPresentationParameters::DEBUG_OUTPUT_FORMAT;
      }
      else
      {
        SmartMet::Spine::Exception exception(
            BCP, "Incorrect value '" + lstr + "' of request parameter 'outputformat'!");
        exception.addDetail("The following formats are supported: " +
                            boost::algorithm::join(SUPPORTED_FORMATS, ", "));
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception;
      }
    }
    else
    {
      output_format = *iter;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StandardPresentationParameters::set_result_type(const std::string& str)
{
  try
  {
    const std::string lstr = Fmi::ascii_tolower_copy(str);
    if (lstr == "results")
    {
      result_type = SPP_RESULTS;
    }
    else if (lstr == "hits")
    {
      result_type = SPP_HITS;
    }
    else
    {
      SmartMet::Spine::Exception exception(
          BCP, "Incorrect value '" + str + "' of parameter 'resultType'!");
      exception.addDetail("Only the values 'results' and 'hits' are supported.");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
