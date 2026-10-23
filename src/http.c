// SPDX-License-Identifier: MIT
// SPDX-CopyrightText: 2026 Rene Schickbauer

#include "asp/http.h"
#include <string.h>
#include "esp_http_client.h"
#include "esp_netif.h"

asp_err_t asp_net_is_connected(bool* out_connected) {
    if (!out_connected) return ASP_ERR_PARAM;

    // Check if any network interface is up and has an IP
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_is_netif_up(netif)) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            *out_connected = ip_info.ip.addr != 0;
            return ASP_OK;
        }
    }
    *out_connected = false;
    return ASP_OK;
}

// Read the response body into `response` (at most max_len - 1 bytes, then a
// NUL). Reads until the server is done, so a chunked response (no
// Content-Length) arrives too.
static void read_body(esp_http_client_handle_t client, char* response, size_t max_len) {
    if (!response || max_len == 0) return;
    size_t total = 0;
    while (total < max_len - 1) {
        int n = esp_http_client_read(client, response + total, max_len - 1 - total);
        if (n <= 0) break;
        total += n;
    }
    response[total] = '\0';
}

asp_err_t asp_http_get(const char* url, char* response, size_t max_len, int* out_status_code) {
    if (!url || !out_status_code) return ASP_ERR_PARAM;

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
        // HTTPS: trust the certificates the firmware put in the global CA
        // store (esp_tls_set_global_ca_store); plain HTTP ignores this.
        .use_global_ca_store = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ASP_ERR_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return ASP_ERR_FAIL;
    }

    esp_http_client_fetch_headers(client);
    *out_status_code = esp_http_client_get_status_code(client);

    read_body(client, response, max_len);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return ASP_OK;
}

asp_err_t asp_http_post(const char* url, const char* body, char* response, size_t max_len, int* out_status_code) {
    if (!url || !out_status_code) return ASP_ERR_PARAM;

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .use_global_ca_store = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ASP_ERR_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");

    int body_len = body ? strlen(body) : 0;
    esp_err_t err = esp_http_client_open(client, body_len);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return ASP_ERR_FAIL;
    }

    if (body && body_len > 0) {
        esp_http_client_write(client, body, body_len);
    }

    esp_http_client_fetch_headers(client);
    *out_status_code = esp_http_client_get_status_code(client);

    read_body(client, response, max_len);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return ASP_OK;
}
