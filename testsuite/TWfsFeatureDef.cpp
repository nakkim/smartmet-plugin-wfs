#define BOOST_TEST_MODULE TWfsFeatureDef
#define BOOST_TEST_DYN_LINK 1
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <boost/optional/optional_io.hpp>
#include <spine/CRSRegistry.h>
#include "WfsFeatureDef.h"

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "WfsFeatureDef tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

using SmartMet::Spine::ConfigBase;
using namespace SmartMet::Plugin::WFS;
using SmartMet::Spine::CRSRegistry;

namespace
{
boost::shared_ptr<CRSRegistry> create_crs_registry()
{
  boost::shared_ptr<CRSRegistry> registry(new CRSRegistry);
  registry->register_epsg("WGS84", 4326, std::string("(urn:ogc:def:crs:|)EPSG::4326"));
  registry->register_epsg("UTM-ZONE-35", 4037, std::string("(urn:ogc:def:crs:|)EPSG::4037"));
  registry->register_epsg("WGS72-UTM-35", 32235, std::string("(urn:ogc:def:crs:|)EPSG::32235"));
  registry->set_attribute<std::string>(
      "WGS84", "projUri", "http://www.opengis.net/def/crs/EPSG/0/4326");
  registry->set_attribute<std::string>(
      "UTM-ZONE-35", "projUri", "http://www.opengis.net/def/crs/EPSG/0/4037");
  registry->set_attribute<std::string>(
      "WGS72-UTM-35", "projUri", "http://www.opengis.net/def/crs/EPSG/0/32235");
  return registry;
}
}

BOOST_AUTO_TEST_CASE(test_feature_def_1)
{
  using namespace libconfig;

  BOOST_TEST_MESSAGE("+ [Test parse WFS feature description]");

  auto crs_registry = create_crs_registry();

  boost::shared_ptr<Config> raw_config(new Config);
  try
  {
    auto& root = raw_config->getRoot();
    auto& def = root.add("def", Setting::TypeGroup);
    def.add("name", Setting::TypeString) = "testType";
    def.add("xmlType", Setting::TypeString) = "test:testType";
    def.add("xmlNamespace", Setting::TypeString) = "http://www.example.com/test";
    def.add("xmlNamespaceLoc", Setting::TypeString) = "http://www.example.com/test/test.xsd";
    auto& title = def.add("title", Setting::TypeGroup);
    title.add("eng", Setting::TypeString) = "An example";
    title.add("fin", Setting::TypeString) = "Esimerkki";
    auto& abstract = def.add("abstract", Setting::TypeGroup);
    abstract.add("eng", Setting::TypeString) = "Abstract of an example";
    abstract.add("fin", Setting::TypeString) = "Esimerkki";
    def.add("defaultCRS", Setting::TypeString) = "EPSG::4326";
    auto& crs2 = def.add("otherCRS", Setting::TypeArray);
    crs2.add(Setting::TypeString) = "EPSG::32235";
    crs2.add(Setting::TypeString) = "EPSG::4037";
  }
  catch (const libconfig::ConfigException& err)
  {
    std::cerr << "C++ exception: " << err.what() << std::endl;
    BOOST_REQUIRE(false);
  }

  boost::shared_ptr<ConfigBase> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new ConfigBase(raw_config, "Test")));

  // config->dump_config(std::cout, *raw_config);

  boost::shared_ptr<WfsFeatureDef> feature;
  try
  {
    Setting& setting = config->get_mandatory_config_param<Setting&>("def");
    feature.reset(new WfsFeatureDef(*crs_registry, "eng", config, setting));
  }
  catch (const std::runtime_error& err)
  {
    std::cerr << "C++ exception: " << err.what() << std::endl;
    BOOST_REQUIRE(false);
  }

  BOOST_CHECK_EQUAL(std::string("testType"), feature->get_name());
  BOOST_CHECK_EQUAL(std::string("test:testType"), feature->get_xml_type());
  BOOST_CHECK_EQUAL(std::string("http://www.example.com/test"), feature->get_xml_namespace());
  BOOST_CHECK_EQUAL(std::string("http://www.example.com/test/test.xsd"),
                    feature->get_xml_namespace_loc());
  BOOST_CHECK_EQUAL(std::string("http://www.opengis.net/def/crs/EPSG/0/4326"),
                    feature->get_default_crs());
  BOOST_CHECK_EQUAL(2, (int)feature->get_other_crs().size());
  BOOST_CHECK_EQUAL(std::string("http://www.opengis.net/def/crs/EPSG/0/32235"),
                    feature->get_other_crs().at(0));
  BOOST_CHECK_EQUAL(std::string("http://www.opengis.net/def/crs/EPSG/0/4037"),
                    feature->get_other_crs().at(1));
}
