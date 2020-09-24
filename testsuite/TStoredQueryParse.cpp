#define BOOST_TEST_MODULE TStoredQueryParse
#define BOOST_TEST_DYN_LINK 1
#include "ArrayParameterTemplate.h"
#include "ConfigBuild.h"
#include "ScalarParameterTemplate.h"
#include "StoredQuery.h"
#include "WfsConst.h"
#include "XmlEnvInit.h"
#include "XmlUtils.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/noncopyable.hpp>
#include <boost/test/unit_test.hpp>
#include <macgyver/StringConversion.h>
#include <spine/HTTP.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "StoredQuery parse tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

void dummy_proc(const char* text)
{
  (void)text;
}
namespace bw = SmartMet::Plugin::WFS;
namespace bwx = SmartMet::Plugin::WFS::Xml;
namespace pt = boost::posix_time;
using SmartMet::Spine::Value;
using Test::add_values;
using Test::TestConfig;
using SmartMet::Spine::HTTP::RequestMethod;
typedef boost::shared_ptr<bw::StoredQueryConfig> StoredQueryConfigP;
typedef boost::shared_ptr<bw::ArrayParameterTemplate> ArrayParameterTemplateP;
typedef boost::shared_ptr<bw::ScalarParameterTemplate> ScalarParameterTemplateP;

BOOST_AUTO_TEST_CASE(test_extract_kvp_stored_query_id)
{
  BOOST_TEST_MESSAGE("+ [Testing retrieving stored query ID from KVP request]");

  SmartMet::Spine::HTTP::Request request;
  request.setMethod(RequestMethod::GET);
  request.addParameter("request", "GetFeature");
  request.addParameter("a1", "12");
  request.addParameter("a2", "foo,bar");

  std::string id;
  BOOST_CHECK(not bw::StoredQuery::get_kvp_stored_query_id(request, &id));

  request.addParameter("storedquery_id", "foo::bar");
  BOOST_CHECK(bw::StoredQuery::get_kvp_stored_query_id(request, &id));
  BOOST_CHECK_EQUAL(id, std::string("foo::bar"));
}

BOOST_AUTO_TEST_CASE(test_extract_xml_stored_query_id)
{
  bwx::EnvInit ensure_init;
  BOOST_TEST_MESSAGE("+ [Testing retrieving stored query ID from XML request]");
  boost::shared_ptr<xercesc::DOMDocument> doc =
      bwx::create_dom_document(WFS_NAMESPACE_URI, "StoredQuery");
  xercesc::DOMElement* root = doc->getDocumentElement();
  bwx::set_attr(*bwx::append_child_text_element(*root, WFS_NAMESPACE_URI, "Parameter", "fo&o"),
                "name",
                "param_1");
  bwx::set_attr(*bwx::append_child_text_element(*root, WFS_NAMESPACE_URI, "Parameter", "b<ar"),
                "name",
                "param_2");

  std::string id;
  BOOST_CHECK_THROW(bw::StoredQuery::get_xml_stored_query_id(*root, &id),
                    Fmi::Exception);

  bwx::set_attr(*root, "id", "foo::bar");
  BOOST_CHECK(bw::StoredQuery::get_xml_stored_query_id(*root, &id));
  BOOST_CHECK_EQUAL(id, std::string("foo::bar"));
}

namespace
{
void output_exception_info(const std::exception& err)
{
  std::cerr << "C++ exception of type " << Fmi::get_type_name(&err)
            << " catched: " << err.what() << std::endl;
}
}  // namespace

#define LOG(x)                      \
  try                               \
  {                                 \
    x;                              \
  }                                 \
  catch (const std::exception& err) \
  {                                 \
    output_exception_info(err);     \
    throw;                          \
  }

namespace
{
class TemporaryConfigFile : public boost::noncopyable
{
  std::string name;

 public:
  TemporaryConfigFile(libconfig::Config& config)
  {
    char c_name[80];
    strcpy(c_name, "/tmp/tmp.XXXXXX");
    int handle = mkstemp(c_name);
    name = c_name;
    if (handle < 0)
      throw std::runtime_error(std::string("Failed to create temporary file: ") + strerror(errno));
    FILE* output = fdopen(handle, "w");
    if (!output)
      throw std::runtime_error(std::string("fdopen(): ") + strerror(errno));
    config.write(output);
    fclose(output);
    // close(handle);  /* Not needed as fclose() does it according to fdopen() manual */
  };

