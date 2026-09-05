#include <icarus/core/identity_protocol.hpp>

#include <string>

int main()
{
    const std::string hello = icarus::core::build_identity_message(
        icarus::core::IdentityMessageKind::hello,
        "76561198000000000",
        "2:41"
    );

    if(hello.empty())
    {
        return 1;
    }

    const auto parsed = icarus::core::parse_identity_message(hello);

    if(!parsed.valid)
    {
        return 2;
    }

    if(parsed.kind != icarus::core::IdentityMessageKind::hello)
    {
        return 3;
    }

    if(parsed.player_uid != "76561198000000000" || parsed.network_id != "2:41")
    {
        return 4;
    }

    if(!icarus::core::build_identity_message(
           icarus::core::IdentityMessageKind::identity,
           "bad\tuid",
           "2:41"
       ).empty())
    {
        return 5;
    }

    if(icarus::core::parse_identity_message("ICARUS-ID\t2\tH\t123\t2:41").valid)
    {
        return 6;
    }

    if(icarus::core::parse_identity_message("ICARUS-ID\t1\tX\t123\t2:41").valid)
    {
        return 7;
    }

    return 0;
}
