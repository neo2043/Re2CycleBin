#include "options.hpp"
#include "Utils/error.hpp"
#include <expected>

std::expected<std::shared_ptr<Disk>,win_error> get_disk(int index) {
	std::shared_ptr<Disk> disk = nullptr;
    {
		if (index >= 0) {
			auto i = core::win::disks::by_index(index);
            if (!i.has_value()) {
                return std::unexpected(i.error().add_to_error_stack("Caller: disk by index error", ERROR_LOC));
            }
            disk = i.value();
		}
	}
    if (disk != nullptr) {
        return disk;
    }
    return std::unexpected(win_error("disk is nullptr",ERROR_LOC));
}