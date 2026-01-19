#include <Windows.h>
#include <errhandlingapi.h>
#include <iostream>

#include "Utils/error.hpp"
#include "Utils/options.hpp"
#include "Utils/table.hpp"
#include "Drive/disk.hpp"
#include "Utils/constant_names.hpp"

void print_hardware_disk(std::shared_ptr<Disk> disk) {
    utils::ui::title("Info for disk: " + disk->name());

    std::cout << "    Model       : " << disk->product_id() << std::endl;
    std::cout << "    Version     : " << disk->product_version() << std::endl;
    std::cout << "    Serial      : " << disk->serial_number() << std::endl;
    std::cout << "    Media Type  : " << constants::disk::media_type(disk->geometry()->Geometry.MediaType) << (disk->is_ssd() ? " SSD" : " HDD") << std::endl;
    std::cout << "    Size        : " << disk->size() << " (" << utils::format::size(disk->size()) << ")" << std::endl;
    std::cout << "    Geometry    : " << std::to_string(disk->geometry()->Geometry.BytesPerSector) << " bytes * " << std::to_string(disk->geometry()->Geometry.SectorsPerTrack) << " sectors * " << std::to_string(disk->geometry()->Geometry.TracksPerCylinder) << " tracks * " << std::to_string(disk->geometry()->Geometry.Cylinders.QuadPart) << " cylinders" << std::endl;
    std::cout << "    Partition   : " << constants::disk::partition_type(disk->partition_type()) << std::endl;
    std::cout << std::endl;

    std::shared_ptr<utils::ui::Table> partitions = std::make_shared<utils::ui::Table>();
    partitions->set_margin_left(4);
    partitions->add_header_line("Id");
    switch (disk->partition_type()) {
    case PARTITION_STYLE_MBR:
    {
        partitions->add_header_line("Boot");
        break;
    }
    case PARTITION_STYLE_GPT:
    {
        partitions->add_header_line("Type");
        break;
    }
    default:
        break;
    }
    partitions->add_header_line("Label");
    partitions->add_header_line("Mounted");
    partitions->add_header_line("Filesystem");
    partitions->add_header_line("Offset");
    partitions->add_header_line("Size");

    for (std::shared_ptr<Volume> volume : disk->volumes()) {
        partitions->add_item_line(std::to_string(volume->index()));

        switch (disk->partition_type()) {
        case PARTITION_STYLE_MBR:
        {
            if (volume->bootable()) partitions->add_item_line("Yes");
            else partitions->add_item_line("No");
            break;
        }
        case PARTITION_STYLE_GPT:
        {
            partitions->add_item_line(volume->guid_type());
            break;
        }
        default:
            break;
        }

        partitions->add_item_line(volume->label());
        partitions->add_item_line(utils::strings::join_vec(volume->mountpoints(), ", "));
        partitions->add_item_line(volume->filesystem());
        partitions->add_item_line(utils::format::hex(volume->offset()));
        partitions->add_item_line(utils::format::hex(volume->size()) + " (" + utils::format::size(volume->size()) + ")");

        partitions->new_line();
    }

    partitions->render(std::cout);
    std::cout << std::endl;
}

// print volume
int main() {
    SetConsoleOutputCP(CP_UTF8);

    if (!utils::processes::elevated(GetCurrentProcess()))
    {
        std::cerr << "Administrator rights are required to read physical drives" << std::endl;
        return 1;
    }

    auto i = get_disk(0);
    if (!i.has_value()) {
        std::cout << i.error();
    }
    print_hardware_disk(i.value());
    
    i = get_disk(1);
    if (!i.has_value()) {
        std::cout << i.error();
    }
    print_hardware_disk(i.value());
}
