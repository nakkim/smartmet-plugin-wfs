#include "SupportsQualityParameters.h"
#include <macgyver/StringConversion.h>
#include <spine/Exception.h>

namespace bw = SmartMet::Plugin::WFS;

const char* bw::SupportsQualityParameters::P_QUALITY_INFO = "qualityInfo";

bw::SupportsQualityParameters::SupportsQualityParameters(
    boost::shared_ptr<StoredQueryConfig> config)
    : bw::SupportsExtraHandlerParams(config, false)
{
  try
  {
    register_scalar_param<bool>(P_QUALITY_INFO, *config);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::SupportsQualityParameters::~SupportsQualityParameters() {}

bool bw::SupportsQualityParameters::isQCParameter(const std::string& name) const
{
  try
  {
    const std::string prefix = Fmi::ascii_tolower_copy(name.substr(0, 3));
    if (prefix.compare(0, 3, "qc_") == 0)
      return true;

    return false;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<std::string>::const_iterator bw::SupportsQualityParameters::firstQCParameter(
    const std::vector<std::string>& parameters) const
{
  try
  {
    for (auto it = parameters.begin(); it != parameters.end(); ++it)
    {
      const std::string prefix = Fmi::ascii_tolower_copy(it->substr(0, 3));
      if (prefix.compare(0, 3, "qc_") == 0)
        return it;
    }
    return parameters.end();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool bw::SupportsQualityParameters::supportQualityInfo(const RequestParameterMap& params) const
{
  try
  {
    return params.get_optional<bool>(P_QUALITY_INFO, false);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

/**

@page WFS_SQ_QUALITY_PARAMS Quality related parameters

@section WFS_SQ_QUALITY_PARAMS_INTRO Introduction

Stored query quality parameters contains parameters to handle quality
related information.

@section WFS_SQ_QUALITY_PARAMS_PARAMS The stored query handler built-in parameters of this group

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Data type</th>
<th>Description</th>
</tr>

<tr>
  <td>qualityInfo</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>bool</td>
  <td>Include quality code information in result if the value true is set,
      otherwise it is not included.</td>
</tr>

</table>

*/
