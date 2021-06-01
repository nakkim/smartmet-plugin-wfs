#include "request/GetCapabilities.h"
#include "WfsConst.h"
#include "WfsException.h"
#include "XmlUtils.h"
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <ctpp2/CDT.hpp>
#include <macgyver/TypeName.h>

namespace bw = SmartMet::Plugin::WFS;
namespace bwx = SmartMet::Plugin::WFS::Xml;

using SmartMet::Plugin::WFS::Request::GetCapabilities;

GetCapabilities::GetCapabilities(const std::string& language, const PluginImpl& plugin_impl)
  : RequestBase(language, plugin_impl)
{
  try
  {
    const auto lang_vect = plugin_impl.get_languages();
    std::copy(lang_vect.begin(), lang_vect.end(), std::inserter(languages, languages.begin()));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

GetCapabilities::~GetCapabilities() {}

bw::RequestBase::RequestType GetCapabilities::get_type() const
{
  return GET_CAPABILITIES;
}

void GetCapabilities::execute(std::ostream& output) const
{
  try
  {
    CTPP::CDT hash;
    const auto& capabilities = plugin_impl.get_capabilities();

    auto fmi_apikey = get_fmi_apikey();
    hash["fmi_apikey"] = fmi_apikey ? *fmi_apikey : "";
    hash["fmi_apikey_prefix"] = QueryBase::FMI_APIKEY_PREFIX_SUBST;
    auto hostname = get_hostname();
    hash["hostname"] = hostname ? *hostname : plugin_impl.get_fallback_hostname();
    auto protocol = get_protocol();
    hash["protocol"] = (protocol ? *protocol : plugin_impl.get_fallback_protocol()) + "://";

    std::string language;
    if (requested_language and languages.count(*requested_language) > 0)
    {
      language = *requested_language;
    }
    else
    {
      language = get_language();
    }

    hash["language"] = language;

    BOOST_FOREACH (const std::string& operation, capabilities.get_operations())
    {
      hash["operation"][operation] = "1";
    }

    int ind = 0;
    BOOST_FOREACH (const std::string& name, capabilities.get_used_features())
    {
      auto feature = capabilities.get_features().at(name);
      if (not feature->is_hidden()) {
	CTPP::CDT& f = hash["features"][ind++];

	std::string ns_alias = "wfsns001";
	std::size_t pos = feature->get_name().find_first_of(":");
	if (pos != std::string::npos)
	  ns_alias = feature->get_name().substr(0, pos);
	f["nsAlias"] = ns_alias;
	f["xmlType"] = feature->get_xml_type();
	f["name"] = feature->get_name();
	f["ns"] = feature->get_xml_namespace();
	auto loc = feature->get_xml_namespace_loc();
	if (loc)
	  f["loc"] = *loc;
	f["title"] = feature->get_title(language);
	f["abstract"] = feature->get_abstract(language);
	f["defaultCrs"] = feature->get_default_crs();
	int k = 0;
	BOOST_FOREACH (const std::string& crs, feature->get_other_crs())
	  {
	    f["otherCrs"][k++] = crs;
	  }
      }
    }

    ind = 0;
    BOOST_FOREACH (const auto& map_item, capabilities.get_data_set_map())
    {
      CTPP::CDT& f = hash["dataSets"][ind++];
      f["code"] = map_item.first;
      f["namespace"] = map_item.second;
    }

    plugin_impl.get_config().get_capabilities_config().apply(hash, language);
    //std::cout << hash.RecursiveDump(10) << std::endl;

    std::ostringstream log_messages;

    try
    {
      auto formatter = plugin_impl.get_get_capabilities_formater();
      assert(formatter != 0);
      std::ostringstream response;
      formatter->process(hash, response, log_messages);
      substitute_all(response.str(), output);
    }
    catch (const std::exception&)
    {
      Fmi::Exception exception(BCP, "Template formatter exception!");
      exception.addDetail(log_messages.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<GetCapabilities> GetCapabilities::create_from_kvp(
    const std::string& language,
    const SmartMet::Spine::HTTP::Request& http_request,
    const PluginImpl& plugin_impl)
{
  try
  {
    check_request_name(http_request, "GetCapabilities");
    boost::shared_ptr<GetCapabilities> request;
    // FIXME: verify required stuff from the request
    request.reset(new GetCapabilities(language, plugin_impl));
    request->requested_language = http_request.getParameter("language");
    return request;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<GetCapabilities> GetCapabilities::create_from_xml(
    const std::string& language,
    const xercesc::DOMDocument& document,
    const PluginImpl& plugin_impl)
{
  try
  {
    check_request_name(document, "GetCapabilities");
    boost::shared_ptr<GetCapabilities> request;
    request.reset(new GetCapabilities(language, plugin_impl));
    return request;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

int bw::Request::GetCapabilities::get_response_expires_seconds() const
{
  try
  {
    return plugin_impl.get_config().getDefaultExpiresSeconds();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
