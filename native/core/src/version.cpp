#include <icarus/core/version.hpp>

namespace icarus::core
{
std::string_view version() noexcept
{
    return ICARUS_VERSION_STRING;
}
}
