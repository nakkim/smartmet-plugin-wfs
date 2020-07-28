#define BOOST_TEST_MODULE TArrayParameterTemplate
#define BOOST_TEST_DYN_LINK 1
#include <boost/test/unit_test.hpp>

#include <cstdio>
#include <string>
#include "ArrayParameterTemplate.h"
#include "RequestParameterMap.h"
#include "StoredQueryConfig.h"

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "ArrayParameterTemplate tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

using SmartMet::Spine::Value;
using SmartMet::Plugin::WFS::RequestParameterMap;

namespace
{
std::string create_config(const std::string& param_name,
                          const std::string& param_type,
                          const std::string& template_name,
                          const std::string& template_def,
                          const std::string& base_path = "")
{
  char fn_buf[80];
  strncpy(fn_buf, "/tmp/param_tmpl.XXXXXX", sizeof(fn_buf));
  fn_buf[sizeof(fn_buf) - 1] = 0;
  int fd = mkstemp(fn_buf);
  FILE* output = fdopen(fd, "w");

  fputs("disable = false;\n", output);
  fputs("id = \"foo\"; \n", output);
  fputs("constructor_name = \"create_foo\";\n", output);
  fputs("title = {en = \"the title\";};\n", output);
  fputs("abstract = {en = \"the abstract\";};\n", output);
  fputs("template = \"foo.c2t\";\n", output);
  fputs("defaultLanguage = \"en\";\n", output);
  fputs("returnTypeNames = [\"fooRetVal\"];\n", output);
  fputs("parameters:\n", output);
  fputs("(\n", output);
  fputs("  {\n", output);
  fprintf(output, "    name = \"%s\";\n", param_name.c_str());
  fputs("    title = {en=\"the title\";};\n", output);
  fputs("    abstract = {en=\"the abstract\";};\n", output);
  fputs("    xmlType = \"testType\";\n", output);
  fprintf(output, "    type = \"%s\";\n", param_type.c_str());
  fputs("  }\n", output);
  fputs(");\n", output);
  fputs("\n", output);
  fprintf(output, "%s:{\n", base_path == "" ? "handler_params" : base_path.c_str());
  fprintf(output, "%s = %s;\n", template_name.c_str(), template_def.c_str());
  fprintf(output, "};\n");

  fclose(output);
  close(fd);

  return fn_buf;
}

void log_exception(const char* file, int line)
{
  try
  {
    throw;
  }
  catch (const std::exception& err)
  {
    std::cerr << "C++ exception of type '" << Fmi::current_exception_type() << "' catched at "
              << file << '#' << line << ": " << err.what() << std::endl;
  }
  catch (...)
  {
    std::cerr << "C++ exception of type '" << Fmi::current_exception_type() << "' catched  at "
              << file << '#' << line << std::endl;
  }
}

template <typename ValueType>
void add(RequestParameterMap& param_map, const std::string& name, ValueType value)
{
  Value tmp(value);
  param_map.insert_value(name, tmp);
}
}

#define LOG_EXCEPTION(x)               \
  try                                  \
  {                                    \
    x;                                 \
  }                                    \
  catch (...)                          \
  {                                    \
    log_exception(__FILE__, __LINE__); \
    throw;                             \
  }

