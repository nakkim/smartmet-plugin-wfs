#include <iostream>
#include <boost/test/included/unit_test.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include "DataSetIndex.h"

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "DataSetIndex tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

namespace
{

const std::string test_dir = "/home/pavenis/smartmet/plugins/smartmet-plugin-wfs";

boost::shared_ptr<libconfig::Config> create_config()
{
  using libconfig::Setting;
  boost::shared_ptr<libconfig::Config> config(new libconfig::Config);
  auto& root = config->getRoot();
  root.add("name", Setting::TypeString) = "Test1";
  root.add("dir", Setting::TypeString) = test_dir;
  root.add("file_regex", Setting::TypeString) = ".*";
  root.add("origin_time_extract", Setting::TypeString) = "^\\d{12}_";
  auto& transl = root.add("origin_time_translate", Setting::TypeArray);
  transl.add(Setting::TypeString) = "^(\\d{4})(\\d{2})(\\d{2})(\\d{2})(\\d{2})_";
  transl.add(Setting::TypeString) = "\\1-\\2-\\3T\\4:\\5Z";
  auto& levels = root.add("levels", Setting::TypeArray);
  levels.add(Setting::TypeInt) = 1;
  levels.add(Setting::TypeInt) = 5;
  levels.add(Setting::TypeInt) = 151;
  levels.add(Setting::TypeInt) = 167;
  auto& bbox = root.add("bbox", Setting::TypeArray);
  bbox.add(Setting::TypeFloat) = 22.0;
  bbox.add(Setting::TypeFloat) = 60.0;
  bbox.add(Setting::TypeFloat) = 24.0;
  bbox.add(Setting::TypeFloat) = 62.0;
  auto& params = root.add("params", Setting::TypeArray);
  params.add(Setting::TypeString) = "TemperatUre";
  params.add(Setting::TypeString) = "SnowDepth";
  params.add(Setting::TypeString) = "Pressure";
  return config;
}
}

using SmartMet::Spine::ConfigBase;
using SmartMet::Plugin::WFS::DataSetDefinition;
namespace bg = boost::gregorian;
namespace pt = boost::posix_time;
using namespace boost::filesystem;
using boost::date_time::neg_infin;
using boost::date_time::pos_infin;

BOOST_AUTO_TEST_CASE(test_parse_config)
{
  BOOST_TEST_MESSAGE("+ [Testing parsing DataSetDefinition from libconfig::Setting]");
  auto raw_config = create_config();
  ConfigBase config(raw_config, "Test");
  try
  {
    auto ds_def = DataSetDefinition::create(config, raw_config->getRoot());
    BOOST_CHECK_EQUAL(std::string("Test1"), ds_def->get_name());
    BOOST_CHECK_EQUAL(std::string(test_dir), ds_def->get_dir().string());
  }
  catch (const std::exception& err)
  {
    std::cerr << "\nC++ exception of type '" << Fmi::current_exception_type() << "': " << err.what()
              << std::endl;
    BOOST_REQUIRE(false);
  }
}

BOOST_AUTO_TEST_CASE(test_time_extract)
{
  BOOST_TEST_MESSAGE("+ [Testing time extraction from file name]");
  auto raw_config = create_config();
  ConfigBase config(raw_config, "Test");
  auto ds_def = DataSetDefinition::create(config, raw_config->getRoot());
  try
  {
    pt::ptime t;
    t = ds_def->extract_origintime(path("foo/201209151234_pal_scandinavia_pinta_sqd"));
    BOOST_CHECK_EQUAL(std::string("2012-09-15T12:34:00"), Fmi::to_iso_extended_string(t));
  }
  catch (const std::exception& err)
  {
    std::cerr << "\nC++ exception of type '" << Fmi::current_exception_type() << "': " << err.what()
              << std::endl;
    BOOST_CHECK(false);
  }
}

namespace
{
inline DataSetDefinition::box_t mkbox(double x1, double y1, double x2, double y2)
{
  typedef DataSetDefinition::point_t point_t;
  return DataSetDefinition::box_t(point_t(x1, y1), point_t(x2, y2));
}
}

BOOST_AUTO_TEST_CASE(test_query_files)
{
  BOOST_TEST_MESSAGE("+ [Testing quering files by time interval]");
  auto raw_config = create_config();
  ConfigBase config(raw_config, "Test");
  auto ds_def = DataSetDefinition::create(config, raw_config->getRoot());
  std::vector<path> files;

  try
  {
    files = ds_def->query_files();
    BOOST_CHECK_EQUAL(2, (int)files.size());
  }
  catch (const std::exception& err)
  {
    std::cerr << "\nC++ exception of type '" << Fmi::current_exception_type() << "': " << err.what()
              << std::endl;
    BOOST_CHECK(false);
  }

  try
  {
    files = ds_def->query_files(pt::ptime(bg::date(2008, 8, 5), pt::time_duration(8, 0, 0, 0)));
    BOOST_CHECK_EQUAL(1, (int)files.size());
    BOOST_CHECK_EQUAL(std::string("200808050933_pal_skandinavia_pinta.sqd"),
                      files.at(0).filename().string());
  }
  catch (const std::exception& err)
  {
    std::cerr << "\nC++ exception of type '" << Fmi::current_exception_type() << "': " << err.what()
              << std::endl;
    BOOST_CHECK(false);
  }
}

BOOST_AUTO_TEST_CASE(test_bbox_intersection)
{
  //typedef DataSetDefinition::point_t point_t;
  typedef DataSetDefinition::box_t box_t;

  BOOST_TEST_MESSAGE("+ [Testing area intersection]");
  auto raw_config = create_config();
  ConfigBase config(raw_config, "Test");
  auto ds_def = DataSetDefinition::create(config, raw_config->getRoot());

  const box_t& bbox = ds_def->get_bbox();
  BOOST_CHECK_EQUAL(22.0, bbox.min_corner().x());
  BOOST_CHECK_EQUAL(60.0, bbox.min_corner().y());
  BOOST_CHECK_EQUAL(24.0, bbox.max_corner().x());
  BOOST_CHECK_EQUAL(62.0, bbox.max_corner().y());

  BOOST_CHECK(ds_def->intersects(mkbox(22.5, 60.5, 22.6, 60.6)));
  BOOST_CHECK(not ds_def->intersects(mkbox(19.0, 60.5, 19.2, 60.6)));
  BOOST_CHECK(ds_def->intersects(mkbox(19.0, 57.0, 22.5, 60.5)));
}
