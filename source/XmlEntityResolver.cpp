#include "XmlEntityResolver.h"
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/LocalFileInputSource.hpp>
#include <xercesc/util/Janitor.hpp>
#include <spine/Exception.h>
#include "XmlUtils.h"

using SmartMet::Plugin::WFS::Xml::EntityResolver;

namespace {

  bool is_url(const std::string &str)
  {
    return (str.substr(0, 7) == "http://") or (str.substr(0, 8) == "https://");
  }

} // anonymous namespace

EntityResolver::EntityResolver()
{
}

EntityResolver::~EntityResolver()
{
}

xercesc::InputSource *
EntityResolver::resolveEntity(xercesc::XMLResourceIdentifier *resource_identifier)
try
  {
    const XMLCh *x_public_id = resource_identifier->getPublicId();
    const XMLCh *x_system_id = resource_identifier->getSystemId();
    const XMLCh *x_base_uri = resource_identifier->getBaseURI();

    const std::string public_id = to_opt_string(x_public_id).first;
    const std::string system_id = to_opt_string(x_system_id).first;
    const std::string base_uri = to_opt_string(x_base_uri).first;

    std::string remote_uri;

    if (is_url(system_id))
      {
	remote_uri = system_id;
      }
    else if (system_id == "")
      {
	return nullptr;
      }
    else if ((*system_id.begin() == '/') or (*base_uri.begin() == '/'))
      {
	return new xercesc::LocalFileInputSource(x_base_uri, x_system_id);
      }
    else
      {
	std::size_t pos = base_uri.find_last_of("/");
	if (pos == std::string::npos)
	  return nullptr;
	remote_uri = base_uri.substr(0, pos + 1) + system_id;
      }

    xercesc::InputSource* source = try_preloaded_schema_cache(remote_uri);

    if (not source) {
      throw SmartMet::Spine::Exception::Trace(BCP, "Failed to resolve URI '" + remote_uri + "'");
    }

    return source;
  }
 catch (...)
   {
     throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
   }

xercesc::InputSource* EntityResolver::try_preloaded_schema_cache(const std::string& remote_uri)
try
  {
    auto it = cache.find(remote_uri);
    if (it == cache.end()) {
      return nullptr;
    } else {
      const std::string &src = it->second;
      std::size_t len = src.length();
      char *data = new char[len + 1];
      memcpy(data, src.c_str(), len + 1);
      xercesc::Janitor<XMLCh> x_remote_id(xercesc::XMLString::transcode(remote_uri.c_str()));
      return new xercesc::MemBufInputSource(reinterpret_cast<XMLByte *>(data),
					    len,
					    x_remote_id.get(),
					    true /* adopt buffer */);
    }
  }
 catch (...)
   {
     throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
   }