BOOST_AUTO_TEST_CASE(array_param_1)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: part 1 (configuration root)]");

  const std::string fn = create_config("test", "string[4]", "foo", "[\"${test}\"]");
  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_NO_THROW(pt.reset(new ArrayParameterTemplate(*config, "foo")));

  const std::vector<ParameterTemplateItem>& items = pt->get_items();
  BOOST_REQUIRE_EQUAL((int)items.size(), 1);
  const ParameterTemplateItem& item = items.at(0);
  BOOST_CHECK(not item.plain_text);
  BOOST_REQUIRE(item.param_ref);
  BOOST_CHECK_EQUAL(*item.param_ref, std::string("test"));
  BOOST_CHECK(not item.param_ind);
  BOOST_CHECK(not item.default_value);

  RequestParameterMap param_map;
  add(param_map, "test2", "baz");

  std::vector<std::string> value;
  BOOST_CHECK_NO_THROW(value = pt->get_string_array(param_map));
  BOOST_CHECK_EQUAL((int)value.size(), 0);

  add(param_map, "test", "a");
  add(param_map, "test", "bb");
  add(param_map, "test", "ccc");
  add(param_map, "test", "dddd");

  BOOST_CHECK_NO_THROW(value = pt->get_string_array(param_map));
  BOOST_CHECK_EQUAL((int)value.size(), 4);
  BOOST_CHECK_EQUAL(value.at(0), std::string("a"));
  BOOST_CHECK_EQUAL(value.at(1), std::string("bb"));
  BOOST_CHECK_EQUAL(value.at(2), std::string("ccc"));
  BOOST_CHECK_EQUAL(value.at(3), std::string("dddd"));
}

BOOST_AUTO_TEST_CASE(array_param_1_non_root)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: part 2 (not configuration root)]");

  const std::string fn = create_config("test", "string[4]", "foo", "[\"${test}\"]", "bar");
  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_THROW(pt.reset(new ArrayParameterTemplate(*config, "baz", "foo")),
                      SmartMet::Spine::Exception);
  BOOST_REQUIRE_NO_THROW(pt.reset(new ArrayParameterTemplate(*config, "bar", "foo")));

  const std::vector<ParameterTemplateItem>& items = pt->get_items();
  BOOST_REQUIRE_EQUAL((int)items.size(), 1);
  const ParameterTemplateItem& item = items.at(0);
  BOOST_CHECK(not item.plain_text);
  BOOST_REQUIRE(item.param_ref);
  BOOST_CHECK_EQUAL(*item.param_ref, std::string("test"));
  BOOST_CHECK(not item.param_ind);
  BOOST_CHECK(not item.default_value);

  RequestParameterMap param_map;
  add(param_map, "test2", "baz");

  std::vector<std::string> value;
  BOOST_CHECK_NO_THROW(value = pt->get_string_array(param_map));
  BOOST_CHECK_EQUAL((int)value.size(), 0);

  add(param_map, "test", "a");
  add(param_map, "test", "bb");
  add(param_map, "test", "ccc");
  add(param_map, "test", "dddd");

  BOOST_CHECK_NO_THROW(value = pt->get_string_array(param_map));
  BOOST_CHECK_EQUAL((int)value.size(), 4);
  BOOST_CHECK_EQUAL(value.at(0), std::string("a"));
  BOOST_CHECK_EQUAL(value.at(1), std::string("bb"));
  BOOST_CHECK_EQUAL(value.at(2), std::string("ccc"));
  BOOST_CHECK_EQUAL(value.at(3), std::string("dddd"));
}

