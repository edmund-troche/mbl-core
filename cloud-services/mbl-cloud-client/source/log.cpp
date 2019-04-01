/*
 * Copyright (c) 2017 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "log.h"

#include "mbed-trace-helper/mbed-trace-helper.h"
#include "mbed-trace/mbed_trace.h"

#include <cassert>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <sys/time.h>
#include <ctime>

#define TRACE_GROUP "mbl"

static const char g_log_path[] = "/var/log/mbl-cloud-client.log";
static FILE* g_log_stream = 0;

// Format the time prefix strings like "YYYY-mm-DDTHH:MM:ss+HHMM " (one of the
// ISO 8601 formats). That's 25 chars + nul.
static const char g_time_prefix_format[] = "%FT%T%z ";
static const size_t g_time_prefix_buffer_size = 26;

static void strncpy_with_nul(char* const dest, const char* const src, const size_t n)
{
    assert(n >= 1);
    strncpy(dest, src, n - 1);
    dest[n - 1] = '\0';
}

static char* make_time_prefix(char* const buffer, const size_t buffer_size)
{
    assert(buffer_size >= g_time_prefix_buffer_size);

    static const char fail_str[] = "TIME UNAVAILABLE ";

    struct timeval tv;
    if (gettimeofday(&tv, 0) != 0) {
        strncpy_with_nul(buffer, fail_str, buffer_size);
        return buffer;
    }

    struct tm time_info;
    if (!localtime_r(&tv.tv_sec, &time_info)) {
        strncpy_with_nul(buffer, fail_str, buffer_size);
        return buffer;
    }

    if (strftime(buffer, buffer_size, g_time_prefix_format, &time_info) == 0) {
        strncpy_with_nul(buffer, fail_str, buffer_size);
    }
    return buffer;
}

/**
 * Callback for printing lines generated by the mbed-trace library. Writes to
 * our log file.
 */
extern "C" void mbl_trace_print_handler(const char* const str)
{
    // Assuming that mbed-trace will synchronize calls to this function.

    if (!g_log_stream) {
        if (g_log_stream) {
            std::fclose(g_log_stream);
        }
        g_log_stream = std::fopen(g_log_path, "a");
        if (g_log_stream) {
            // We can't use mbed-trace to log here because it doesn't expect to
            // be used from within its own print handler
            char buffer[g_time_prefix_buffer_size];
            std::fprintf(g_log_stream,
                         "%s[INFO][mbl ]: Log file opened\n",
                         make_time_prefix(buffer, sizeof(buffer)));
        }
    }

    if (g_log_stream) {
        std::fprintf(g_log_stream, "%s\n", str);
        std::fflush(g_log_stream);
    }
}

/**
 * Callback for creating a prefix for lines generated by the mbed-trace
 * library. Returns timestamps.
 */
extern "C" char* mbl_trace_prefix_handler(size_t)
{
    // The mbed-trace library requires a function that allocates its own buffer
    // and returns a pointer to it. Use "thread_local" to make it thread safe.
    thread_local static char buffer[g_time_prefix_buffer_size];
    return make_time_prefix(buffer, sizeof(buffer));
}

namespace mbl
{

MblError log_init(const char* user_log_path)
{
    assert(g_log_stream == 0);
    g_log_stream = std::fopen(user_log_path ? user_log_path : g_log_path, "a");
    if (!g_log_stream) {
        std::perror("log_init: fopen");
        return Error::LogInitFopen;
    }

    mbed_trace_init();

    // No colors, no carriage returns, print all log levels
    // TODO: set trace default level back to TRACE_ACTIVE_LEVEL_INFO before
    // merging to master
    mbed_trace_config_set(TRACE_ACTIVE_LEVEL_DEBUG);

    mbed_trace_print_function_set(mbl_trace_print_handler);
    mbed_trace_cmdprint_function_set(mbl_trace_print_handler);
    mbed_trace_prefix_function_set(mbl_trace_prefix_handler);

    if (!mbed_trace_helper_create_mutex()) {
        return Error::LogInitMutexCreate;
    }

    mbed_trace_mutex_wait_function_set(mbed_trace_helper_mutex_wait);
    mbed_trace_mutex_release_function_set(mbed_trace_helper_mutex_release);

    return Error::None;
}

} // namespace mbl
