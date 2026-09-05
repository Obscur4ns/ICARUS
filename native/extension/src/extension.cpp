#include <icarus/core/version.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string_view>

namespace
{
void write_output(char* output, unsigned int output_size, std::string_view value) noexcept
{
    if(output == nullptr || output_size == 0)
    {
        return;
    }

    const std::size_t capacity = static_cast<std::size_t>(output_size - 1);
    const std::size_t length = std::min(value.size(), capacity);

    std::memcpy(output, value.data(), length);
    output[length] = '\0';
}

std::string_view dispatch(std::string_view function) noexcept
{
    if(function == "ping")
    {
        return "pong";
    }

    if(function == "version")
    {
        return icarus::core::version();
    }

    return "unsupported";
}
}

extern "C"
{
__declspec(dllexport) void __stdcall RVExtensionVersion(char* output, unsigned int output_size)
{
    write_output(output, output_size, icarus::core::version());
}

__declspec(dllexport) void __stdcall RVExtension(char* output, unsigned int output_size, const char* function)
{
    const std::string_view function_name = function == nullptr ? std::string_view{} : std::string_view{function};
    write_output(output, output_size, dispatch(function_name));
}

__declspec(dllexport) int __stdcall RVExtensionArgs(
    char* output,
    unsigned int output_size,
    const char* function,
    const char**,
    unsigned int)
{
    const std::string_view function_name = function == nullptr ? std::string_view{} : std::string_view{function};
    write_output(output, output_size, dispatch(function_name));
    return 0;
}
}
