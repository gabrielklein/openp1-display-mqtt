#pragma once
#include "esp_http_server.h"
namespace openp1::network::detail {
bool stream_start();
esp_err_t stream_open(httpd_req_t* request);
// One real worker iteration; exposed for deterministic host transport tests.
void stream_tick();
}
