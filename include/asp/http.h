// SPDX-License-Identifier: MIT
// SPDX-CopyrightText: 2026 Rene Schickbauer

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "asp/err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Check if network is available.
// On success, writes the connection state to *out_connected and returns ASP_OK.
asp_err_t asp_net_is_connected(bool* out_connected);

// Perform HTTP GET request.
// On success, writes the HTTP status code to *out_status_code and returns ASP_OK.
// Returns ASP_ERR_FAIL on transport error.
// The body goes into `response`, NUL-terminated: at most max_len - 1 bytes,
// read until the server is done (a chunked response too). HTTPS URLs are
// checked against the firmware's global CA store (esp_tls_set_global_ca_store).
asp_err_t asp_http_get(const char* url, char* response, size_t max_len, int* out_status_code);

// Perform HTTP POST request.
// On success, writes the HTTP status code to *out_status_code and returns ASP_OK.
// Returns ASP_ERR_FAIL on transport error.
// The response body as for asp_http_get.
asp_err_t asp_http_post(const char* url, const char* body, char* response, size_t max_len, int* out_status_code);

#ifdef __cplusplus
}
#endif
