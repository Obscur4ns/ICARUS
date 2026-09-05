function(icarus_read_project_version version_file output_variable)
    if(NOT EXISTS "${version_file}")
        message(FATAL_ERROR "Version file not found: ${version_file}")
    endif()

    foreach(component MAJOR MINOR PATCH)
        file(
            STRINGS "${version_file}"
            line
            REGEX "^#define[ \t]+${component}[ \t]+[0-9]+[ \t]*$"
        )

        if(NOT line)
            message(FATAL_ERROR "Missing ${component} in ${version_file}")
        endif()

        string(
            REGEX REPLACE
            "^#define[ \t]+${component}[ \t]+([0-9]+)[ \t]*$"
            "\\1"
            value
            "${line}"
        )

        set("${component}_VALUE" "${value}")
    endforeach()

    set(
        "${output_variable}"
        "${MAJOR_VALUE}.${MINOR_VALUE}.${PATCH_VALUE}"
        PARENT_SCOPE
    )
endfunction()
