#pragma once

#include "WfsConvenience.h"
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Xml
{
/**
      @brief Data of gml::SRSInformationGroup attrubute group

              @verbatim
  <attributeGroup name="SRSInformationGroup">
      <annotation>
              <documentation>
                                      The attributes uomLabels and axisLabels, defined in the
  SRSInformationGroup
                                      attribute group, are optional additional and redundant
  information for a CRS
                                      to simplify the processing of the coordinate values when a
  more complete
                                      definition of the CRS is not needed. This information shall be
  the same
                                      as included in the complete definition of the CRS, referenced
  by the srsName
                                      attribute. When the srsName attribute is included, either both
  or neither of
                                      the axisLabels and uomLabels attributes shall be included.
  When the srsName
                                      attribute is omitted, both of these attributes shall be
  omitted.
                                      The attribute axisLabels is an ordered list of labels for all
  the axes of this CRS.
                                      The gml:axisAbbrev value should be used for these axis labels,
  after spaces
                                      and forbidden characters are removed. When the srsName
  attribute is included,
                                      this attribute is optional. When the srsName attribute is
  omitted, this attribute
                                      shall also be omitted.
                                      The attribute uomLabels is an ordered list of unit of measure
  (uom) labels for
                                      all the axes of this CRS. The value of the string in the
  gml:catalogSymbol
                                      should be used for this uom labels, after spaces and forbidden
  characters
                                      are removed. When the axisLabels attribute is included, this
  attribute shall
                                      also be included. When the axisLabels attribute is omitted,
  this attribute
                                      shall also be omitted.</documentation>
      </annotation>
      <attribute name="axisLabels" type="gml:NCNameList"/>
      <attribute name="uomLabels" type="gml:NCNameList"/>
  </attributeGroup>
              @endverbatim
 */
struct GmlSRSInformationGroup
{
  std::vector<std::string> axisLabels;
  std::vector<std::string> uomLabels;
};

/**
       @brief Represents gml:SRSReferenceGroup attribute group

       @verbatim
       <attributeGroup name="SRSReferenceGroup">
      <annotation>
              <documentation>
                                      The attribute group SRSReferenceGroup is an optional reference
to the CRS
                                      used by this geometry, with optional additional information to
simplify the
                                      processing of the coordinates when a more complete definition
of the CRS is
                                      not needed.
                                      In general the attribute srsName points to a CRS instance of
                                      gml:AbstractCoordinateReferenceSystem. For well-known
references it is not
                                      required that the CRS description exists at the location the
URI points to.
                                      If no srsName attribute is given, the CRS shall be specified
as part of the
                                      larger context this geometry element is part of.
                                      </documentation>
      </annotation>
      <attribute name="srsName" type="anyURI"/>
      <attribute name="srsDimension" type="positiveInteger"/>
      <attributeGroup ref="gml:SRSInformationGroup"/>
</attributeGroup>
       @endverbatim
 */
struct GmlSRSReferenceGroup
{
  boost::optional<std::string> srs_name;
  boost::optional<unsigned MAY_ALIAS> srs_dimension;
  boost::optional<GmlSRSInformationGroup> srs_info;
};

/**
       @brief Represents gml::DirectPositionType

       @verbatim
<complexType name="DirectPositionType">
      <annotation>
              <documentation>
                                      Direct position instances hold the coordinates for a position
within some
                                      coordinate reference system (CRS). Since direct positions, as
data types, will often be
                                      included in larger objects (such as geometry elements) that
have references to CRS, the
                                      srsName attribute will in general be missing, if this
particular direct position is included
                                      in a larger element with such a reference to a CRS. In this
case, the CRS is implicitly
                                      assumed to take on the value of the containing object's CRS.
                                      if no srsName attribute is given, the CRS shall be specified
as part of the larger
                                      context this geometry element is part of, typically a
geometric object like a point, curve, etc.
                                      </documentation>
      </annotation>
      <simpleContent>
              <extension base="gml:doubleList">
                      <attributeGroup ref="gml:SRSReferenceGroup"/>
              </extension>
      </simpleContent>
</complexType>
       @endverbatim
 */
struct GmlDirectPositionType
{
  GmlSRSReferenceGroup srs_reference;
  std::vector<double> data;
};

/**
       @brief Represents gml::EnvelopeType

       @verbatim
<complexType name="EnvelopeType">
      <choice>
              <sequence>
                      <element name="lowerCorner" type="gml:DirectPositionType"/>
                      <element name="upperCorner" type="gml:DirectPositionType"/>
              </sequence>
              <element ref="gml:pos" minOccurs="2" maxOccurs="2">
                      <annotation>
                              <appinfo>deprecated</appinfo>
                      </annotation>
              </element>
              <element ref="gml:coordinates"/>
      </choice>
      <attributeGroup ref="gml:SRSReferenceGroup"/>
</complexType>
       @endverbatim
 */
struct GmlEnvelopeType
{
  GmlSRSReferenceGroup srs_reference;
  GmlDirectPositionType lower_corner;
  GmlDirectPositionType upper_corner;
};

boost::optional<GmlSRSInformationGroup> read_gml_srs_info_group(const xercesc::DOMElement& element);
GmlSRSReferenceGroup read_gml_srs_reference_group(const xercesc::DOMElement& element);
GmlDirectPositionType read_gml_direct_position_type(const xercesc::DOMElement& element);
GmlEnvelopeType read_gml_envelope_type(const xercesc::DOMElement& element);

}  // namespace Xml
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

/*
  NOT YET IMPLEMENTED

       <complexType name="CoordinatesType">
                <annotation>
                        <documentation>
                                                This type is deprecated for tuples with ordinate
  values that are numbers.
                                                CoordinatesType is a text string, intended to be
  used to record an array
                                                of tuples or coordinates.
                                                While it is not possible to enforce the internal
  structure of the string
                                                through schema validation, some optional attributes
  have been provided
                                                in previous versions of GML to support a description
  of the internal
                                                structure. These attributes are deprecated. The
  attributes were intended
                                                to be used as follows:
                                                Decimal symbol used for a decimal point (default="."
  a stop or period)
                                                cs              symbol used to separate components
  within a tuple or coordinate string (default="," a comma)
                                                ts              symbol used to separate tuples or
  coordinate strings (default=" " a space)
                                                Since it is based on the XML Schema string type,
  CoordinatesType may be
                                                used in the construction of tables of tuples or
  arrays of tuples, including
                                                ones that contain mixed text and numeric
  values.</documentation>
                </annotation>
                <simpleContent>
                        <extension base="string">
                                <attribute name="decimal" type="string" default="."/>
                                <attribute name="cs" type="string" default=","/>
                                <attribute name="ts" type="string" default="&#x20;"/>
                        </extension>
                </simpleContent>
        </complexType>


 */
