# SPDX-License-Identifier: GPL-2.0-or-later
#
# Guard test: DaemonClient::call is a nested-event-loop compatibility wrapper
# kept only as a test convenience. Production code must use callAsync, so this
# script fails if any synchronous `->call(` appears in the production source
# trees (ipc/DaemonClient itself and tests/ are exempt).

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR must be provided.")
endif()

set(guard_violations "")
foreach(scan_dir app backends core ui daemon)
    file(GLOB_RECURSE scan_files
        "${SOURCE_DIR}/${scan_dir}/*.cpp"
        "${SOURCE_DIR}/${scan_dir}/*.h"
    )
    foreach(scan_file ${scan_files})
        file(READ "${scan_file}" file_contents)
        string(FIND "${file_contents}" "->call(" match_offset)
        if(NOT match_offset EQUAL -1)
            list(APPEND guard_violations "${scan_file}")
        endif()
    endforeach()
endforeach()

if(guard_violations)
    list(JOIN guard_violations "\n  " violation_list)
    message(FATAL_ERROR
        "Synchronous daemon calls (->call() are not allowed in production code; "
        "use DaemonClient::callAsync. Violations:\n  ${violation_list}"
    )
endif()
