#include "XmlParser.h"
#include "XmlUtils.h"
#include "XmlEntityResolver.h"
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/serialization/map.hpp>
#include <curl/curl.h>
#include <macgyver/TypeName.h>
#include <spine/Exception.h>
#include <xercesc/framework/LocalFileInputSource.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/XMLGrammarPoolImpl.hpp>
#include <xercesc/sax/ErrorHandler.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/util/BinFileInputStream.hpp>
#include <xercesc/util/Janitor.hpp>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = boost::filesystem;

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Xml
{
Parser::Parser(bool stop_on_error, xercesc::XMLGrammarPool *grammar_pool)
    : xercesc::XercesDOMParser(nullptr, xercesc::XMLPlatformUtils::fgMemoryManager, grammar_pool),
      error_handler(new XmlErrorHandler(stop_on_error))
{
  try
  {
    // Enable XML namespace handling
    setDoNamespaces(true);

    // Always do schema validation
    setValidationScheme(xercesc::XercesDOMParser::Val_Always);

    // Do not continue on fatal error
    setExitOnFirstFatalError(true);

    // setValidationConstraintFatal(true);

    setDoSchema(true);
    setLoadSchema(true);
    setHandleMultipleImports(true);

    // Do not parse schemas each time but keep already parsed schemas
    // from earlier parsed XML documents. This is much faster
    useCachedGrammarInParse(true);

    // Cannot save grammars to locked pool
    // cacheGrammarFromParse(true);

    setErrorHandler(error_handler.get());
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

Parser::~Parser() {}

boost::shared_ptr<xercesc::DOMDocument> Parser::parse_file(
    const std::string &file_name, Parser::root_element_cb_t root_element_cb)
{
  try
  {
    xercesc::Janitor<XMLCh> fn(xercesc::XMLString::transcode(file_name.c_str()));
    xercesc::LocalFileInputSource input(fn.get());
    return parse_input(input, root_element_cb);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<xercesc::DOMDocument> Parser::parse_string(
    const std::string &xml_data,
    const std::string &doc_id,
    Parser::root_element_cb_t root_element_cb)
{
  try
  {
    xercesc::MemBufInputSource input(
        (const XMLByte *)xml_data.c_str(), xml_data.length(), doc_id.c_str());
    return parse_input(input, root_element_cb);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<xercesc::DOMDocument> Parser::parse_input(
    xercesc::InputSource &input, Parser::root_element_cb_t root_element_cb)
{
  try
  {
    error_handler->resetErrors();

    this->root_element_cb = root_element_cb;
    this->parse(input);

    // Verify that there are no errors. Throw an exception otherwise
    error_handler->check_errors("XML parser failed");

    boost::shared_ptr<xercesc::DOMDocument> result(this->adoptDocument());
    return result;
  }
  catch (const XmlError&)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "XML error!")
      .addDetails(error_handler->get_messages());
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<std::string> Parser::get_messages() const
{
  try
  {
    return error_handler->get_messages();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Parser::startElement(const xercesc::XMLElementDecl &elemDecl,
                          const unsigned int uriId,
                          const XMLCh *const prefixName,
                          const xercesc::RefVectorOf<xercesc::XMLAttr> &attrList,
                          const XMLSize_t attrCount,
                          const bool isEmpty,
                          const bool isRoot)
{
  try
  {
    if (isRoot)
    {
      using SmartMet::Plugin::WFS::Xml::to_string;
      RootElemInfo root_info;
      const auto *qname = elemDecl.getElementName();
      root_info.prefix = to_string(prefixName);  // qname->getPrefix());
      root_info.nqname = to_string(qname->getLocalPart());
      const std::string name =
          (root_info.prefix == "" ? std::string("") : root_info.prefix + ":") + root_info.nqname;
      for (int i = 0; i < (int)attrCount; i++)
      {
        const xercesc::XMLAttr *attr = attrList.elementAt(i);
        const std::string attr_name = to_string(attr->getQName());
        const std::string attr_value = to_string(attr->getValue());
        root_info.attr_map[attr_name] = attr_value;
      }

      std::string xmlns_name = std::string("xmlns");
      if (root_info.prefix != "")
        xmlns_name = xmlns_name + ":" + root_info.prefix;

      // std::cout << "@@@ name='" << name << "'\n"
      //          << "@@@ local_name='" << root_info.nqname << "'\n"
      //          << "@@@ base_name='" << to_string(elemDecl.getBaseName()) << "'\n"
      //          << "@@@ prefix='" << root_info.prefix << "'\n"
      //          << "@@@ xmlns attr name='" << xmlns_name << "'" << std::endl;

      if (root_info.attr_map.count(xmlns_name) > 0 and root_element_cb)
      {
        root_info.ns_uri = root_info.attr_map[xmlns_name];
        root_element_cb(root_info);
      }
    }

    inherited::startElement(elemDecl, uriId, prefixName, attrList, attrCount, isEmpty, isRoot);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

ParserMT::ParserMT(const std::string &grammar_pool_file_name, bool stop_on_error)
    : grammar_pool_file_name(grammar_pool_file_name), stop_on_error(stop_on_error)
    , entity_resolver(new EntityResolver)
{
  try
  {
    grammar_pool.reset(new xercesc::XMLGrammarPoolImpl);
    try
    {
      xercesc::BinFileInputStream input(grammar_pool_file_name.c_str());
      grammar_pool->deserializeGrammars(&input);
      grammar_pool->lockPool();
    }
    catch (const xercesc::XMLException &err)
    {
      std::cerr << METHOD_NAME << ": failed to read Xerces-C grammar pool dump file '"
                << grammar_pool_file_name << "': C++ exception type is '"
                << Fmi::get_type_name(&err) << "', message '"
                << to_opt_string(err.getMessage()).first << "'" << std::endl;

      throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

ParserMT::~ParserMT() {}

Parser *ParserMT::get()
{
  try
  {
    Parser *parser = t_parser.get();
    if (parser == nullptr)
    {
      t_parser.reset(new Parser(stop_on_error, grammar_pool.get()));
      parser = t_parser.get();
    }

    if (entity_resolver and not parser->getXMLEntityResolver())
      parser->setXMLEntityResolver(entity_resolver.get());

    return parser;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void ParserMT::load_schema_cache(const std::string &file_name)
{
  try
  {
    std::ifstream input;
    input.exceptions(std::ios::failbit | std::ios::badbit);
    input.open(file_name.c_str());
    boost::archive::text_iarchive ia(input);
    ia >> *entity_resolver;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void ParserMT::enable_schema_download(const std::string& httpProxy, const std::string& noProxy)
{
  entity_resolver->init_schema_download(httpProxy, noProxy);
}

boost::shared_ptr<xercesc::DOMDocument> str2xmldom(const std::string &src,
                                                   const std::string &doc_id)
{
  try
  {
    xercesc::XercesDOMParser parser;
    XmlErrorHandler error_handler(true);
    parser.setDoNamespaces(true);
    parser.setValidationScheme(xercesc::XercesDOMParser::Val_Never);
    // parser.setExitOnFirstFatalError(true);
    parser.setErrorHandler(&error_handler);

    xercesc::MemBufInputSource input((const XMLByte *)src.c_str(), src.length(), doc_id.c_str());

    try
    {
      parser.parse(input);
    }
    catch (XmlError &error)
    {
      // Add messages to the exception object if an exception
      // has been thrown while parsing and messages are not yet added
      if (error.get_messages().empty())
      {
        error.add_messages(error_handler.get_messages());
      }
      throw SmartMet::Spine::Exception::Trace(BCP, "XML error!");
    }

    // Verify that parse suceeded and throw XmlError otherwise
    error_handler.check_errors("XML parser failed");

    boost::shared_ptr<xercesc::DOMDocument> result(parser.adoptDocument());
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Xml
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