BOOST_AUTO_TEST_CASE(mix_of_predefined_and_given_parameters_1)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: mix of predefined and given parameters (part 1)]");

  const std::string fn = create_config("test", "double", "foo", "[\"1.1\", \"${test}\", \"1.3\"]");
  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_NO_THROW(pt.reset(new ArrayParameterTemplate(*config, "foo")));

  const std::vector<ParameterTemplateItem>& items = pt->get_items();
  BOOST_REQUIRE_EQUAL((int)items.size(), 3);
  const ParameterTemplateItem& item1 = items.at(0);
  BOOST_REQUIRE(item1.plain_text);
  BOOST_REQUIRE(item1.plain_text->type() == typeid(std::string));
  BOOST_CHECK_EQUAL(item1.plain_text->get_string(), std::string("1.1"));
  BOOST_CHECK(not item1.param_ref);
  BOOST_CHECK(not item1.param_ind);
  BOOST_CHECK(not item1.default_value);
  const ParameterTemplateItem& item2 = items.at(1);
  BOOST_CHECK(not item2.plain_text);
  BOOST_REQUIRE(item2.param_ref);
  BOOST_CHECK_EQUAL(*item2.param_ref, std::string("test"));
  BOOST_CHECK(not item2.param_ind);
  BOOST_CHECK(not item2.default_value);
  const ParameterTemplateItem& item3 = items.at(2);
  BOOST_REQUIRE(item3.plain_text);
  BOOST_REQUIRE(item3.plain_text->type() == typeid(std::string));
  BOOST_CHECK_EQUAL(item3.plain_text->get_string(), std::string("1.3"));
  BOOST_CHECK(not item1.param_ref);
  BOOST_CHECK(not item1.param_ind);
  BOOST_CHECK(not item1.default_value);

  RequestParameterMap param_map;
  add(param_map, "test", 1.2);

  std::vector<double> value;
  BOOST_CHECK_NO_THROW(LOG_EXCEPTION(value = pt->get_double_array(param_map)));
  BOOST_CHECK_EQUAL((int)value.size(), 3);
  BOOST_CHECK_CLOSE(value.at(0), (double)1.1, 1e-10);
  BOOST_CHECK_CLOSE(value.at(1), (double)1.2, 1e-10);
  BOOST_CHECK_CLOSE(value.at(2), (double)1.3, 1e-10);
}

BOOST_AUTO_TEST_CASE(mix_of_predefined_and_given_parameters_2)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: mix of predefined and given parameters (part 2)]");

  const std::string fn =
      create_config("test", "double[2]", "foo", "[\"1.1\", \"${test}\", \"1.3\"]");
  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_NO_THROW(pt.reset(new ArrayParameterTemplate(*config, "foo")));

  const std::vector<ParameterTemplateItem>& items = pt->get_items();
  BOOST_REQUIRE_EQUAL((int)items.size(), 3);
  const ParameterTemplateItem& item1 = items.at(0);
  BOOST_REQUIRE(item1.plain_text);
  BOOST_REQUIRE(item1.plain_text->type() == typeid(std::string));
  BOOST_CHECK_EQUAL(item1.plain_text->get_string(), std::string("1.1"));
  BOOST_CHECK(not item1.param_ref);
  BOOST_CHECK(not item1.param_ind);
  BOOST_CHECK(not item1.default_value);
  const ParameterTemplateItem& item2 = items.at(1);
  BOOST_CHECK(not item2.plain_text);
  BOOST_REQUIRE(item2.param_ref);
  BOOST_CHECK_EQUAL(*item2.param_ref, std::string("test"));
  BOOST_CHECK(not item2.param_ind);
  BOOST_CHECK(not item2.default_value);
  const ParameterTemplateItem& item3 = items.at(2);
  BOOST_REQUIRE(item3.plain_text);
  BOOST_REQUIRE(item3.plain_text->type() == typeid(std::string));
  BOOST_CHECK_EQUAL(item3.plain_text->get_string(), std::string("1.3"));
  BOOST_CHECK(not item1.param_ref);
  BOOST_CHECK(not item1.param_ind);
  BOOST_CHECK(not item1.default_value);

  RequestParameterMap param_map;
  add(param_map, "test", 1.2);
  add(param_map, "test", 1.21);

  std::vector<double> value;
  BOOST_CHECK_NO_THROW(value = pt->get_double_array(param_map));
  BOOST_CHECK_EQUAL((int)value.size(), 4);
  BOOST_CHECK_CLOSE(value.at(0), (double)1.1, 1e-10);
  BOOST_CHECK_CLOSE(value.at(1), (double)1.2, 1e-10);
  BOOST_CHECK_CLOSE(value.at(2), (double)1.21, 1e-10);
  BOOST_CHECK_CLOSE(value.at(3), (double)1.3, 1e-10);
}

