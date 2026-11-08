// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlstreamparser.inl
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SF): the 2014.1 SDK's template, checked against the <unsigned short>
// instance in rfg.exe.

// <unsigned short> 0x00F56900 (:11): a quoted, comma separated list of integers, appended to out. Each value is
// truncated to IntegerType.
template<typename IntegerType>
hkResult hkXmlStreamParser::getIntListAttribute(const char* key, hkArray<IntegerType,hkContainerHeapAllocator>& out) const
{
    hkSubString subString;
    if (getValue(key, subString) != HK_SUCCESS)
    {
        return HK_FAILURE;
    }
    if (!(subString.m_start[0] == '"' && subString.m_end[-1] == '"'))
    {
        return HK_FAILURE;
    }
    subString.m_start++;
    subString.m_end--;
    for (const char* it = subString.m_start; it < subString.m_end; ++it)
    {
        const char* endIt = it;
        while (endIt != subString.m_end && *endIt != ',')
        {
            ++endIt;
        }
        hkSubString substr(it, endIt);
        int64_t valueOut;
        if (parseInt(substr, valueOut) == HK_FAILURE)
        {
            return HK_FAILURE;
        }
        out.pushBack(static_cast<IntegerType>(valueOut));
        it = endIt;
    }
    return HK_SUCCESS;
}
