#pragma once

#include "Utils/error.hpp"
#include "Drive/disk.hpp"

#include <expected>
#include <string>
#include <cstdint>
#include <memory>

std::expected<std::shared_ptr<Disk>,win_error> get_disk(int index);