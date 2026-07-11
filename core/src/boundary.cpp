#include "chronos/boundary.hpp"

namespace chronos {

std::string round_trip_hello_event(std::string_view payload) {
  std::string response;
  response.reserve(payload.size() + 31);
  response.append("chronos.boundary.v0|hello_ack|");
  response.append(payload);
  return response;
}

} // namespace chronos
