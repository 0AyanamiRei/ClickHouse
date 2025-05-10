#pragma once

#include <Functions/FunctionBaseXXConversion.h>

#include <Common/Base58.h>

namespace DB
{
struct Base58EncodeTraits
{
    template <typename Col>
    static size_t getBufferSize(Col const & src_column)
    {
        auto const src_length = src_column.getChars().size();
        /// Base58 has efficiency of 73% (8/11) [https://monerodocs.org/cryptography/base58/],
        /// and we take double scale to avoid any reallocation.
        constexpr auto oversize = 2;
        return static_cast<size_t>(ceil(oversize * src_length + 1));
    }

    static size_t perform(std::string_view src, UInt8 * dst)
    {
        return encodeBase58(reinterpret_cast<const UInt8 *>(src.data()), src.size(), dst);
    }
};

struct Base58DecodeTraits
{
    template <typename Col>
    static size_t getBufferSize(Col const & src_column)
    {
        /// For Base58, each character represents ~5.9 bits
        /// We just need the input size as the upper bound
        return src_column.byteSize();
    }

    static std::optional<size_t> perform(std::string_view src, UInt8 * dst)
    {
        return decodeBase58(reinterpret_cast<const UInt8 *>(src.data()), src.size(), dst);
    }
};
}
