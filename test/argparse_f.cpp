#include <cstdint>
#include <ios>
#include <iostream>
#include <string>

#include "argparse/argparse.hpp"

int main(int argc, char *argv[]) {
    argparse::ArgumentParser program("program_name");

    program.add_argument("--color")
        // .default_value<std::vector<std::string>>({ "orange" })
        // .append()
        .help("specify the cat's fur color")
        .default_value<std::string>("default")
        .scan<'u', uint64_t>();

    try {
        program.parse_args(argc, argv);    // Example: ./main --color red --color green --color blue
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    // auto colors = program.get<std::vector<std::string>>("--color");  // {"red", "green", "blue"}
    std::cout << "is color used:    " << program.is_used("--color") << std::endl;
    std::cout << "is color present: " << program.present<uint64_t>("--color").transform([](int val){return std::to_string(val);}).value_or("not provided") << std::endl;
    std::cout << "the color is:     " << program.get<uint64_t>("--color") << std::endl;

    return 0;
}