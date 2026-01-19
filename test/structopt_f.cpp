#include <numeric>
#include <structopt/app.hpp>

enum class LogLevel { debug, info, warn, error, critical };

struct Options {
   // positional argument
   //   e.g., ./main <file>
   std::string config_file;

   // optional argument
   //   e.g., -b "192.168.5.3"
   //   e.g., --bind_address "192.168.5.3"
   //
   // options can be delimited with `=` or `:`
   // note: single dash (`-`) is enough for short & long option
   //   e.g., -bind_address=localhost
   //   e.g., -b:192.168.5.3
   //
   // the long option can also be provided in kebab case:
   //   e.g., --bind-address 192.168.5.3
   std::optional<std::string> bind_address;
 
   // flag argument
   // Use `std::optional<bool>` and provide a default value. 
   //   e.g., -v
   //   e.g., --verbose
   //   e.g., -verbose
   std::optional<bool> verbose = false;

   // directly define and use enum classes to limit user choice
   //   e.g., --log-level debug
   //   e.g., -l error
   std::optional<LogLevel> log_level = LogLevel::info;

   // pair argument
   // e.g., -u <first> <second>
   // e.g., --user <first> <second>
   std::optional<std::pair<std::string, std::string>> user;

   // use containers like std::vector
   // to collect "remaining arguments" into a list
   std::vector<std::string> files;
};

STRUCTOPT(Options, config_file, bind_address, verbose, log_level, user, files);

std::string log_level_to_string(LogLevel arg) {
  switch (arg) {
    case (LogLevel::info):
      return "Info";
    case (LogLevel::critical):
      return "Critical";
    case (LogLevel::debug):
      return "Debug";
    case (LogLevel::error):
      return "Error";
    case (LogLevel::warn):
      return "Warn";
    default:
      return "Not provided";
  }
}

int main(int argc, char *argv[]) {
  Options options;

  try {
  
    // Line of code that does all the work:
    options = structopt::app("my_app").parse<Options>(argc, argv);

    // Print out parsed arguments:
    std::cout << "config_file  = " << options.config_file << "\n";
    std::cout << "bind_address = " << options.bind_address.value_or("not provided") << "\n";
    std::cout << "verbose      = " << std::boolalpha << options.verbose.value() << "\n";
    std::cout << "log_level    = " << log_level_to_string(options.log_level.value()) << "\n";
    std::cout << "user         = " << (options.user.has_value() ? options.user.value().first + options.user.value().second : "Not Provided") << "\n";
    std::cout << "files        = " << std::reduce(options.files.begin(),options.files.end(), std::string(),[](std::string prev, std::string curr){
      return prev == "" ? curr : prev+", "+curr;
    }) << "\n";

    
  } catch (structopt::exception& e) {
    std::cout << e.what() << "\n";
    // std::cout << e.help();
  }

}