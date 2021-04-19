#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/serialization/map.hpp>
#include <xercesc/util/BinFileInputStream.hpp>
#include <xercesc/framework/BinOutputStream.hpp>
#include <xercesc/framework/XMLGrammarPoolImpl.hpp>
#include <xercesc/framework/LocalFileInputSource.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/internal/BinFileOutputStream.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/sax/ErrorHandler.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/util/Janitor.hpp>
#include <xercesc/util/XMLEntityResolver.hpp>
#include <getopt.h>
#include <curl/curl.h>
#include <openssl/sha.h>

namespace ba = boost::algorithm;
namespace fs = boost::filesystem;

// Renamed recently (one needs to support both versions)
#if LIBCURL_VERSION_NUM < 0x071506
#define CURLOPT_ACCEPT_ENCODING CURLOPT_ENCODING
#endif

std::string to_string(const XMLCh *src)
{
  if (src)
  {
    xercesc::Janitor<char> tmp(xercesc::XMLString::transcode(src));
    return tmp.get();
  }
  else
  {
    return "";
  }
}

bool is_url(const std::string &str)
{
  return (str.substr(0, 7) == "http://") or (str.substr(0, 8) == "https://");
}

class MyErrorHandler : public xercesc::ErrorHandler
{
 public:
  MyErrorHandler() : num_warnings(0), num_errors(0), num_fatal_errors(0) {}
  virtual ~MyErrorHandler() {}
  virtual void warning(const xercesc::SAXParseException &exc)
  {
    num_warnings++;
    output_message("WARNING", exc);
  }

  virtual void error(const xercesc::SAXParseException &exc)
  {
    num_errors++;
    output_message("ERROR", exc);
  }

  virtual void fatalError(const xercesc::SAXParseException &exc)
  {
    num_fatal_errors++;
    output_message("FATAL ERROR", exc);
    throw std::runtime_error("Fatal error parsing XML file");
  }

  virtual void resetErrors()
  {
    num_warnings = 0;
    num_errors = 0;
    num_fatal_errors = 0;
  }

  bool isOk() const { return (num_errors == 0) and (num_fatal_errors == 0); }
 private:
  void output_message(const std::string &prefix, const xercesc::SAXParseException &exc)
  {
    std::cerr << prefix << ": " << to_string(exc.getMessage()) << " at " << exc.getLineNumber()
              << ':' << exc.getColumnNumber() << ", publicId='" << to_string(exc.getPublicId())
              << "', systemId='" << to_string(exc.getSystemId()) << "'"
	      << std::endl;
  }

 private:
  int num_warnings;
  int num_errors;
  int num_fatal_errors;
};

class MyEntityResolver : public xercesc::XMLEntityResolver
{
  CURL *curl;

  std::map<std::string, std::string> cache;
  bool verbose;

