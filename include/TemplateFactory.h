// ======================================================================
/*!
 * \brief A factory for thread safe template formatting
 */
// ======================================================================

#pragma once

#include <boost/filesystem/path.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <macgyver/TemplateFormatter.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
using SharedFormatter = boost::shared_ptr<Fmi::TemplateFormatter>;

class TemplateFactory : public boost::noncopyable
{
 public:
  SharedFormatter get(const boost::filesystem::path& theFilename) const;

};  // class TemplateFactory

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
