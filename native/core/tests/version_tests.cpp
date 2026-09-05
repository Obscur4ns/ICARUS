#include <icarus/core/version.hpp>

#include <string_view>

int main()
{
    const std::string_view version = icarus::core::version();

    if(version.empty())
    {
        return 1;
    }

    if(version.find(' ') != std::string_view::npos)
    {
        return 2;
    }

    return 0;
}
