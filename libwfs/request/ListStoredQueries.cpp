#include "request/ListStoredQueries.h"
#include "WfsConvenience.h"
#include "XmlUtils.h"
#include <ctpp2/CDT.hpp>

namespace bw = SmartMet::Plugin::WFS;

bw::Request::ListStoredQueries::ListStoredQueries(const std::string& language,
                                                  const PluginImpl& plugin_impl)
    : RequestBase(language, plugin_impl)
{
}

bw::Request::ListStoredQueries::~ListStoredQueries() {}

bw::RequestBase::RequestType bw::Request::ListStoredQueries::get_type() const
{
  return LIST_STORED_QUERIES;
}

void bw::Request::ListStoredQueries::execute(std::ostream& output) const
{
  try
  {
    const std::string language = get_language();
    const auto& stored_query_map = plugin_impl.get_stored_query_map();
    auto template_formatter = plugin_impl.get_list_stored_queries_formatter();

    CTPP::CDT hash;

    auto fmi_apikey = get_fmi_apikey();
    hash["fmi_apikey"] = fmi_apikey ? *fmi_apikey : "";
    hash["fmi_apikey_prefix"] = QueryBase::FMI_APIKEY_PREFIX_SUBST;
    auto hostname = get_hostname();
    hash["hostname"] = hostname ? *hostname : plugin_impl.get_fallback_hostname();
    auto protocol = get_protocol();
    hash["protocol"] = (protocol ? *protocol : plugin_impl.get_fallback_protocol()) + "://";

    std::vector<std::string> names = stored_query_map.get_handler_names();

    std::sort(names.begin(), names.end());

    std::size_t cnt = 0;
    for (std::size_t ind = 0; ind < names.size(); ind++)
    {
      boost::shared_ptr<const StoredQueryHandlerBase> p_handler =
          stored_query_map.get_handler_by_name(names[ind]);
      if (not p_handler->is_hidden())
      {
        CTPP::CDT& sq = hash["queryList"][cnt++];
        const std::string id = p_handler->get_query_name();
        const std::string title = p_handler->get_title(language);
        const std::vector<std::string> return_types = p_handler->get_return_types();
        sq["id"] = id;
        sq["title"] = title;
        for (std::size_t k = 0; k < return_types.size(); k++)
        {
          const std::string& name = return_types[k];
          CTPP::CDT& return_type = sq["returnTypes"][k];
          const auto* feature = plugin_impl.get_capabilities().find_feature(name);
          if (feature)
          {
            return_type["name"] = feature->get_xml_type();
            return_type["namespace"] = feature->get_xml_namespace();
            auto ns_loc = feature->get_xml_namespace_loc();
            if (ns_loc)
            {
              return_type["nsLoc"] = *ns_loc;
            }
          }
          else
          {
            SmartMet::Spine::Exception exception(
                BCP, "INTERNAL ERROR: feature '" + name + "' not found!");
            exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
            throw exception;
          }
        }
      }
    }

    std::ostringstream log_messages;
    try
    {
      std::ostringstream response;
      template_formatter->process(hash, response, log_messages);
      substitute_all(response.str(), output);
    }
    catch (...)
    {
      SmartMet::Spine::Exception exception(BCP, "Template formatter exception!", nullptr);
      exception.addDetail(log_messages.str());
      if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<bw::Request::ListStoredQueries> bw::Request::ListStoredQueries::create_from_kvp(
    const std::string& language,
    const SmartMet::Spine::HTTP::Request& http_request,
    const bw::PluginImpl& plugin_impl)

{
  try
  {
    bw::Request::ListStoredQueries::check_request_name(http_request, "ListStoredQueries");
    check_wfs_version(http_request);

    boost::shared_ptr<bw::Request::ListStoredQueries> result;
    // FIXME: verify required stuff from the request
    result.reset(new bw::Request::ListStoredQueries(language, plugin_impl));
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<bw::Request::ListStoredQueries> bw::Request::ListStoredQueries::create_from_xml(
    const std::string& language,
    const xercesc::DOMDocument& document,
    const bw::PluginImpl& plugin_impl)
{
  try
  {
    bw::Request::ListStoredQueries::check_request_name(document, "ListStoredQueries");
    boost::shared_ptr<bw::Request::ListStoredQueries> result;
    result.reset(new bw::Request::ListStoredQueries(language, plugin_impl));
    result->check_mandatory_attributes(document);
    // FIXME: extract language from the request
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

int bw::Request::ListStoredQueries::get_response_expires_seconds() const
{
  try
  {
    return plugin_impl.get_config().getDefaultExpiresSeconds();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
