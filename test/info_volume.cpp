#include <Windows.h>
#include <errhandlingapi.h>
#include <iostream>

#include "Utils/error.hpp"
#include "Utils/options.hpp"
#include "Drive/disk.hpp"
#include "Utils/constant_names.hpp"

void print_hardware_volume(std::shared_ptr<Disk> disk, std::shared_ptr<Volume> vol) {
    utils::ui::title("Info for " + disk->name() + " > Volume:" + std::to_string(vol->index()));

    if (vol->serial_number())
    {
        std::cout << "Serial Number  : ";
        std::cout << utils::format::hex(vol->serial_number() >> 16) << "-" << utils::format::hex(vol->serial_number() & 0xffff) << std::endl;
    }
    if (vol->filesystem().length() > 0)
    {
        std::cout << "Filesystem     : " << vol->filesystem() << std::endl;
    }
    if (vol->partition_type() == PARTITION_STYLE_MBR)
    {
        std::cout << "Bootable       : " << (vol->bootable() ? "True" : "False") << std::endl;
    }
    if (vol->partition_type() == PARTITION_STYLE_GPT)
    {
        std::cout << "GUID           : " << vol->guid_type() << std::endl;
    }
    std::cout << "Type           : " << constants::disk::drive_type(vol->type()) << std::endl;
    if (vol->label().length() > 0)
    {
        std::cout << "Label          : " << vol->label() << std::endl;
    }
    std::cout << "Offset         : " << vol->offset() << " (" << utils::format::size(vol->offset()) << ")" << std::endl;
    std::cout << "Size           : " << vol->size() << " (" << utils::format::size(vol->size()) << ")" << std::endl;
    std::cout << "Free           : " << vol->free() << " (" << utils::format::size(vol->free()) << ")" << std::endl;

    if (vol->is_mounted())
    {
        std::cout << " (" << utils::strings::join_vec(vol->mountpoints(), ", ") << ")";
    }
    std::cout << std::endl;
    std::cout << "Bitlocker      : " << (vol->bitlocker().bitlocked ? "True" : "False");
    if (vol->bitlocker().bitlocked)
    {
        std::cout << " (" << (vol->filesystem().length() == 0 ? "Locked" : "Unlocked") << ")";
    }
    std::cout << std::endl;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);

    if (!utils::processes::elevated(GetCurrentProcess())) {
        std::cerr << "Administrator rights are required to read physical drives" << std::endl;
        return 1;
    }

    std::ios_base::fmtflags flag_backup(std::cout.flags());
	std::cout << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" << std::endl << std::endl;
    auto i = get_disk(0);
	if (!i.has_value()) {
		std::cout << i.error();
    }
	std::shared_ptr<Disk> disk = i.value();
	std::shared_ptr<Volume> volume = disk->volumes(1);
	print_hardware_volume(disk, volume);
	std::cout << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" << std::endl << std::endl;
	
    i = get_disk(1);
	if (!i.has_value()) {
		std::cout << i.error();
    }
	disk = i.value();
	volume = disk->volumes(1);
	print_hardware_volume(disk, volume);
	std::cout << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" << std::endl << std::endl;
	
    i = get_disk(1);
	if (!i.has_value()) {
		std::cout << i.error();
    }
	disk = i.value();
	volume = disk->volumes(2);
	print_hardware_volume(disk, volume);
	std::cout << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" << std::endl << std::endl;
	
    i = get_disk(1);
	if (!i.has_value()) {
		std::cout << i.error();
    }
	disk = i.value();
	volume = disk->volumes(3);
	print_hardware_volume(disk, volume);
	std::cout << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" << std::endl << std::endl;
	
    i = get_disk(1);
	if (!i.has_value()) {
		std::cout << i.error();
    }
	disk = i.value();
	volume = disk->volumes(4);
	print_hardware_volume(disk, volume);
	std::cout << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" << std::endl << std::endl;

    std::cout.flags(flag_backup);
}