BOOST_AUTO_TEST_CASE(array_parameter_shuffle)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: array parameter shuffle 1]");

  const std::string fn =
      create_config("test",
                    "string[4]",
                    "foo",
                    "[\"${test[1]}\", \"${test[0]}\", \"${test[3]}\", \"${test[2]}\"]");

  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_NO_THROW(pt.reset(new ArrayParameterTemplate(*config, "foo")));

  int exp_ind[4] = {1, 0, 3, 2};
  const std::vector<ParameterTemplateItem>& items = pt->get_items();
  BOOST_REQUIRE_EQUAL((int)items.size(), 4);
  for (int i = 0; i < 4; i++)
  {
    const ParameterTemplateItem& item = items.at(i);
    BOOST_REQUIRE(not item.plain_text);
    BOOST_CHECK(item.param_ref);
    BOOST_CHECK_EQUAL(*item.param_ref, std::string("test"));
    BOOST_CHECK(item.param_ind);
    BOOST_CHECK_EQUAL((int)(*item.param_ind), exp_ind[i]);
    BOOST_CHECK(not item.default_value);
  }
}

BOOST_AUTO_TEST_CASE(array_parameter_shuffle_2)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: array parameter shuffle 2]");

  const std::string fn =
      create_config("test",
                    "string[4]",
                    "foo",
                    "[\"${test[1]:a1}\", \"${test[0]:a2}\", \"${test[3]:a3}\", \"${test[2]:a4}\"]");

  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_NO_THROW(pt.reset(new ArrayParameterTemplate(*config, "foo")));

  int exp_ind[4] = {1, 0, 3, 2};
  const std::vector<ParameterTemplateItem>& items = pt->get_items();
  BOOST_REQUIRE_EQUAL((int)items.size(), 4);
  for (int i = 0; i < 4; i++)
  {
    const ParameterTemplateItem& item = items.at(i);
    BOOST_REQUIRE(not item.plain_text);
    BOOST_CHECK(item.param_ref);
    BOOST_CHECK_EQUAL(*item.param_ref, std::string("test"));
    BOOST_CHECK(item.param_ind);
    BOOST_CHECK_EQUAL((int)(*item.param_ind), exp_ind[i]);
    BOOST_CHECK(item.default_value);
  }

  RequestParameterMap param_map;
  std::vector<std::string> t1;
  BOOST_REQUIRE_NO_THROW(t1 = pt->get_string_array(param_map));
  BOOST_REQUIRE_EQUAL(4, (int)t1.size());
  BOOST_CHECK_EQUAL(std::string("a1"), t1.at(0));
  BOOST_CHECK_EQUAL(std::string("a2"), t1.at(1));
  BOOST_CHECK_EQUAL(std::string("a3"), t1.at(2));
  BOOST_CHECK_EQUAL(std::string("a4"), t1.at(3));

  add(param_map, "test", "b1");

  BOOST_REQUIRE_NO_THROW(t1 = pt->get_string_array(param_map));
  BOOST_REQUIRE_EQUAL(4, (int)t1.size());
  BOOST_CHECK_EQUAL(std::string("a1"), t1.at(0));
  BOOST_CHECK_EQUAL(std::string("b1"), t1.at(1));
  BOOST_CHECK_EQUAL(std::string("a3"), t1.at(2));
  BOOST_CHECK_EQUAL(std::string("a4"), t1.at(3));

  add(param_map, "test", "b2");
  add(param_map, "test", "b3");
  add(param_map, "test", "b4");

  BOOST_REQUIRE_NO_THROW(t1 = pt->get_string_array(param_map));
  BOOST_REQUIRE_EQUAL(4, (int)t1.size());
  BOOST_CHECK_EQUAL(std::string("b2"), t1.at(0));
  BOOST_CHECK_EQUAL(std::string("b1"), t1.at(1));
  BOOST_CHECK_EQUAL(std::string("b4"), t1.at(2));
  BOOST_CHECK_EQUAL(std::string("b3"), t1.at(3));
}

