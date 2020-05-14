#pragma once

#include "RequestParameterMap.h"
#include "StoredQueryConfig.h"
#include "StoredQueryParamRegistry.h"
#include "SupportsExtraHandlerParams.h"
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *  @brief The class for adding quality parameter support to stored query handler
 */
class SupportsQualityParameters : protected virtual SupportsExtraHandlerParams,
                                  protected virtual StoredQueryParamRegistry
{
 public:
  SupportsQualityParameters(boost::shared_ptr<StoredQueryConfig> config);
  virtual ~SupportsQualityParameters();

  /** \brief Test if the input string contains "qc_" prefix (case insensitive).
   *  \param[in] name Test string.
   *  \retval true The prefis found.
   *  \retval false The prefis not found.
   */
  bool isQCParameter(const std::string& name) const;

  /** \brief Resolve state of the qualityInfo parameter.
   *  \param[in] params Request map includes the state of the qualityInfo parameter.
   *  \retval true Value of the qualityInfo parameter is "on".
   *  \retval false Value of the qualityInfo parameter is something else than "on".
   */
  bool supportQualityInfo(const RequestParameterMap& params) const;

  /** \brief Find first parameter name with "qc_" prefix.
   *  \param[in] parameters Parameter vector from which parameter with "qc_" prefix were seatched.
   *  \return Vector element that has parameter with "qc_" prefix or
   *          if such a prameter is not found end of the input vector is returned.
   */
  std::vector<std::string>::const_iterator firstQCParameter(
      const std::vector<std::string>& parameters) const;

 private:
  bool m_supportQCParameters;

 protected:
  static const char* P_QUALITY_INFO;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
