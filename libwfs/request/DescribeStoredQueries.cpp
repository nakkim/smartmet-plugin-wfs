#include "request/DescribeStoredQueries.h"
#include "WfsConst.h"
#include "WfsConvenience.h"
#include "XmlUtils.h"
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <algorithm>

namespace bw = SmartMet::Plugin::WFS;
namespace bwx = SmartMet::Plugin::WFS::Xml;
namespace ba = boost::algorithm;

using boost::format;
using boost::str;

namespace
{
struct XmlNamespeceDef
{
  std::string ns;
  std::string prefix;
  boost::optional<std::string> loc;
};
};  // namespace

bw::Request::DescribeStoredQueries::DescribeStoredQueries(const std::string& language,
                                                          const std::vector<std::string>& ids,
                                                          const bw::PluginImpl& plugin_impl)
  : RequestBase(language, plugin_impl)
{
  try
  {
    if (ids.empty())
    {
      show_hidden = false;
      this->ids = plugin_impl.get_stored_query_map().get_handler_names();
      // Ensure that stored query IDs are sorted if not specified explicitly.
      // This is mostly to get repeatable results when running tests.
      std::sort(this->ids.begin(), this->ids.end());
    }
    else
    {
      show_hidden = true;
      this->ids = ids;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::Request::DescribeStoredQueries::~DescribeStoredQueries() {}

bw::RequestBase::RequestType bw::Request::DescribeStoredQueries::get_type() const
{
  return DESCRIBE_STORED_QUERIES;
}

void bw::Request::DescribeStoredQueries::execute(std::ostream& output) const
{
  try
  {
    // FIXME: how to get the language from the request? XML schema does not seem to provide
    //        a possibilty.
    const std::string language = get_language();

    const auto& stored_query_map = plugin_impl.get_stored_query_map();
    auto template_formatter = plugin_impl.get_describe_stored_queries_formatter();

    int cnt = 0;
    CTPP::CDT hash;

    auto fmi_apikey = get_fmi_apikey();
    hash["fmi_apikey"] = fmi_apikey ? *fmi_apikey : "";
    hash["fmi_apikey_prefix"] = QueryBase::FMI_APIKEY_PREFIX_SUBST;
    auto hostname = get_hostname();
    hash["hostname"] = hostname ? *hostname : plugin_impl.get_fallback_hostname();
    auto protocol = get_protocol();
    hash["protocol"] = (protocol ? *protocol : plugin_impl.get_fallback_protocol()) + "://";

    BOOST_FOREACH (const auto& id, ids)
    {
      boost::shared_ptr<const StoredQueryHandlerBase> handler =
          stored_query_map.get_handler_by_name(id);

      if (show_hidden or not handler->is_hidden())
      {
        CTPP::CDT& sq = hash["storedQueryList"][cnt++];

        boost::shared_ptr<const StoredQueryConfig> config = handler->get_config();
        sq["id"] = config->get_query_id();
        sq["title"] = config->get_title(language);
        sq["abstract"] = config->get_abstract(language);

        const int N = config->get_num_param_desc();
        for (int k = 0; k < N; k++)
        {
          sq["params"][k]["name"] = config->get_param_name(k);
          sq["params"][k]["title"] = config->get_param_title(k, language);
          sq["params"][k]["abstract"] = config->get_param_abstract(k, language);
          sq["params"][k]["type"] = config->get_param_xml_type(k);
        }

        bool have_ns_loc = false;
        std::map<std::string, std::string> ns_map;
        const std::vector<std::string> return_types = handler->get_return_types();
        for (std::size_t k = 0; k < return_types.size(); k++)
        {
          const std::string& name = return_types[k];
          const auto* feature = plugin_impl.get_capabilities().find_feature(name);
          if (feature)
          {
            const std::string ns = feature->get_xml_namespace();
            if (ns_map.count(ns) == 0)
            {
              const int ind = ns_map.size();
              const std::string& prefix = ns_map[ns] = str(format("wfs_%03u") % (1 + ind));
              CTPP::CDT& ns_def = sq["namespaceDef"][ind];
              ns_def["prefix"] = prefix;
              ns_def["namespace"] = ns;
              const auto ns_loc = feature->get_xml_namespace_loc();
              if (ns_loc)
              {
                ns_def["nsLoc"] = *ns_loc;
                have_ns_loc = true;
              }
            }
          }
          else
          {
            std::ostringstream msg;
            Fmi::Exception exception(
                BCP, "INTERNAL ERROR: feature '" + name + "' not found!");
            exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
            throw exception;
          }
        }

        if (have_ns_loc)
        {
          sq["haveNamespaceLoc"] = 1;
        }

        for (std::size_t k = 0; k < return_types.size(); k++)
        {
          const std::string& name = return_types[k];
          CTPP::CDT& return_type = sq["returnTypes"][k];
          const auto* feature = plugin_impl.get_capabilities().find_feature(name);
          // No need to check for nullptr as it is done in earlier loop
          const std::string& xml_type = feature->get_xml_type();
          const std::string& ns = feature->get_xml_namespace();
          const std::string& prefix = ns_map.at(ns);
          return_type["name"] = prefix + ":" + xml_type;
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
    catch (const std::exception&)
    {
      Fmi::Exception exception(BCP, "Template formater exception!");
      exception.addDetail(log_messages.str());
      if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<bw::Request::DescribeStoredQueries>
bw::Request::DescribeStoredQueries::create_from_kvp(
    const std::string& language,
    const SmartMet::Spine::HTTP::Request& http_request,
    const PluginImpl& plugin_impl)
{
  try
  {
    assert((bool)plugin_impl.get_describe_stored_queries_formatter());

    bw::Request::DescribeStoredQueries::check_request_name(http_request, "DescribeStoredQueries");
    check_wfs_version(http_request);

    boost::shared_ptr<bw::Request::DescribeStoredQueries> result;
    // FIXME: verify required stuff from the request
    // FIXME: extract language from the request

    std::vector<std::string> ids;
    auto ids_str = http_request.getParameter("storedquery_id");
    if (ids_str)
    {
      std::string s_ids = ba::trim_copy(std::string(*ids_str));
      ba::split(ids, s_ids, ba::is_any_of(","), ba::token_compress_on);
      BOOST_FOREACH (auto& id, ids)
      {
        ba::trim(id);
      }
    }

    result.reset(new bw::Request::DescribeStoredQueries(language, ids, plugin_impl));
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<bw::Request::DescribeStoredQueries>
bw::Request::DescribeStoredQueries::create_from_xml(const std::string& language,
                                                    const xercesc::DOMDocument& document,
                                                    const PluginImpl& plugin_impl)
{
  try
  {
    assert((bool)plugin_impl.get_describe_stored_queries_formatter());

    check_request_name(document, "DescribeStoredQueries");

    boost::shared_ptr<bw::Request::DescribeStoredQueries> result;
    const xercesc::DOMElement* root = bw::RequestBase::get_xml_root(document);
    std::vector<std::string> ids;
    std::vector<xercesc::DOMElement*> elems =
        bwx::get_child_elements(*root, WFS_NAMESPACE_URI, "StoredQueryId");
    BOOST_FOREACH (const xercesc::DOMElement* elem, elems)
    {
      ids.push_back(ba::trim_copy(bwx::extract_text(*elem)));
    }

    result.reset(new bw::Request::DescribeStoredQueries(language, ids, plugin_impl));
    result->check_mandatory_attributes(document);
    // FIXME: extract language from the request
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

int bw::Request::DescribeStoredQueries::get_response_expires_seconds() const
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
