#include "XmlEntityResolver.h"
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/LocalFileInputSource.hpp>
#include <xercesc/util/Janitor.hpp>
#include <macgyver/Exception.h>
#include <spine/Convenience.h>
#include "XmlUtils.h"

using SmartMet::Plugin::WFS::Xml::EntityResolver;

namespace {

  bool is_url(const std::string &str)
  {
    return (str.substr(0, 7) == "http://") or (str.substr(0, 8) == "https://");
  }

  size_t append_data(char *ptr, size_t size, size_t nmemb, void *userdata)
  {
    std::ostream *ost = reinterpret_cast<std::ostream *>(userdata);
    ost->write(ptr, size * nmemb);
    return size * nmemb;
  }

} // anonymous namespace

EntityResolver::EntityResolver()
  : enable_download(false)
{
}

EntityResolver::~EntityResolver()
{
}

void
EntityResolver::init_schema_download(const std::string& proxy, const std::string& no_proxy)
{
  enable_download = true;
  this->proxy = proxy;
  this->no_proxy = no_proxy;
}

bool
EntityResolver::merge_downloaded_schemas()
{
  bool changed = false;
  std::unique_lock<std::mutex> lock(mutex);
  std::map<std::string, Download>::iterator curr, next;
  for (curr = download_map.begin(); curr != download_map.end(); curr = next) {
    next = curr;
    next++;
    try {
      const std::string uri = curr->first;
      const std::string content = curr->second.task.get();
      cache[uri] = content;
      download_map.erase(uri);
      changed = true;
    } catch (...) {
      // Ignore exceptions here
    }
  }
  return changed;
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

    xercesc::InputSource* source = get_schema(remote_uri);

    if (not source) {
      throw Fmi::Exception::Trace(BCP, "Failed to resolve URI '" + remote_uri + "'");
    }

    return source;
  }
 catch (...)
   {
     throw Fmi::Exception::Trace(BCP, "Operation failed!");
   }

xercesc::InputSource* EntityResolver::get_schema(const std::string& remote_uri)
try
  {
    std::string src;
    if (get_schema(remote_uri, src)) {
      std::size_t len = src.length();
      char *data = new char[len + 1];
      memcpy(data, src.c_str(), len + 1);
      xercesc::Janitor<XMLCh> x_remote_id(xercesc::XMLString::transcode(remote_uri.c_str()));
      return new xercesc::MemBufInputSource(reinterpret_cast<XMLByte *>(data),
					    len,
					    x_remote_id.get(),
					    true /* adopt buffer */);
    } else {
      return nullptr;
    }
  }
 catch (...)
   {
     throw Fmi::Exception::Trace(BCP, "Operation failed!");
   }

bool EntityResolver::get_schema(const std::string& uri, std::string& result)
try
{
  std::map<std::string, std::string>::const_iterator it;
  std::map<std::string, Download>::iterator it_d;
  std::unique_lock<std::mutex> lock(mutex);
  it = cache.find(uri);
  if (it == cache.end()) {
    if (enable_download) {
      bool downloading = false;
      it_d = download_map.find(uri);
      if (it_d == download_map.end()) {
	// URI encountered first time: try to download
	downloading = true;
	auto x = download_map.emplace(uri, Download());
	assert (not x.second); // Should not happen
	it_d = x.first;
      } else if (it_d->second.num_failed and it_d->second.num_failed < 100) {
	unsigned inc = it_d->second.num_failed < 10 ? 10 : 600;
	if (std::time(0) > it_d->second.last_failed + inc) {
	  downloading = true;
	}
      }

      if (downloading) {
	it_d->second.task = std::async(std::launch::async,
				       std::bind(&EntityResolver::download, this, uri));
      }

      lock.unlock();

      auto task = it_d->second.task;
      try {
	result = task.get();
	it_d->second.num_failed = 0;
      } catch (...) {
	if (downloading) {
	  it_d->second.num_failed++;
	  it_d->second.last_failed = std::time(0);
	}
	throw;
      }
      return true;
    } else {
      // Not found and downloading schemas is not allowed
      return false;
    }
  } else {
    result = it->second;
    return true;
  }
} catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

std::string EntityResolver::download(const std::string& uri) const
{
  CURL* curl = curl_easy_init();

  if (not curl) {
    throw Fmi::Exception::Trace(BCP, "curl_easy_init() call failed");
  }

  std::ostringstream result;

  const int verbose = 0;
  const int autoreferer = 1;
  const int fail_on_error = 1;
  const int content_decoding = 1;
  const int transfer_decoding = 1;
  const char *accept_encoding = "gzip, deflate";

  curl_easy_setopt(curl, CURLOPT_VERBOSE, &verbose);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &append_data);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
  curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());
  curl_easy_setopt(curl, CURLOPT_AUTOREFERER, &autoreferer);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, &fail_on_error);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, accept_encoding);
  curl_easy_setopt(curl, CURLOPT_HTTP_CONTENT_DECODING, &content_decoding);
  curl_easy_setopt(curl, CURLOPT_HTTP_TRANSFER_DECODING, &transfer_decoding);

  if (proxy != "") {
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
  }

  if (no_proxy != "") {
    curl_easy_setopt(curl, CURLOPT_NOPROXY, no_proxy.c_str());
  }

  std::cout << SmartMet::Spine::log_time_str() << ": [WFS] Trying to download XML schema from "
	    << uri << std::endl;

  CURLcode ret = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  if (ret == 0)
  {
    std::cout << SmartMet::Spine::log_time_str() << ": [WFS] Downloaded XML schema from "
	      << uri << std::endl;
    return result.str();
  } else {
    // FIXME: for some reason throwing Fmi::Exception causes it to crash instead
    //        of postponing exception handling. Throwing std::runtime_error works
    //throw Fmi::Exception::Trace(BCP, "Failed to download " + uri);
    throw std::runtime_error("Failed to download " + uri + ": " + curl_easy_strerror(ret));
  }
}
