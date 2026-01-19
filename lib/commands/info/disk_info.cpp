#include <Windows.h>
#include <errhandlingapi.h>
#include <iostream>

#include "Utils/table.hpp"
#include "Drive/disk.hpp"
#include "Utils/constant_names.hpp"

void print_disks(std::vector<std::shared_ptr<Disk>> disks) {
    std::shared_ptr<utils::ui::Table> disktable = std::make_shared<utils::ui::Table>();
    disktable->set_margin_left(4);

    disktable->add_header_line("Id");
    disktable->add_header_line("Model");
    disktable->add_header_line("Type");
    disktable->add_header_line("Partition");
    disktable->add_header_line("Size");

    for (std::shared_ptr<Disk> disk : disks) {
        disktable->add_item_line(std::to_string(disk->index()));
        disktable->add_item_line(disk->product_id());

        std::string media_type = constants::disk::media_type(disk->geometry()->Geometry.MediaType);
        if (media_type == "Virtual")
        {
            disktable->add_item_line(media_type);
        }
        else
        {
            disktable->add_item_line(media_type + (disk->is_ssd() ? " SSD" : " HDD"));
        }
        disktable->add_item_line(constants::disk::partition_type(disk->partition_type()));
        disktable->add_item_line(std::to_string(disk->size()) + " (" + utils::format::size(disk->size()) + ")");
        disktable->new_line();
    }

    if (disks.size() > 0)
    {
        utils::ui::title("Disks:");
        disktable->render(std::cout);
    }
    else
    {
        std::cout << "No disk found" << std::endl;
    }
}

void print_disk_volumes(std::shared_ptr<Disk> disk) {
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

void print_volume_info(std::shared_ptr<Disk> disk, std::shared_ptr<Volume> vol) {
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