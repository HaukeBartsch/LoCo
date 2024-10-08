// start a new project
#include <iostream>
#include <format>
#include <filesystem>
#include <chrono>
#include <string.h>
#include "json.hpp"
#include <boost/program_options.hpp>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>
#include "history.hpp"

using namespace std;
using json = nlohmann::json;
using namespace boost::filesystem;
namespace po = boost::program_options;
namespace fs = std::filesystem;

bool verbose = false;
json summaryJSON;


inline bool ends_with(std::string const & value, std::string const & ending) {
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

void addToImportedTime(file_entry_t *entry, int seconds) {
    auto duration = std::chrono::duration<float, std::ratio<1, 1>>(seconds); // minus 60 seconds to force write
    (*entry).last_imported_time = (*entry).last_write_time + std::chrono::duration_cast<std::chrono::seconds>(duration);
}


int main(int argc, char *argv[]) {
    setlocale(LC_NUMERIC, "en_US.utf-8");

    boost::posix_time::ptime timeLocal = boost::posix_time::microsec_clock::local_time();
    summaryJSON["run_date_time"] = to_simple_string(timeLocal);
    

    po::options_description desc("LoCo: Log combiner.\nExample:\n  LoCo --verbose data/*.log\nAllowed options");
    desc.add_options()
      ("help,h", "Print this help.")
      ("version,V", "Print the version number.")
      ("verbose,v", po::bool_switch(&verbose), "Print more verbose output during processing.")
      ("logfiles", po::value< vector<string> >(), "log file")
    ;
    // allow positional arguments to map to logfiles
    po::positional_options_description p;
    p.add("logfiles", -1);

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).
                options(desc).positional(p).run(), vm);
        po::notify(vm);
    } catch(std::exception& e) {
            std::cout << e.what() << "\n";
            return 1;
    }

    if (vm.count("help") || argc <= 1) {
        std::cout << desc << std::endl;
        return 0;
    }

    std::string versionString = std::string("0.0.1.") + boost::replace_all_copy(std::string(__DATE__), " ", ".");
    if (vm.count("version")) {
        fprintf(stdout, "version: %s\n", versionString.c_str());
        return 0;
    }

    summaryJSON["command_line"] = json::array();
    for (int i = 0; i < argc; i++) {
        summaryJSON["command_line"].push_back(std::string(argv[i]));
    }

    // keep the last modification time for each log_files entry

    std::vector< file_entry_t > log_files;
    std::vector<std::string> log_folders;
    if (vm.count("logfiles")) {
        auto rd = vm["logfiles"].as< std::vector< std::string > >();
        for (int i = 0; i < rd.size(); i++) {
            std::string tmp = rd[i];
            if (std::filesystem::is_directory(tmp)) {
                log_folders.push_back(tmp);
            } else if (std::filesystem::is_regular_file(tmp)) {
                //std::filesystem::file_time_type ftime = std::filesystem::last_write_time(tmp);
                file_entry_t entry = { .filename = tmp, .last_write_time = std::filesystem::last_write_time(tmp), .num_imported = 0 };
                addToImportedTime(&entry, -60);
                log_files.push_back(entry);
            } else {
                fprintf(stderr, "Error: logfiles argument is not a valid directory or file [%s]\n", tmp.c_str());
                exit(-1);
            }
        }
    }

    for (int i = 0; i < log_folders.size(); i++) {
        std::string path = log_folders[i];
        for (const auto & entry: fs::recursive_directory_iterator(path)) {
            std::string fn = entry.path().string();
            if (std::filesystem::is_regular_file(fn)) { // ignore directories
                // ignore all files that are not .log
                if (!ends_with(fn, ".log"))
                    continue; // ignore
                //std::filesystem::file_time_type ftime = std::filesystem::last_write_time(fn);
                file_entry_t entry = { .filename = fn, .last_write_time = std::filesystem::last_write_time(fn), .num_imported = 0 };
                addToImportedTime(&entry, -60);
                log_files.push_back(entry);
            }
        }
    }

    // create history and add events to it
    history_t history;
    for (int i = 0; i < log_files.size(); i++) {
        if (verbose)
            fprintf(stderr, "Reading in %s\n", log_files[i].filename.c_str());
        updateHistory(&history, &(log_files[i]));
    }
    // upload yet again (hopefully no duplicates now)
    for (int i = 0; i < log_files.size(); i++) {
        updateHistory(&history, &(log_files[i]));
    }
    if (verbose)
        printHistory(&history); // only works in verbose mode

    // get local history with
    //    std::vector<HistoryEntry> getLocalHistory(history_t *history, int location, int window=3)
    /*std::vector<HistoryEntry> localHistory = getLocalHistory(&history, 15, 3);
    fprintf(stdout, "Start printing specific history entries\n");
    for (int i = 0; i < localHistory.size(); i++) {
        fprintf(stdout, "\t%d %s\n", i-3, localHistory[i].toString().c_str());
    }*/

    std::vector<HistoryEntry> localHistory2 = getLocalHistoryDuration(&history, history.size()/2, 60*60); // seconds around this element
    fprintf(stdout, "Start printing specific local time history entries\n");
    for (int i = 0; i < localHistory2.size(); i++) {
        fprintf(stdout, "\t%d %s\n", i, localHistory2[i].toString().c_str());
    }

    // now print in summary all our files in log_files
    summaryJSON["logs"] = json::array();
    for (int i = 0; i < log_files.size(); i++) {
        json ar = json::object();
        ar["filename"] = log_files[i].filename;
        // this requires C++ version 20
        ar["last_imported_time"] = std::format("{}", log_files[i].last_imported_time);
        ar["last_write_time"] = std::format("{}", log_files[i].last_write_time);
        ar["num_entries"] = log_files[i].linear_event_list.size();
        ar["num_imported"] = log_files[i].num_imported;
        summaryJSON["logs"].push_back(ar);
    }

    // we can import into a memory cache (last lines up to 4096 bytes per sector on disk, look for line endings in there)
    // we would like to get for each file the last couple of events sorted by time, if we read a file again we would like to 
    // add entries but not create duplicates.


    if (verbose) {
        std::string res = summaryJSON.dump(4) + "\n";
        fprintf(stdout, "%s", res.c_str());
    }


    return 0;
}