  virtual ~TemporaryConfigFile() { unlink(name.c_str()); }
  inline const std::string& get_name() const { return name; }
};

boost::shared_ptr<libconfig::Config> create_initial_test_config()
{
  boost::shared_ptr<libconfig::Config> config(new libconfig::Config);
  libconfig::Setting& root = config->getRoot();
  BOOST_REQUIRE_NO_THROW(root.add("id", libconfig::Setting::TypeString) = "foo::bar");
  BOOST_REQUIRE_NO_THROW(root.add("constructor_name", libconfig::Setting::TypeString) = "foo::bar");
  BOOST_REQUIRE_NO_THROW(
      root.add("title", libconfig::Setting::TypeGroup).add("eng", libconfig::Setting::TypeString) =
          "title");
  BOOST_REQUIRE_NO_THROW(root.add("abstract", libconfig::Setting::TypeGroup)
                             .add("eng", libconfig::Setting::TypeString) = "abstract");
  BOOST_REQUIRE_NO_THROW(root.add("template", libconfig::Setting::TypeString) = "foo::bar.c2t");
  BOOST_REQUIRE_NO_THROW(root.add("returnTypeNames", libconfig::Setting::TypeArray)
                             .add(libconfig::Setting::TypeString) = "foo:result");
  BOOST_REQUIRE_NO_THROW(root.add("parameters", libconfig::Setting::TypeList));
  return config;
}

void add_param(libconfig::Config& config,
               const std::string& name,
               const std::string& xml_type,
               const std::string& type,
               int min_occurs = 0,
               int max_occurs = 1)
{
  libconfig::Setting& params = config.lookup("parameters");
  libconfig::Setting& p = params.add(libconfig::Setting::TypeGroup);
  BOOST_REQUIRE_NO_THROW(p.add("name", libconfig::Setting::TypeString) = name);
  BOOST_REQUIRE_NO_THROW(
      p.add("title", libconfig::Setting::TypeGroup).add("eng", libconfig::Setting::TypeString) =
          "title");
  BOOST_REQUIRE_NO_THROW(
      p.add("abstract", libconfig::Setting::TypeGroup).add("eng", libconfig::Setting::TypeString) =
          "abstract");
  BOOST_REQUIRE_NO_THROW(p.add("xmlType", libconfig::Setting::TypeString) = xml_type);
  BOOST_REQUIRE_NO_THROW(p.add("type", libconfig::Setting::TypeString) = type);
  if (min_occurs != 0)
    BOOST_REQUIRE_NO_THROW(p.add("minOccurs", libconfig::Setting::TypeInt) = min_occurs);
  if (max_occurs != 1)
    BOOST_REQUIRE_NO_THROW(p.add("maxOccurs", libconfig::Setting::TypeInt) = max_occurs);
}
}  // namespace

namespace
{
/**
 *   Simple wrapper to expose protected constructor for testing purposes
 */
class StoredQueryWrapper : public bw::StoredQuery
{
 public:
  StoredQueryWrapper() : StoredQuery() {}
  virtual ~StoredQueryWrapper() {}
};

template <typename T>
std::ostream& operator<<(std::ostream& stream, const std::multimap<std::string, T>& m)
{
  BOOST_FOREACH (const auto& item, m)
  {
    std::cout << "item[" << item.first << "] = '" << item.second << "'\n";
  }
  return stream;
}
}  // namespace

