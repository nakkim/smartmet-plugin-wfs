#pragma once

#include <boost/function.hpp>
#include <boost/variant.hpp>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *   @brief Base class for URL generator from template with parameters substitution
 *
 *   Supported parameter template formats:
 *   - <b>foo=bar</b> - constant parameter which is copied into URL parameters
 *   - <b>${foo}</b> - reference to external parameter. The parameter is added to
 *      URL parameters even if empty. Comma separated list used in case of an array
 *   - <b>${?foo}</b> - same as <b>${foo}</b> but parameter is ommited from URL
 *      parameters if empty
 *   - <b>${*foo}</b> - separate URL parameter generated for each array element
 */
class UrlTemplateGenerator
{
 public:
  struct ParamRef
  {
    std::string name;
    bool omit_empty;
    bool separate_param;
  };

  struct StringParam
  {
    std::string name;
    std::string value;
  };

 public:
  UrlTemplateGenerator(const std::string& url, const std::vector<std::string>& param_templates);

  virtual ~UrlTemplateGenerator();

  std::string generate(
      boost::function1<std::vector<std::string>, std::string> param_getter_cb) const;

  inline const std::vector<boost::variant<ParamRef, StringParam> > get_content() const
  {
    return params;
  }

 private:
  void parse_param_def(const std::string& str);

  static std::string eval_string_param(
      const std::string& param_str,
      boost::function1<std::vector<std::string>, std::string> param_getter_cb,
      bool allow_list = true);

 private:
  std::string url;
  std::vector<boost::variant<ParamRef, StringParam> > params;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