 public:
  MyEntityResolver(bool verbose = false) : verbose(verbose) { curl = curl_easy_init(); }
  virtual ~MyEntityResolver() { curl_easy_cleanup(curl); }
  virtual xercesc::InputSource *resolveEntity(xercesc::XMLResourceIdentifier *resource_identifier)
  {
    const XMLCh *x_system_id = resource_identifier->getSystemId();
    const XMLCh *x_base_uri = resource_identifier->getBaseURI();
    const std::string public_id = to_string(resource_identifier->getPublicId());
    const std::string system_id = to_string(x_system_id);
    const std::string base_uri = to_string(x_base_uri);

    std::string remote_uri;

    if (is_url(system_id))
    {
      remote_uri = system_id;
    }
    else if (system_id == "")
    {
      return NULL;
    }
    else if ((*system_id.begin() == '/') or (*base_uri.begin() == '/'))
    {
      return new xercesc::LocalFileInputSource(x_base_uri, x_system_id);
    }
    else
    {
      std::size_t pos = base_uri.find_last_of("/");
      if (pos == std::string::npos) return NULL;
      remote_uri = base_uri.substr(0, pos + 1) + system_id;
    }

    if (cache.count(remote_uri))
    {
      const std::string &src = cache.at(remote_uri);
      std::size_t len = src.length();
      char *data = new char[len + 1];
      memcpy(data, src.c_str(), len + 1);
      xercesc::Janitor<XMLCh> x_remote_id(xercesc::XMLString::transcode(remote_uri.c_str()));
      return new xercesc::MemBufInputSource(
          reinterpret_cast<XMLByte *>(data), len, x_remote_id.get(), true /* adopt buffer */);
    } else {
        std::ostringstream fn_str;
        unsigned char md[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(remote_uri.c_str()), remote_uri.length(), md);
        fn_str << "local_cache/";
        for (unsigned i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            char tmp[3];
            snprintf(tmp, 3, "%02X", md[i]);
            fn_str << tmp;
        }
        const std::string fn = fn_str.str();
        try {
            if (boost::filesystem::exists(fn)) {
                xercesc::Janitor<XMLCh> x_remote_id(xercesc::XMLString::transcode(fn.c_str()));
                return new xercesc::LocalFileInputSource(x_remote_id.get());
            }
        } catch (const std::exception& e) {
            std::cerr << "fn: " << e.what() << std::endl;
        }
    }

    std::cout << "## Resolving  system_id='" << system_id << "'   base_uri='" << base_uri << "'"
              << std::endl;

    const int verbose = this->verbose ? 1 : 0;
    const int autoreferer = 1;
    const int fail_on_error = 1;
    const long follow_location = 1;
    const int content_decoding = 1;
    const int transfer_cododing = 1;
    const char *accept_encoding = "gzip, deflate";
    char error_buffer[CURL_ERROR_SIZE];

    std::ostringstream output;

    std::memset(error_buffer, 0, sizeof(error_buffer));
    curl_easy_reset(curl);
    if (verbose) curl_easy_setopt(curl, CURLOPT_VERBOSE, &verbose);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &MyEntityResolver::curl_write_function);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
    curl_easy_setopt(curl, CURLOPT_URL, remote_uri.c_str());
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, &autoreferer);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, &fail_on_error);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, &follow_location);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, accept_encoding);
    curl_easy_setopt(curl, CURLOPT_HTTP_CONTENT_DECODING, &content_decoding);
    curl_easy_setopt(curl, CURLOPT_HTTP_TRANSFER_DECODING, &transfer_cododing);

    CURLcode ret = curl_easy_perform(curl);
    if (ret == 0)
    {
      const std::string &src = output.str();
      cache[remote_uri] = src;
      std::size_t len = src.length();
      char *data = new char[len + 1];
      memcpy(data, src.c_str(), len + 1);
      xercesc::Janitor<XMLCh> x_remote_id(xercesc::XMLString::transcode(remote_uri.c_str()));
      return new xercesc::MemBufInputSource(
          reinterpret_cast<XMLByte *>(data), len, x_remote_id.get(), true /* adopt buffer */);
    }
    else
    {
      return NULL;
    }
  }

  int write_schemas(const fs::path &dir)
  {
    int ret_code = 0;
    for (std::map<std::string, std::string>::const_iterator it = cache.begin(); it != cache.end();
         ++it)
    {
      const std::string &url = it->first;
      const std::string &content = it->second;
      const std::string editted_url = ba::replace_first_copy(url, "://", "/");
      fs::path fn = dir;
      fn /= editted_url;
      if (fn.filename().native() == ".")
      {
        std::string n1 = fn.parent_path().filename().native() + ".xsd";
        fn = fn.parent_path();
        fn /= n1;
      }

      try
      {
        fs::create_directories(fn.parent_path());
        std::ofstream output;
        output.exceptions(std::ios::badbit | std::ios::failbit);
        output.open(fn.native().c_str());
        output.write(content.c_str(), content.length());
        std::cout << "## Wrote " << url << " to local file " << fn.native() << std::endl;
      }
      catch (const std::exception &err)
      {
        std::cerr << "Failed to write file '" << fn.native() << "':" << err.what() << std::endl;
        ret_code = 1;
      }
    }

    return ret_code;
  }

  template <class Archive>
  void serialize(Archive &ar, const unsigned int version)
  {
    (void)version;
    ar &BOOST_SERIALIZATION_NVP(cache);
  }

 private:
  static size_t curl_write_function(char *ptr, size_t size, size_t nmemb, void *userdata)
  {
    std::ostream *ost = reinterpret_cast<std::ostream *>(userdata);
    ost->write(ptr, size * nmemb);
    return size * nmemb;
  }
};