BOOST_AUTO_TEST_CASE(array_parameter_shuffle_out_of_range_index)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: array parameter shuffle out of range]");

  const std::string fn =
      create_config("test",
                    "string[4]",
                    "foo",
                    "[\"${test[1]}\", \"${test[4]}\", \"${test[3]}\", \"${test[2]}\"]");

  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_THROW(pt.reset(new ArrayParameterTemplate(*config, "foo")), SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(typo_in_parameter_reference)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: typo in param reference]");

  const std::string fn =
      create_config("test",
                    "string[4]",
                    "foo",
                    "[\"${test[1]}\", \"${test[0]}\", \"${tset[3]}\", \"${test[2]}\"]");

  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_THROW(pt.reset(new ArrayParameterTemplate(*config, "foo")), SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(array_size_detection_1)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: array size detection 1]");

  const std::string fn = create_config("test", "string[4]", "foo", "[\"${test}\"]");

  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_NO_THROW(pt.reset(new ArrayParameterTemplate(*config, "foo", 4, 4)));
}

BOOST_AUTO_TEST_CASE(array_size_detection_error_1)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: array size detection error 1]");

  const std::string fn = create_config("test", "string[4]", "foo", "[\"${test}\"]");

  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_THROW(pt.reset(new ArrayParameterTemplate(*config, "foo", 1, 3)),
                      SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(array_size_detection_2)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: array size detection 2]");

  const std::string fn = create_config("test", "string[4]", "foo", "[\"${test}\", \"${test}\"]");

  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_NO_THROW(pt.reset(new ArrayParameterTemplate(*config, "foo", 8, 8)));
}

BOOST_AUTO_TEST_CASE(array_size_detection_error_2)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: array size detection error 2]");

  const std::string fn = create_config("test", "string[4]", "foo", "[\"${test}\", \"${test}\"]");

  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_THROW(pt.reset(new ArrayParameterTemplate(*config, "foo", 12, 12)),
                      SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(array_size_detection_3)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: array size detection 3]");

  const std::string fn = create_config("test", "string[4]", "foo", "[\"${test}\", \"${test}\"]");

  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_NO_THROW(pt.reset(new ArrayParameterTemplate(*config, "foo", 8, 8)));
}

BOOST_AUTO_TEST_CASE(array_size_detection_error_3)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: array size detection error 3]");

  const std::string fn = create_config("test", "string[4]", "foo", "[\"${test}\", \"${test}\"]");

  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_THROW(pt.reset(new ArrayParameterTemplate(*config, "foo", 2, 2)),
                      SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(array_with_min_len_0_ommitted_in_config)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: array with min size 0 not found in config"
      " (should get warning)]");

  const std::string fn = create_config("test", "string[4]", "foo", "[\"${test}\", \"${test}\"]");

  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_NO_THROW(pt.reset(new ArrayParameterTemplate(*config, "bar", 0, 10)));
  BOOST_CHECK_EQUAL(0, int(pt->get_items().size()));
}

BOOST_AUTO_TEST_CASE(array_with_min_len_1_ommitted_in_config)
{
  using namespace SmartMet;
  using namespace SmartMet::Plugin::WFS;

  BOOST_TEST_MESSAGE("+[Testing ArrayParameterTemplate: array with min size 1 not found in config"
      " (test pass if exception thrown)]");

  const std::string fn = create_config("test", "string[4]", "foo", "[\"${test}\", \"${test}\"]");

  boost::shared_ptr<StoredQueryConfig> config;
  BOOST_REQUIRE_NO_THROW(config.reset(new StoredQueryConfig(fn, nullptr)));
  unlink(fn.c_str());

  boost::shared_ptr<ArrayParameterTemplate> pt;
  BOOST_REQUIRE_THROW(pt.reset(new ArrayParameterTemplate(*config, "bar", 1, 10)),
                      SmartMet::Spine::Exception);
}
