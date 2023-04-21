// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <libhal-esp8266/at/socket.hpp>
#include <libhal-esp8266/at/wlan_client.hpp>
#include <libhal-esp8266/util.hpp>
#include <libhal-util/serial.hpp>
#include <libhal-util/steady_clock.hpp>
#include <libhal/timeout.hpp>

#include "../hardware_map.hpp"
#include "helper.hpp"

hal::status application(hal::esp8266::hardware_map& p_map)
{
  using namespace std::chrono_literals;
  using namespace hal::literals;

  auto& counter = *p_map.counter;
  auto& esp = *p_map.esp;
  auto& debug = *p_map.debug;

  HAL_CHECK(hal::write(debug, "ESP8266 WiFi Client Application Starting...\n"));

  // 8kB buffer to read data into
  std::array<hal::byte, 8192> buffer{};

  // Connect to WiFi AP
  auto wlan_client_result = hal::esp8266::at::wlan_client::create(
    esp, "ssid", "password", HAL_CHECK(hal::create_timeout(counter, 20s)));

  // Return error and restart
  if (!wlan_client_result) {
    HAL_CHECK(hal::write(debug, "Failed to create wifi client!\n"));
    return wlan_client_result.error();
  }

  // Create a tcp_socket and connect it to example.com port 80
  auto wlan_client = wlan_client_result.value();
  auto tcp_socket_result = hal::esp8266::at::socket::create(
    wlan_client,
    HAL_CHECK(hal::create_timeout(counter, 10s)),
    {
      .type = hal::socket::type::tcp,
      .domain = "example.com",
      .port = "80",
    });

  if (!tcp_socket_result) {
    HAL_CHECK(hal::write(debug, "TCP Socket couldn't be established\n"));
    return tcp_socket_result.error();
  }

  // Move tcp_socket out of the result object
  auto tcp_socket = std::move(tcp_socket_result.value());

  while (true) {
    // Minimalist GET request to example.com domain
    static constexpr std::string_view get_request = "GET / HTTP/1.1\r\n"
                                                    "Host: example.com:80\r\n"
                                                    "\r\n";
    // Fill buffer with underscores to determine which blocks aren't filled.
    buffer.fill('.');

    // Send out HTTP GET request
    HAL_CHECK(hal::write(debug, "Sending:\n\n"));
    HAL_CHECK(hal::write(debug, get_request));
    HAL_CHECK(hal::write(debug, "\n\n"));
    HAL_CHECK(tcp_socket.write(hal::as_bytes(get_request),
                               HAL_CHECK(hal::create_timeout(counter, 500ms))));

    // Wait 1 second before reading response back
    HAL_CHECK(hal::delay(counter, 1000ms));

    // Read response back from serial port
    auto received = HAL_CHECK(tcp_socket.read(buffer)).data;

    HAL_CHECK(print_http_response_info(debug, to_string_view(received)));
  }

  return hal::success();
}
