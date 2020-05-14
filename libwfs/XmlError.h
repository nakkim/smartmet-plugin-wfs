#pragma once

#include <list>
#include <stdexcept>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Xml
{
class XmlError : public std::runtime_error
{
  friend class Parser;

 public:
  enum error_level_t
  {
    ERROR,
    FATAL_ERROR
  };

 public:
  XmlError(const std::string& text, error_level_t error_level);

  virtual ~XmlError() throw();

  inline const std::list<std::string>& get_messages() const { return messages; }
  inline error_level_t get_error_level() const { return error_level; }
  void add_messages(const std::list<std::string>& messages);

  static const char* error_level_name(enum error_level_t error_level);

 private:
  std::list<std::string> messages;
  error_level_t error_level;
};

}  // namespace Xml
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
