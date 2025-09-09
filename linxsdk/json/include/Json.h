#pragma once

#include "json.hpp"

namespace linx {
using json = nlohmann::json;

using json_exception = nlohmann::detail::exception;

}  // namespace linx