void usage(const char *argv0)
{
  std::cout << argv0 << ": parse provided XML files and dump XML final grammar pool to the file\n"
            << "\n"
            << "Command line format:\n"
            << "  " << argv0 << "-i grammar_ppol_fn_1 -o grammar_pool_output_fn file1 [file2 ...]\n"
            << "\n"
            << "Parameters:\n"
            << "   -i    specifies XML grammar pool dump file to load at the start\n"
            << "   -o    specifies XML grammar pool file to write at end\n"
            << "   -w    specifies top of directory hierarchy where to write XML schemas\n"
            << "         read from network (the default actions is not to write loaded\n"
            << "         schemas anywhere\n"
            << "   -s    serializes loaded schemas to provided file\n"
            << "   -v    verbose libCURL output\n"
            << std::endl;
}

int run(int argc, char *argv[])
{
  bool verbose = false;
  std::string input_fn = "";
  std::string output_fn = "XMLGrammarPool.dump";
  boost::optional<std::string> output_dir;
  boost::optional<std::string> serialize_fn;
  std::vector<std::string> files_to_parse;

  int opt;
  while ((opt = getopt(argc, argv, "hi:o:s:w:v")) != -1)
  {
    switch (opt)
    {
      case 'i':
        input_fn = optarg;
        break;

      case 'o':
        output_fn = optarg;
        break;

      case 'w':
        output_dir = optarg;
        break;

      case 's':
        serialize_fn = optarg;
        break;

      case 'v':
        verbose = true;
        break;

      default:
        usage(argv[0]);
        return 1;
    }
  }

  while (optind < argc)
  {
    files_to_parse.push_back(argv[optind++]);
  }

  if (files_to_parse.empty())
  {
    std::cerr << argv[0] << ": At least one file must be specified" << std::endl;
    return 1;
  }

  MyErrorHandler error_handler;
  MyEntityResolver entity_resolver(verbose);

  xercesc::XMLGrammarPoolImpl grammar_pool;

  if (input_fn != "")
  {
    std::cout << "##\n"
              << "## Loading grammar pool from " << input_fn << "\n"
              << "##" << std::endl;
    xercesc::BinFileInputStream input(input_fn.c_str());
    grammar_pool.deserializeGrammars(&input);
  }

  xercesc::XercesDOMParser parser(NULL, xercesc::XMLPlatformUtils::fgMemoryManager, &grammar_pool);
  parser.setErrorHandler(&error_handler);
  parser.setXMLEntityResolver(&entity_resolver);
  parser.setDoNamespaces(true);
  parser.setValidationScheme(xercesc::XercesDOMParser::Val_Always);
  parser.setExitOnFirstFatalError(true);
  parser.setDoSchema(true);
  parser.setLoadSchema(true);
  parser.setHandleMultipleImports(true);
  parser.useCachedGrammarInParse(true);
  parser.cacheGrammarFromParse(true);

  for (std::vector<std::string>::const_iterator it = files_to_parse.begin();
       it != files_to_parse.end() and error_handler.isOk();
       ++it)
  {
    std::cout << "##\n"
              << "## Parsing " << *it << "\n"
              << "##" << std::endl;
    parser.parse(it->c_str());
  }

  if (output_dir)
  {
    entity_resolver.write_schemas(*output_dir);
  }

  if (error_handler.isOk())
  {
    std::cout << "##\n"
              << "## Writing grammar pool to " << output_fn << "\n"
              << "##" << std::endl;
    xercesc::BinFileOutputStream output(output_fn.c_str());
    grammar_pool.serializeGrammars(&output);

    if (serialize_fn)
    {
      std::ofstream output;
      output.exceptions(std::ios::badbit | std::ios::failbit);
      output.open(serialize_fn->c_str());
      boost::archive::text_oarchive oa(output);
      oa << entity_resolver;
    }

    std::cout << std::endl;
    bool changed = false;
    auto *xs_model = grammar_pool.getXSModel(changed);
    auto *namespaces = xs_model->getNamespaces();
    int len = namespaces->size();
    std::vector<std::string> ns_vect;
    for (int i = 0; i < len; i++)
    {
      const XMLCh *ns = namespaces->elementAt(i);
      ns_vect.push_back(to_string(ns));
    }

    std::sort(ns_vect.begin(), ns_vect.end());
    BOOST_FOREACH (const auto &ns, ns_vect)
    {
      std::cout << "CACHED NAMESPACE: '" << ns << "'" << std::endl;
    }

    return 0;
  }
  else
  {
    std::cerr << "XML Parsing failed" << std::endl;
    return 1;
  }
}

int main(int argc, char *argv[])
{
  xercesc::XMLPlatformUtils::Initialize();
  int retval = run(argc, argv);
  xercesc::XMLPlatformUtils::Terminate();
  return retval;
}
