#include "request/DescribeFeatureType.h"
#include "WfsConst.h"
#include "WfsException.h"
#include "XmlUtils.h"
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <ctpp2/CDT.hpp>
#include <macgyver/TypeName.h>
#include <sstream>

namespace bw = SmartMet::Plugin::WFS;
namespace bwx = SmartMet::Plugin::WFS::Xml;

namespace ba = boost::algorithm;

bw::Request::DescribeFeatureType::DescribeFeatureType(
    const std::string& language,
    const std::vector<std::pair<std::string, std::string> >& type_names,
    const PluginImpl& plugin_impl)
  : RequestBase(language, plugin_impl), type_names(type_names)
{
}

bw::Request::DescribeFeatureType::~DescribeFeatureType() {}

bw::RequestBase::RequestType bw::Request::DescribeFeatureType::get_type() const
{
  return DESCRIBE_FEATURE_TYPE;
}

void bw::Request::DescribeFeatureType::execute(std::ostream& output) const
{
  try
  {
    auto template_formatter = plugin_impl.get_feature_type_formatter();
    std::map<std::string, boost::optional<std::string> > namespaces;
    const auto& features = plugin_impl.get_capabilities().get_features();

    if (type_names.empty())
    {
      // No type names specified: collect all types from WfsCapabilities object
      BOOST_FOREACH (const auto& name, plugin_impl.get_capabilities().get_used_features())
      {
        auto feature = features.at(name);
        const std::string& ns = feature->get_xml_namespace();
        boost::optional<std::string> loc = feature->get_xml_namespace_loc();

        // No check done here that namespace location is the same
        // Should be checked when parsing features instead
        namespaces.insert(std::make_pair(ns, loc));
      }
    }
    else
    {
      BOOST_FOREACH (const auto& item, type_names)
      {
        bool found = false;
        const std::string& name = item.second;
        const std::string& ns = item.first;
        // FIXME: optimize loop if needed
        const auto& used_names = plugin_impl.get_capabilities().get_used_features();
        for (auto it = used_names.begin(); not found and it != used_names.end(); ++it)
        {
          auto feature = features.at(*it);
          const std::string& item_ns = feature->get_xml_namespace();
          boost::optional<std::string> loc = feature->get_xml_namespace_loc();
          if ((name == feature->get_xml_type()) and ((ns == "*") or (ns == item_ns)))
          {
            namespaces.insert(std::make_pair(item_ns, loc));
            found = true;
          }
        }

        if (not found)
        {
          std::ostringstream msg;
          msg << "Type '{" << ns << "}:" << name << "' not found";
          SmartMet::Spine::Exception exception(BCP, msg.str());
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
          throw exception;
        }
      }
    }

    CTPP::CDT hash;

    auto fmi_apikey = get_fmi_apikey();
    hash["fmi_apikey"] = fmi_apikey ? *fmi_apikey : "";
    auto hostname = get_hostname();
    hash["hostname"] = hostname ? *hostname : plugin_impl.get_fallback_hostname();
    auto protocol = get_protocol();
    hash["protocol"] = (protocol ? *protocol : plugin_impl.get_fallback_protocol()) + "://";

    int ind = 0;
    for (auto it = namespaces.begin(); it != namespaces.end(); ++it)
    {
      const std::string& name = it->first;
      const boost::optional<std::string>& loc = it->second;
      if (ind == 0)
      {
        hash["target_namespace"] = name;
        hash["target_prefix"] = "wfsns001";
        hash["target_location"] = loc ? *loc : name;
      }
      else
      {
        hash["ns_list"][ind - 1]["namespace"] = name;
        if (loc)
        {
          hash["ns_list"][ind - 1]["location"] = *loc;
        }
      }

      ind++;
    }

    std::ostringstream log_messages;
    try
    {
      template_formatter->process(hash, output, log_messages);
    }
    catch (const std::exception&)
    {
      std::ostringstream msg;
      msg << "Template formatter exception '" << Fmi::current_exception_type()
          << "' catched: " << log_messages.str();
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool bw::Request::DescribeFeatureType::may_validate_xml() const
{
  return false;
}

boost::shared_ptr<bw::Request::DescribeFeatureType>
bw::Request::DescribeFeatureType::create_from_kvp(
    const std::string& language,
    const SmartMet::Spine::HTTP::Request& http_request,
    const PluginImpl& plugin_impl)
{
  try
  {
    assert((bool)plugin_impl.get_feature_type_formatter());

    check_request_name(http_request, "DescribeFeatureType");
    check_wfs_version(http_request);

    auto format = http_request.getParameter("outputformat");
    if (format)
    {
      check_output_format_attribute(*format);
    }

    std::vector<std::string> type_params;
    std::vector<std::pair<std::string, std::string> > type_names;
    auto type_names_str = http_request.getParameter("typename");
    if (type_names_str)
    {
      const std::string tmp = ba::trim_copy(*type_names_str);
      ba::split(type_params, tmp, ba::is_any_of(","));
    }

    BOOST_FOREACH (const auto& item, type_params)
    {
      // FIXME: do real parsing
      // Currently simply ignore namespace prefix as we have no means how to map it
      // to the namespace here
      std::string name = item;
      std::size_t pos = name.find_last_of(":");
      if (pos != std::string::npos)
      {
        name = name.substr(pos + 1);
      }

      type_names.push_back(std::make_pair(std::string("*"), name));
    }

    boost::shared_ptr<bw::Request::DescribeFeatureType> result;
    result.reset(new bw::Request::DescribeFeatureType(language, type_names, plugin_impl));
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<bw::Request::DescribeFeatureType>
bw::Request::DescribeFeatureType::create_from_xml(const std::string& language,
                                                  const xercesc::DOMDocument& document,
                                                  const PluginImpl& plugin_impl)
{
  try
  {
    assert((bool)plugin_impl.get_feature_type_formatter());

    check_request_name(document, "DescribeFeatureType");
    boost::shared_ptr<bw::Request::DescribeFeatureType> result;
    const xercesc::DOMElement* root = bw::RequestBase::get_xml_root(document);
    std::vector<std::pair<std::string, std::string> > type_names;
    std::vector<xercesc::DOMElement*> elems =
        bwx::get_child_elements(*root, WFS_NAMESPACE_URI, "TypeName");
    BOOST_FOREACH (const xercesc::DOMElement* elem, elems)
    {
      const std::string item = ba::trim_copy(bwx::extract_text(*elem));
      const std::size_t pos = item.find_last_of(":");
      const XMLCh* x_uri = nullptr;
      std::string name;
      if (pos == std::string::npos)
      {
        name = item;
        x_uri = document.lookupNamespaceURI(nullptr);
      }
      else
      {
        const std::string prefix = item.substr(0, pos);
        name = item.substr(pos + 1);
        xercesc::Janitor<XMLCh> x_prefix(xercesc::XMLString::transcode(prefix.c_str()));
        x_uri = document.lookupNamespaceURI(x_prefix.get());
      }

      if (x_uri == nullptr)
      {
        std::ostringstream msg;
        msg << "bw::Request::DescribeFeatureType::create_from_xml(): failed to get XML namespace"
            << " for '" << item << "'";
        SmartMet::Spine::Exception exception(BCP, msg.str());
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception;
      }

      type_names.push_back(std::make_pair(Xml::to_string(x_uri), name));
    }

    result.reset(new bw::Request::DescribeFeatureType(language, type_names, plugin_impl));
    result->check_mandatory_attributes(document);
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

int bw::Request::DescribeFeatureType::get_response_expires_seconds() const
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