BOOST_AUTO_TEST_CASE(test_kvp_missing_mandatory_scalar_parameter)
{
  BOOST_TEST_MESSAGE("+ [Testing KVP request: missing mandatory scalar parameter]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "string").min_occurs(1);
  raw_config->add_scalar("s1", "${p1}");

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q1;
  SmartMet::Spine::HTTP::Request request1;
  request1.setMethod(RequestMethod::GET);
  request1.addParameter("request", "GetFeature");
  request1.addParameter("storedquery_id", "foo:bar");
  BOOST_CHECK_THROW(bw::StoredQuery::extract_kvp_parameters(request1, *config, Q1),
                    Fmi::Exception);
}

BOOST_AUTO_TEST_CASE(test_kvp_single_scalar_parameter)
{
  BOOST_TEST_MESSAGE("+ [Testing KVP request: read scalar parameter]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "string").min_occurs(1);
  raw_config->add_scalar("s1", "${p1}");

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q2;
  SmartMet::Spine::HTTP::Request request2;
  request2.setMethod(RequestMethod::GET);
  request2.addParameter("request", "GetFeature");
  request2.addParameter("storedquery_id", "foo:bar");
  request2.addParameter("p1", "foobar");
  BOOST_REQUIRE_NO_THROW(LOG(bw::StoredQuery::extract_kvp_parameters(request2, *config, Q2)));
  const auto& P1 = Q2.get_param_map();
  BOOST_CHECK_EQUAL(1, (int)P1.size());
  const auto v = Q2.get_param_values("p1");
  BOOST_REQUIRE_EQUAL(1, (int)v.size());
  BOOST_REQUIRE(v.at(0).type() == typeid(std::string));
  BOOST_CHECK_EQUAL(v.at(0).get_string(), std::string("foobar"));
}

BOOST_AUTO_TEST_CASE(test_kvp_repeatable_scalar_parameter_1)
{
  BOOST_TEST_MESSAGE("+ [Testing KVP request: repeatable scalar parameter (ommited)]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "string").min_occurs(0).max_occurs(99);
  raw_config->add_array("s1", "${p1}");

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q2;
  SmartMet::Spine::HTTP::Request request2;
  request2.setMethod(RequestMethod::GET);
  request2.addParameter("request", "GetFeature");
  request2.addParameter("storedquery_id", "foo:bar");
  BOOST_REQUIRE_NO_THROW(LOG(bw::StoredQuery::extract_kvp_parameters(request2, *config, Q2)));
  const auto& P1 = Q2.get_param_map();
  BOOST_CHECK_EQUAL(0, (int)P1.size());

  std::vector<std::string> data;
  ArrayParameterTemplateP param;
  BOOST_REQUIRE_NO_THROW(param.reset(new bw::ArrayParameterTemplate(*config, "s1", 0, 99)));
  BOOST_REQUIRE_NO_THROW(LOG(data = param->get_string_array(P1)));
  BOOST_REQUIRE_EQUAL(0, (int)data.size());
}

BOOST_AUTO_TEST_CASE(test_kvp_repeatable_scalar_parameter_2)
{
  BOOST_TEST_MESSAGE("+ [Testing KVP request: repeatable scalar parameter (used once)]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "string").min_occurs(0).max_occurs(99);
  raw_config->add_array("s1", "${p1}");

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q2;
  SmartMet::Spine::HTTP::Request request2;
  request2.setMethod(RequestMethod::GET);
  request2.addParameter("request", "GetFeature");
  request2.addParameter("storedquery_id", "foo:bar");
  request2.addParameter("p1", "Helsinki,Suomi");
  BOOST_REQUIRE_NO_THROW(LOG(bw::StoredQuery::extract_kvp_parameters(request2, *config, Q2)));
  const auto& P1 = Q2.get_param_map();
  BOOST_CHECK_EQUAL(1, (int)P1.size());
  const auto v = Q2.get_param_values("p1");
  BOOST_REQUIRE_EQUAL(1, (int)v.size());
  BOOST_REQUIRE(v.at(0).type() == typeid(std::string));
  BOOST_CHECK_EQUAL(v.at(0).get_string(), std::string("Helsinki,Suomi"));

  std::vector<std::string> data;
  ArrayParameterTemplateP param;
  BOOST_REQUIRE_NO_THROW(param.reset(new bw::ArrayParameterTemplate(*config, "s1", 0, 99)));
  BOOST_REQUIRE_NO_THROW(data = param->get_string_array(P1));
  BOOST_REQUIRE_EQUAL(1, (int)data.size());
  BOOST_CHECK_EQUAL(std::string("Helsinki,Suomi"), data.at(0));
}

BOOST_AUTO_TEST_CASE(test_kvp_repeatable_scalar_parameter_3)
{
  BOOST_TEST_MESSAGE("+ [Testing KVP request: repeatable scalar parameter (used twice)]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "string").min_occurs(0).max_occurs(99);
  raw_config->add_array("s1", "${p1}");

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q2;
  SmartMet::Spine::HTTP::Request request2;
  request2.setMethod(RequestMethod::GET);
  request2.addParameter("request", "GetFeature");
  request2.addParameter("storedquery_id", "foo:bar");
  request2.addParameter("p1", "Helsinki,Suomi");
  request2.addParameter("p1", "Stockholm,Sweden");
  BOOST_REQUIRE_NO_THROW(LOG(bw::StoredQuery::extract_kvp_parameters(request2, *config, Q2)));
  const auto& P1 = Q2.get_param_map();
  BOOST_CHECK_EQUAL(2, (int)P1.size());
  const auto v = Q2.get_param_values("p1");
  BOOST_REQUIRE_EQUAL(2, (int)v.size());
  BOOST_REQUIRE(v.at(0).type() == typeid(std::string));
  BOOST_REQUIRE(v.at(1).type() == typeid(std::string));
  BOOST_CHECK_EQUAL(v.at(0).get_string(), std::string("Helsinki,Suomi"));
  BOOST_CHECK_EQUAL(v.at(1).get_string(), std::string("Stockholm,Sweden"));

  std::vector<std::string> data;
  ArrayParameterTemplateP param;
  BOOST_REQUIRE_NO_THROW(param.reset(new bw::ArrayParameterTemplate(*config, "s1", 0, 99)));
  BOOST_REQUIRE_NO_THROW(data = param->get_string_array(P1));
  BOOST_REQUIRE_EQUAL(2, (int)data.size());
  BOOST_CHECK_EQUAL(std::string("Helsinki,Suomi"), data.at(0));
  BOOST_CHECK_EQUAL(std::string("Stockholm,Sweden"), data.at(1));
}

BOOST_AUTO_TEST_CASE(test_kvp_repeatable_scalar_parameter_4)
{
  BOOST_TEST_MESSAGE(
      "+ [Testing KVP request: repeatable scalar parameter (use count == maxOccurs > 1)]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "string").min_occurs(0).max_occurs(99);
  raw_config->add_scalar("s1", "${p1}");

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q2;
  SmartMet::Spine::HTTP::Request request2;
  request2.setMethod(RequestMethod::GET);
  request2.addParameter("request", "GetFeature");
  request2.addParameter("storedquery_id", "foo:bar");
  for (int i = 0; i < 99; i++)
    request2.addParameter("p1", Fmi::to_string(i + 1));
  BOOST_REQUIRE_NO_THROW(LOG(bw::StoredQuery::extract_kvp_parameters(request2, *config, Q2)));
  const auto& P1 = Q2.get_param_map();
  BOOST_CHECK_EQUAL(99, (int)P1.size());
}

BOOST_AUTO_TEST_CASE(test_kvp_repeatable_scalar_parameter_5)
{
  BOOST_TEST_MESSAGE("+ [Testing KVP request: repeatable scalar parameter (used too many times)]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "string").min_occurs(0).max_occurs(99);
  raw_config->add_scalar("s1", "${p1}");

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q2;
  SmartMet::Spine::HTTP::Request request2;
  request2.setMethod(RequestMethod::GET);
  request2.addParameter("request", "GetFeature");
  request2.addParameter("storedquery_id", "foo:bar");
  for (int i = 0; i < 100; i++)
    request2.addParameter("p1", Fmi::to_string(i + 1));
  BOOST_REQUIRE_THROW(bw::StoredQuery::extract_kvp_parameters(request2, *config, Q2),
                      Fmi::Exception);
}

BOOST_AUTO_TEST_CASE(test_kvp_single_scalar_parameter_too_many_occurrences)
{
  BOOST_TEST_MESSAGE("+ [Testing KVP request: scalar parameter provided too many times]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "string").min_occurs(1).max_occurs(1);
  raw_config->add_scalar("s1", "${p1}");

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q1;
  SmartMet::Spine::HTTP::Request request;
  request.setMethod(RequestMethod::GET);
  request.addParameter("request", "GetFeature");
  request.addParameter("storedquery_id", "foo:bar");
  request.addParameter("p1", "foobar");
  request.addParameter("p1", "foobarbaz");

  try
  {
    bw::StoredQuery::extract_kvp_parameters(request, *config, Q1);
    std::cout << Q1.get_param_map().get_map() << "\n";
    BOOST_REQUIRE(false);
  }
  catch (const std::exception& err)
  {
    // std::cout << "Got C++ exception of type '"
    //          << Fmi::current_exception_type()
    //          << "': message='" << err.what() << "'"
    //          << std::endl;
    BOOST_REQUIRE_THROW(throw, Fmi::Exception);
  };
}

BOOST_AUTO_TEST_CASE(test_kvp_mandatory_array_parameter_of_fixed_length_1)
{
  BOOST_TEST_MESSAGE(
      "+ [Testing KVP request: mandatory array parameter of fixed length (comma separated "
      "values)]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "double[2]").min_occurs(1).max_occurs(1);
  std::array<std::string, 1> p001 = {{"${p1}"}};
  raw_config->add_array<1>("s1", p001);

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q2;
  SmartMet::Spine::HTTP::Request request2;
  request2.setMethod(RequestMethod::GET);
  request2.addParameter("request", "GetFeature");
  request2.addParameter("storedquery_id", "foo:bar");
  request2.addParameter("p1", "12.5,6.25");
  BOOST_REQUIRE_NO_THROW(LOG(bw::StoredQuery::extract_kvp_parameters(request2, *config, Q2)));
  const auto& P1 = Q2.get_param_map();
  BOOST_CHECK_EQUAL(2, (int)P1.size());
  const auto v = Q2.get_param_values("p1");
  BOOST_REQUIRE_EQUAL(2, (int)v.size());
  BOOST_REQUIRE(v.at(0).type() == typeid(double));
  BOOST_REQUIRE(v.at(1).type() == typeid(double));
  BOOST_CHECK_CLOSE(v.at(0).get_double(), 12.5, 1e-10);
  BOOST_CHECK_CLOSE(v.at(1).get_double(), 6.25, 1e-10);
}

BOOST_AUTO_TEST_CASE(test_kvp_repeatable_array_parameter_of_fixed_length_ommited)
{
  BOOST_TEST_MESSAGE(
      "+ [Testing KVP request: repeatable array parameter of fixed length (ommited)]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "double[2]").min_occurs(0).max_occurs(999);
  std::array<std::string, 1> p001 = {{"${p1}"}};
  raw_config->add_array<1>("s1", p001);

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q2;
  SmartMet::Spine::HTTP::Request request2;
  request2.setMethod(RequestMethod::GET);
  request2.addParameter("request", "GetFeature");
  request2.addParameter("storedquery_id", "foo:bar");
  BOOST_REQUIRE_NO_THROW(LOG(bw::StoredQuery::extract_kvp_parameters(request2, *config, Q2)));
  const auto& P1 = Q2.get_param_map();
  BOOST_CHECK_EQUAL(0, (int)P1.size());

  std::vector<double> data;
  ArrayParameterTemplateP param;
  BOOST_REQUIRE_NO_THROW(param.reset(new bw::ArrayParameterTemplate(*config, "s1", 0, 99)));
  BOOST_REQUIRE_NO_THROW(LOG(data = param->get_double_array(P1)));
  BOOST_REQUIRE_EQUAL(0, (int)data.size());
}

BOOST_AUTO_TEST_CASE(test_kvp_repeatable_array_parameter_of_fixed_length_once)
{
  BOOST_TEST_MESSAGE(
      "+ [Testing KVP request: repeatable array parameter of fixed length (used once)]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "double[2]").min_occurs(0).max_occurs(999);
  std::array<std::string, 1> p001 = {{"${p1}"}};
  raw_config->add_array<1>("s1", p001);

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q2;
  SmartMet::Spine::HTTP::Request request2;
  request2.setMethod(RequestMethod::GET);
  request2.addParameter("request", "GetFeature");
  request2.addParameter("storedquery_id", "foo:bar");
  request2.addParameter("p1", "12.5,25.0");
  BOOST_REQUIRE_NO_THROW(LOG(bw::StoredQuery::extract_kvp_parameters(request2, *config, Q2)));
  const auto& P1 = Q2.get_param_map();
  BOOST_CHECK_EQUAL(2, (int)P1.size());
  const auto v = Q2.get_param_values("p1");
  BOOST_REQUIRE_EQUAL(2, (int)v.size());
  BOOST_REQUIRE(v.at(0).type() == typeid(double));
  BOOST_REQUIRE(v.at(1).type() == typeid(double));
  BOOST_CHECK_EQUAL(v.at(0).get_double(), 12.5);
  BOOST_CHECK_EQUAL(v.at(1).get_double(), 25.0);

  std::vector<double> data;
  ArrayParameterTemplateP param;
  BOOST_REQUIRE_NO_THROW(param.reset(new bw::ArrayParameterTemplate(*config, "s1", 0, 99)));
  BOOST_REQUIRE_NO_THROW(data = param->get_double_array(P1));
  BOOST_REQUIRE_EQUAL(2, (int)data.size());
}

BOOST_AUTO_TEST_CASE(test_kvp_repeatable_array_parameter_of_fixed_length_twice)
{
  BOOST_TEST_MESSAGE(
      "+ [Testing KVP request: repeatable array parameter of fixed length (used twice)]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "double[2]").min_occurs(0).max_occurs(999);
  std::array<std::string, 1> p001 = {{"${p1}"}};
  raw_config->add_array<1>("s1", p001);

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q2;
  SmartMet::Spine::HTTP::Request request2;
  request2.setMethod(RequestMethod::GET);
  request2.addParameter("request", "GetFeature");
  request2.addParameter("storedquery_id", "foo:bar");
  request2.addParameter("p1", "12.5,25.0");
  request2.addParameter("p1", "1.5,2.5");
  BOOST_REQUIRE_NO_THROW(LOG(bw::StoredQuery::extract_kvp_parameters(request2, *config, Q2)));
  const auto& P1 = Q2.get_param_map();
  BOOST_REQUIRE_EQUAL(4, (int)P1.size());
  const auto v = Q2.get_param_values("p1");
  BOOST_REQUIRE_EQUAL(4, (int)v.size());
  BOOST_REQUIRE(v.at(0).type() == typeid(double));
  BOOST_REQUIRE(v.at(1).type() == typeid(double));
  BOOST_REQUIRE(v.at(2).type() == typeid(double));
  BOOST_REQUIRE(v.at(3).type() == typeid(double));
  BOOST_CHECK_EQUAL(v.at(0).get_double(), 12.5);
  BOOST_CHECK_EQUAL(v.at(1).get_double(), 25.0);
  BOOST_CHECK_EQUAL(v.at(2).get_double(), 1.5);
  BOOST_CHECK_EQUAL(v.at(3).get_double(), 2.5);

  std::vector<double> data;
  ArrayParameterTemplateP param;
  BOOST_REQUIRE_NO_THROW(param.reset(new bw::ArrayParameterTemplate(*config, "s1", 0, 99)));
  BOOST_REQUIRE_NO_THROW(data = param->get_double_array(P1));
  BOOST_REQUIRE_EQUAL(4, (int)data.size());
}

BOOST_AUTO_TEST_CASE(test_kvp_repeatable_array_parameter_of_fixed_length_mixed_up_1)
{
  BOOST_TEST_MESSAGE(
      "+ [Testing KVP request: repeatable array parameter of fixed length (invalid groups: test "
      "1)]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "double[2]").min_occurs(0).max_occurs(999);
  std::array<std::string, 1> p001 = {{"${p1}"}};
  // raw_config->add_array<1>("s1", p001);

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q2;
  SmartMet::Spine::HTTP::Request request2;
  request2.setMethod(RequestMethod::GET);
  request2.addParameter("request", "GetFeature");
  request2.addParameter("storedquery_id", "foo:bar");
  request2.addParameter("p1", "12.5,25.0,1.5");
  request2.addParameter("p1", "2.5");
  BOOST_REQUIRE_THROW(bw::StoredQuery::extract_kvp_parameters(request2, *config, Q2),
                      Fmi::Exception);
}

BOOST_AUTO_TEST_CASE(test_kvp_repeatable_array_parameter_of_fixed_length_mixed_up_2)
{
  BOOST_TEST_MESSAGE(
      "+ [Testing KVP request: repeatable array parameter of fixed length (invalid groups: test "
      "2)]");

  auto raw_config = Test::TestConfig::create("foo::bar");
  raw_config->add_param("p1", "xxx", "double[4]").min_occurs(0).max_occurs(999);
  std::array<std::string, 1> p001 = {{"${p1}"}};
  // raw_config->add_array<1>("s1", p001);

  StoredQueryConfigP config;
  BOOST_REQUIRE_NO_THROW(config.reset(new bw::StoredQueryConfig(raw_config, nullptr)));

  StoredQueryWrapper Q2;
  SmartMet::Spine::HTTP::Request request2;
  request2.setMethod(RequestMethod::GET);
  request2.addParameter("request", "GetFeature");
  request2.addParameter("storedquery_id", "foo:bar");
  request2.addParameter("p1", "1.0,2.0,3.0");
  request2.addParameter("p1", "4.0");
  BOOST_REQUIRE_THROW(bw::StoredQuery::extract_kvp_parameters(request2, *config, Q2),
                      Fmi::Exception);
}

BOOST_AUTO_TEST_CASE(test_parse_sample_kvp_stored_query_id)
{
  BOOST_TEST_MESSAGE("+ [Testing parsing sample KVP format stored query]");

  SmartMet::Spine::HTTP::Request request;
  request.setMethod(RequestMethod::GET);
  request.addParameter("request", "GetFeature");
  request.addParameter("time", "2012-11-15T12:36:45Z");
  request.addParameter("name", "foo:bar");

  auto config = create_initial_test_config();
  add_param(*config, "time", "xsi:dateTime", "time", 0, 999);
  add_param(*config, "name", "xsi:string", "string");
  add_param(*config, "num", "gml:doubleList", "double[0..999]", 0, 999);

  std::vector<Value> v;
  boost::shared_ptr<bw::StoredQueryConfig> sqc;
  StoredQueryWrapper Q1, Q2, Q3;

  BOOST_TEST_MESSAGE("    + [part 1: 2 scalar values of different types]");
  TemporaryConfigFile cfg_file_1(*config);
  BOOST_REQUIRE_NO_THROW(sqc.reset(new bw::StoredQueryConfig(cfg_file_1.get_name(), nullptr)));
  BOOST_REQUIRE_NO_THROW(bw::StoredQuery::extract_kvp_parameters(request, *sqc, Q1));
  const auto& P1 = Q1.get_param_map();
  BOOST_REQUIRE_EQUAL((int)P1.size(), 2);
  BOOST_REQUIRE_EQUAL((int)P1.count("time"), 1);
  BOOST_REQUIRE_EQUAL((int)P1.count("name"), 1);
  v = Q1.get_param_values("time");
  BOOST_REQUIRE_EQUAL(1, (int)v.size());
  BOOST_REQUIRE(v.at(0).type() == typeid(boost::posix_time::ptime));
  BOOST_CHECK_EQUAL(Fmi::to_iso_extended_string(v.at(0).get_ptime()),
                    std::string("2012-11-15T12:36:45"));
  v = Q1.get_param_values("name");
  BOOST_REQUIRE_EQUAL(1, (int)v.size());
  BOOST_REQUIRE(v.at(0).type() == typeid(std::string));
  BOOST_CHECK_EQUAL(v.at(0).get_string(), std::string("foo:bar"));

  request.addParameter("time", "2012-11-15T13:40:16Z");
  BOOST_TEST_MESSAGE("    + [part 2: scalar value and one repeating scalar parameter]");
  BOOST_REQUIRE_NO_THROW(bw::StoredQuery::extract_kvp_parameters(request, *sqc, Q2));
  const auto& P2 = Q2.get_param_map();
  BOOST_REQUIRE_EQUAL((int)Q2.get_param_map().size(), 3);
  BOOST_REQUIRE_EQUAL((int)P2.count("name"), 1);
  BOOST_REQUIRE_EQUAL((int)P2.count("time"), 2);
  v = Q2.get_param_values("time");
  BOOST_REQUIRE_EQUAL(2, (int)v.size());
  BOOST_REQUIRE(v.at(0).type() == typeid(boost::posix_time::ptime));
  BOOST_CHECK_EQUAL(Fmi::to_iso_extended_string(v.at(0).get_ptime()),
                    std::string("2012-11-15T12:36:45"));
  BOOST_REQUIRE(v.at(1).type() == typeid(boost::posix_time::ptime));
  BOOST_CHECK_EQUAL(Fmi::to_iso_extended_string(v.at(1).get_ptime()),
                    std::string("2012-11-15T13:40:16"));
  v = Q2.get_param_values("name");
  BOOST_REQUIRE_EQUAL(1, (int)v.size());
  BOOST_REQUIRE(v.at(0).type() == typeid(std::string));
  BOOST_CHECK_EQUAL(v.at(0).get_string(), std::string("foo:bar"));

  request.addParameter("num", "1.1,1.21");
  request.addParameter("num", "1.331");
  request.addParameter("num", "1.4641");
  BOOST_TEST_MESSAGE("    + [part 3: double array parameter added]");
  BOOST_REQUIRE_NO_THROW(bw::StoredQuery::extract_kvp_parameters(request, *sqc, Q3));
  const auto& P3 = Q3.get_param_map();
  BOOST_REQUIRE_EQUAL((int)P3.size(), 7);
  v = Q3.get_param_values("name");
  BOOST_REQUIRE_EQUAL(1, (int)v.size());
  BOOST_REQUIRE(v.at(0).type() == typeid(std::string));
  BOOST_CHECK_EQUAL(v.at(0).get_string(), std::string("foo:bar"));
  v = Q3.get_param_values("time");
  BOOST_REQUIRE_EQUAL(2, (int)v.size());
  BOOST_REQUIRE(v.at(0).type() == typeid(boost::posix_time::ptime));
  BOOST_CHECK_EQUAL(Fmi::to_iso_extended_string(v.at(0).get_ptime()),
                    std::string("2012-11-15T12:36:45"));
  BOOST_REQUIRE(v.at(1).type() == typeid(boost::posix_time::ptime));
  BOOST_CHECK_EQUAL(Fmi::to_iso_extended_string(v.at(1).get_ptime()),
                    std::string("2012-11-15T13:40:16"));
  v = Q3.get_param_values("num");
  BOOST_REQUIRE_EQUAL(4, (int)v.size());
  BOOST_REQUIRE(v.at(0).type() == typeid(double));
  BOOST_CHECK_CLOSE(v.at(0).get_double(), 1.1, 1e-12);
  BOOST_REQUIRE(v.at(1).type() == typeid(double));
  BOOST_CHECK_CLOSE(v.at(1).get_double(), 1.21, 1e-12);
  BOOST_REQUIRE(v.at(2).type() == typeid(double));
  BOOST_CHECK_CLOSE(v.at(2).get_double(), 1.331, 1e-12);
  BOOST_REQUIRE(v.at(3).type() == typeid(double));
  BOOST_CHECK_CLOSE(v.at(3).get_double(), 1.4641, 1e-12);
}

BOOST_AUTO_TEST_CASE(test_parse_sample_xml_stored_query_id)
{
  BOOST_TEST_MESSAGE("+ [Testing parsing sample XML format stored query request]");

  bwx::EnvInit ensure_init;
  boost::shared_ptr<xercesc::DOMDocument> doc =
      bwx::create_dom_document(WFS_NAMESPACE_URI, "StoredQuery");
  xercesc::DOMElement* root = doc->getDocumentElement();
  bwx::set_attr(*bwx::append_child_text_element(
                    *root, WFS_NAMESPACE_URI, "Parameter", "2012-11-15T12:36:45Z"),
                "name",
                "time");
  bwx::set_attr(*bwx::append_child_text_element(*root, WFS_NAMESPACE_URI, "Parameter", "foo:bar"),
                "name",
                "name");

  auto config = create_initial_test_config();
  add_param(*config, "time", "xsi:dateTime", "time", 0, 999);
  add_param(*config, "name", "xsi:string", "string");
  add_param(*config, "num", "gml:doubleList", "double[0..999]", 0, 999);

  boost::shared_ptr<bw::StoredQueryConfig> sqc;
  StoredQueryWrapper Q1, Q2, Q3;

  BOOST_TEST_MESSAGE("    + [part 1: 2 scalar values of different types]");
  TemporaryConfigFile cfg_file_1(*config);
  BOOST_REQUIRE_NO_THROW(sqc.reset(new bw::StoredQueryConfig(cfg_file_1.get_name(), nullptr)));
  BOOST_REQUIRE_NO_THROW(bw::StoredQuery::extract_xml_parameters(*root, *sqc, Q1));
  BOOST_REQUIRE_EQUAL(1, (int)Q1.get_param_values("time").size());
  BOOST_REQUIRE_EQUAL(1, (int)Q1.get_param_values("name").size());
  BOOST_REQUIRE(Q1.get_param_values("time").at(0).type() == typeid(boost::posix_time::ptime));
  BOOST_CHECK_EQUAL(Fmi::to_iso_extended_string(Q1.get_param_values("time").at(0).get_ptime()),
                    std::string("2012-11-15T12:36:45"));
  BOOST_REQUIRE(Q1.get_param_values("name").at(0).type() == typeid(std::string));
  BOOST_CHECK_EQUAL(Q1.get_param_values("name").at(0).get_string(), std::string("foo:bar"));

  bwx::set_attr(*bwx::append_child_text_element(
                    *root, WFS_NAMESPACE_URI, "Parameter", "2012-11-15T13:40:16Z"),
                "name",
                "time");
  BOOST_TEST_MESSAGE("    + [part 2: scalar value and one repeating scalar parameter]");
  BOOST_REQUIRE_NO_THROW(bw::StoredQuery::extract_xml_parameters(*root, *sqc, Q2));
  BOOST_REQUIRE_EQUAL(2, (int)Q2.get_param_values("time").size());
  BOOST_REQUIRE_EQUAL(1, (int)Q2.get_param_values("name").size());
  BOOST_REQUIRE(Q2.get_param_values("time").at(0).type() == typeid(boost::posix_time::ptime));
  BOOST_CHECK_EQUAL(Fmi::to_iso_extended_string(Q2.get_param_values("time").at(0).get_ptime()),
                    std::string("2012-11-15T12:36:45"));
  BOOST_REQUIRE(Q2.get_param_values("time").at(1).type() == typeid(boost::posix_time::ptime));
  BOOST_CHECK_EQUAL(Fmi::to_iso_extended_string(Q2.get_param_values("time").at(1).get_ptime()),
                    std::string("2012-11-15T13:40:16"));
  BOOST_REQUIRE(Q2.get_param_values("name").at(0).type() == typeid(std::string));
  BOOST_CHECK_EQUAL(Q2.get_param_values("name").at(0).get_string(), std::string("foo:bar"));

  bwx::set_attr(*bwx::append_child_text_element(*root, WFS_NAMESPACE_URI, "Parameter", "1.1 1.21 "),
                "name",
                "num");
  bwx::set_attr(*bwx::append_child_text_element(*root, WFS_NAMESPACE_URI, "Parameter", " 1.4641 "),
                "name",
                "num");
  bwx::set_attr(*bwx::append_child_text_element(*root, WFS_NAMESPACE_URI, "Parameter", " 1.331 "),
                "name",
                "num");

  BOOST_TEST_MESSAGE("    + [part 3: double array parameter added]");
  BOOST_REQUIRE_NO_THROW(bw::StoredQuery::extract_xml_parameters(*root, *sqc, Q3));
  BOOST_REQUIRE_EQUAL(2, Q3.get_param_values("time").size());
  BOOST_REQUIRE_EQUAL((int)Q3.get_param_values("name").size(), 1);
  BOOST_REQUIRE_EQUAL((int)Q3.get_param_values("num").size(), 4);
  BOOST_REQUIRE(Q3.get_param_values("time").at(0).type() == typeid(boost::posix_time::ptime));
  BOOST_CHECK_EQUAL(Fmi::to_iso_extended_string(Q3.get_param_values("time").at(0).get_ptime()),
                    std::string("2012-11-15T12:36:45"));
  BOOST_REQUIRE(Q3.get_param_values("time").at(1).type() == typeid(boost::posix_time::ptime));
  BOOST_CHECK_EQUAL(Fmi::to_iso_extended_string(Q3.get_param_values("time").at(1).get_ptime()),
                    std::string("2012-11-15T13:40:16"));
  BOOST_REQUIRE(Q3.get_param_values("name").at(0).type() == typeid(std::string));
  BOOST_CHECK_EQUAL(Q3.get_param_values("name").at(0).get_string(), std::string("foo:bar"));
  BOOST_REQUIRE(Q3.get_param_values("num").at(0).type() == typeid(double));
  BOOST_CHECK_CLOSE(Q3.get_param_values("num").at(0).get_double(), 1.1, 1e-12);
  BOOST_REQUIRE(Q3.get_param_values("num").at(1).type() == typeid(double));
  BOOST_CHECK_CLOSE(Q3.get_param_values("num").at(1).get_double(), 1.21, 1e-12);
  BOOST_REQUIRE(Q3.get_param_values("num").at(2).type() == typeid(double));
  BOOST_CHECK_CLOSE(Q3.get_param_values("num").at(2).get_double(), 1.4641, 1e-12);
  BOOST_REQUIRE(Q3.get_param_values("num").at(3).type() == typeid(double));
  BOOST_CHECK_CLOSE(Q3.get_param_values("num").at(3).get_double(), 1.331, 1e-12);
}
