#pragma once

#include "disk.hpp"
#include "Utils/error.hpp"

enum class VirtualDiskType { TrueCrypt, VeraCrypt, Dummy };

class VirtualDisk final : public Disk {
    public:
        VirtualDisk(VirtualDiskType type, PWCHAR volume_name);
        ~VirtualDisk();

        // std::expected<void, win_error> add_volume_image(std::string filename);
        virtual bool is_virtual() const { return true; }
};


namespace core {
	namespace win {
		namespace virtualdisk {
			std::expected<std::vector<std::shared_ptr<Disk>>,win_error> list();
		}
	}
}
