#include <Windows.h>
#include <errhandlingapi.h>
#include <iostream>

#include "Utils/error.hpp"
#include "Utils/table.hpp"
#include "Drive/disk.hpp"
#include "Utils/constant_names.hpp"

void print_disks_short(std::vector<std::shared_ptr<Disk>> disks) {
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

int main() {
    SetConsoleOutputCP(CP_UTF8);

    if (!utils::processes::elevated(GetCurrentProcess()))
    {
        std::cerr << "Administrator rights are required to read physical drives" << std::endl;
        return 1;
    }

    auto i = core::win::disks::list();
    if (!i.has_value()) {
        std::cout << i.error();
    }

    print_disks_short(core::win::disks::list().value());
}
