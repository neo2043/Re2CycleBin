// #include <filesystem>
// #include <iostream>

// namespace fs = std::filesystem;

// int main() {
//     fs::path p1{"./some/dir/../file.txt"};
//     fs::path p2 = fs::absolute(p1); // convert to absolute path
//     fs::path pp2 = fs::absolute("./env.nu"); // convert to absolute path

//     fs::path base{"/home/user"};
//     fs::path config = base / "config" / "settings.json";

//     fs::path p{"foo"};
//     p += "bar"; // "foobar"

//     fs::path pp{"foo"};
//     pp.append("bar"); // system-dependent behavior

//     // fs::path p3 = fs::canonical(p1);  // resolves symlinks + removes ., ..

//     std::cout << "p1: " << p1 << std::endl;
//     std::cout << "p2: " << p2 << std::endl;
//     std::cout << "config: " << config << std::endl;
//     std::cout << "p: " << p << std::endl;
//     std::cout << "pp: " << pp << std::endl;
//     std::cout << "pp2: " << pp2 << std::endl;

//     if (fs::exists(pp2)) {
//         std::cout << "exists";
//     } else {
//         std::cout << "does not exists";
//     }
// }

#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

int main() {
    fs::path root = ".\\";

    std::error_code ec;

    for (fs::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec)) {

        if (ec) { 
            std::cerr << "Error accessing: " << it->path() << " (" << ec.message() << ")\n";
            ec.clear();
            continue;
        }

        const fs::path& p = it->path();

        // Skip hidden directories (simple heuristic)
        if (it->is_directory(ec) &&
            p.filename().string().starts_with(".")) 
        {
            it.disable_recursion_pending();
            continue;
        }

        // Only print .cpp/.hpp/.h
        std::cout << p << "\n";
        // if (it->is_regular_file(ec)) {
        //     auto ext = p.extension();
        //     if (ext == ".cpp" || ext == ".hpp" || ext == ".h") {
        //         std::cout << p << "\n";
        //     }
        // }
    }
}
