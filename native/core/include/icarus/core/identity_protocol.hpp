#pragma once

#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

namespace icarus::core
{
inline constexpr std::uint16_t identity_protocol_major = 1;
inline constexpr std::size_t identity_player_uid_max = 39;
inline constexpr std::size_t identity_network_id_max = 63;

enum class IdentityMessageKind
{
    hello,
    identity,
};

struct IdentityMessage
{
    bool valid{};
    IdentityMessageKind kind{IdentityMessageKind::identity};
    std::string_view player_uid;
    std::string_view network_id;
};

[[nodiscard]] constexpr bool valid_identity_field(
    std::string_view value,
    std::size_t maximum_length
) noexcept
{
    if(value.empty() || value.size() > maximum_length)
    {
        return false;
    }

    for(const char character : value)
    {
        if(character == '\t' || character == '\r' || character == '\n')
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline std::string build_identity_message(
    IdentityMessageKind kind,
    std::string_view player_uid,
    std::string_view network_id
)
{
    if(!valid_identity_field(player_uid, identity_player_uid_max)
        || !valid_identity_field(network_id, identity_network_id_max))
    {
        return {};
    }

    std::string result{"ICARUS-ID\t1\t"};
    result += kind == IdentityMessageKind::hello ? 'H' : 'I';
    result += '\t';
    result += player_uid;
    result += '\t';
    result += network_id;
    return result;
}

[[nodiscard]] inline IdentityMessage parse_identity_message(std::string_view value) noexcept
{
    std::array<std::string_view, 5> fields{};

    for(std::size_t index = 0; index < fields.size() - 1; ++index)
    {
        const std::size_t separator = value.find('\t');

        if(separator == std::string_view::npos)
        {
            return {};
        }

        fields[index] = value.substr(0, separator);
        value.remove_prefix(separator + 1);
    }

    fields.back() = value;

    if(fields[0] != "ICARUS-ID")
    {
        return {};
    }

    std::uint16_t protocol_major{};
    const char* begin = fields[1].data();
    const char* end = begin + fields[1].size();
    const auto parsed = std::from_chars(begin, end, protocol_major);

    if(parsed.ec != std::errc{} || parsed.ptr != end
        || protocol_major != identity_protocol_major)
    {
        return {};
    }

    IdentityMessageKind kind{};

    if(fields[2] == "H")
    {
        kind = IdentityMessageKind::hello;
    }
    else if(fields[2] == "I")
    {
        kind = IdentityMessageKind::identity;
    }
    else
    {
        return {};
    }

    if(!valid_identity_field(fields[3], identity_player_uid_max)
        || !valid_identity_field(fields[4], identity_network_id_max))
    {
        return {};
    }

    return {
        .valid = true,
        .kind = kind,
        .player_uid = fields[3],
        .network_id = fields[4],
    };
}
}
