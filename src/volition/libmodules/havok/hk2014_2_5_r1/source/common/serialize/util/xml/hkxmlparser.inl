// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlparser.inl
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SF): the 2014.1 SDK's inline code, checked against rfg.exe.

// 0x00F53300 (:10)
inline hkXmlParser::StartElement* hkXmlParser::Node::asStart()
{
    return (type == START_ELEMENT) ? static_cast<hkXmlParser::StartElement*>(this) : nullptr;
}

// 0x00FA86D0 (:14)
inline hkXmlParser::EndElement* hkXmlParser::Node::asEnd()
{
    return (type == END_ELEMENT) ? static_cast<hkXmlParser::EndElement*>(this) : nullptr;
}

// 0x00FA86E0 (:18)
inline hkXmlParser::Characters* hkXmlParser::Node::asCharacters()
{
    return (type == CHARACTERS) ? static_cast<hkXmlParser::Characters*>(this) : nullptr;
}
