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
#include <regex>

#include "readline/readline.h"
#include "readline/history.h"

using namespace std;
using json = nlohmann::json;
using namespace boost::filesystem;
namespace po = boost::program_options;
namespace fs = std::filesystem;

bool verbose = false;
int numSplits = 6;
int limit = 10;
int minNumberOfObservations = numSplits;
int maxNumberOfPattern = 1000;
std::string cmd("");
json summaryJSON;


inline bool ends_with(std::string const & value, std::string const & ending) {
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

void addToImportedTime(file_entry_t *entry, int seconds) {
    auto duration = std::chrono::duration<float, std::ratio<1, 1>>(seconds); // minus 60 seconds to force write
    (*entry).last_imported_time = (*entry).last_write_time + std::chrono::duration_cast<std::chrono::seconds>(duration);
}

std::vector<std::pair<int, bool> > parseInstruction(std::string line, history_t *history) {
    std::vector< std::pair<int, bool> > result;
    result.push_back(std::make_pair(0, false)); // store location
    result.push_back(std::make_pair(0, false)); // store width
    result.push_back(std::make_pair(0, false)); // store timeUnit (seconds or absolution number)
    
    float flocation = .15;
    float fwidth = 0.004;
    int location, width;
    bool timeUnits = false;

        std::smatch sm;
        const std::regex e ("([0-9.]+)[, ]+([0-9.]+)s");
        std::string line_as_std(line);
        std::regex_match (line_as_std,sm,e);
        if (sm.size() == 3) {
            std::ssub_match sub_match = sm[1];
            sscanf(sub_match.str().c_str(), "%f", &flocation);
            sub_match = sm[2];
            sscanf(sub_match.str().c_str(), "%f", &fwidth);
            //2 == sscanf(line, "%f, %fs", &flocation, &fwidth)) {
            timeUnits = true;
        }
        if (!timeUnits) { // try to read in with an appended -s
            const std::regex e2("([0-9.]+)[, ]+([0-9.]+)");
            std::string line_as_std2(line);
            std::regex_match (line_as_std2,sm,e2);
            if (sm.size() == 3) { // 2 == sscanf(line, "%f, %f", &flocation, &fwidth)) {
                std::ssub_match sub_match = sm[1];
                sscanf(sub_match.str().c_str(), "%f", &flocation);
                sub_match = sm[2];
                sscanf(sub_match.str().c_str(), "%f", &fwidth);
                timeUnits = false;
            } else {
                return result; 
            }
        }

        if (flocation < 0)
            flocation = 1.0f;
        if (flocation < 1.0001f) {
            location = history->size() * flocation;
        } else {
            location = flocation;
        }
        if (fwidth < 0.9999f) {
            width = history->size() * fwidth;
        } else {
            width = (int)fwidth;
        }

        if (location > history->size())
            location = history->size()-1;
        if (width < 1)
            width = 1; // in seconds

    result[0].first = location;
    result[0].second = true;
    result[1].first = width;
    result[1].second = true;
    result[2].second = timeUnits;

    return result;   
}


int main(int argc, char *argv[]) {
    setlocale(LC_NUMERIC, "en_US.utf-8");

    boost::posix_time::ptime timeLocal = boost::posix_time::microsec_clock::local_time();
    summaryJSON["run_date_time"] = to_simple_string(timeLocal);
    

    po::options_description desc("LoCo: Log combiner and event detection.\n\nExample:\n  LoCo --verbose data/*.log\n  >>> 7, 3\n\nA REPL allows you to search for pattern throughout the history. Specify a center log-entry\nas proportion 0..1 or index position if greater than 1 and how many history entries\nas proportion 0..1 or absolute number or '<number>s' for seconds around the location.\n\nAllowed options");
    desc.add_options()
      ("help,h", "Print this help.")
      ("numSplits,s", po::value< int >(&numSplits), "Number of splits used to represent single history as sequences [6].")
      ("limit,l", po::value< int >(&limit), "Limit the maximum distance allowed between log entries [10].")
      ("minNumberOfObservations,m", po::value< int >(&minNumberOfObservations), "An event has to occur at least that many times [3]. Can be set the same as numSplits.")
      ("maxNumberOfPattern,e", po::value< int >(&maxNumberOfPattern), "Some logs can produce a very large number of pattern, stop generating more if you reach this limit [1000].")
      ("cmd,c", po::value< std::string >(&cmd), "Run this command [.5 300].")
      ("version,V", "Print the version number.")
      ("verbose,v", po::bool_switch(&verbose), "Print more verbose output during processing.")
      ("logfiles", po::value< vector<string> >(), "Log files, either folder or list of .log files. We assume that log entries are prefixed with 'Y-m-d H:M:S:'.")
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

    std::string versionString = std::string("0.0.3.") + boost::replace_all_copy(std::string(__DATE__), " ", ".");
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
            fprintf(stderr, "Reading %s\n", log_files[i].filename.c_str());
        updateHistory(&history, &(log_files[i]));
    }
    // upload yet again (hopefully no duplicates now)
    //for (int i = 0; i < log_files.size(); i++) {
    //    updateHistory(&history, &(log_files[i]));
    //}
    if (verbose)
        printHistory(&history);

    // get local history in number of events
    //    std::vector<HistoryEntry> getLocalHistory(history_t *history, int location, int window=3)
    /*std::vector<HistoryEntry> localHistory = getLocalHistory(&history, 15, 3);
    fprintf(stdout, "Start printing specific history entries\n");
    for (int i = 0; i < localHistory.size(); i++) {
        fprintf(stdout, "\t%d %s\n", i-3, localHistory[i].toString().c_str());
    }*/

    // use a REPL for user interaction
    int location = history.size()/2;
    fprintf(stdout, "Instructions: \n\t.5, 0.0004\n");
    add_history(cmd.c_str());
    int width = 360*60;
    std::string text;
    const char *line;
    bool display = false;
    while ((line = readline(">>> ")) != nullptr) {
        //cout << "[" << line << "]" << endl;
        if (line && *line) 
            add_history(line);
        if (std::string(line) == "display") { // toggle the display option
            display = !display;
            fprintf(stdout, "display is now: %s\n", display?"on":"off");
            std::free((void *)line);
            continue; 
        }

        auto parsed = parseInstruction(std::string(line), &history);
        if (!parsed[0].second || !parsed[1].second) {
            fprintf(stdout, "Usage: .5, 0.004s (units of time) or .15, 0.004 (number of entries)\n");
            std::free((void*)line);
            continue; // try again
        }
        location = parsed[0].first;
        width = parsed[1].first;
        bool timeUnits = parsed[2].second;

        fprintf(stdout, "location %d with window ±%d\n", location, width);
        // get local history in seconds around an event
        std::vector<HistoryEntry> localHistory2;
        if (timeUnits) {
            localHistory2 = getLocalHistoryDuration(&history, location, width); // seconds around this element
        } else {
            localHistory2 = getLocalHistory(&history, location, width);
        }
        fprintf(stdout, "Specific local time history entries [location: %d, width: %d]\n", location, width);
        for (int i = 0; i < localHistory2.size(); i++) {
            fprintf(stdout, "  %03d %s\n", i+1, localHistory2[i].toString().c_str());
        }

        // see if we have repeating things
        std::pair< std::vector<std::vector< std::string > >, std::vector<int> > res = detectEvent(&localHistory2, numSplits, limit, minNumberOfObservations, maxNumberOfPattern);
        if (display && res.first.size() > 0) {
            displayPattern(res.first, res.second);
        }
        fprintf(stdout, "↑ location %d/%zu with window ±%d\n", location, history.size(), width);

        // display the results
        //    - use a stable animation: fixed location for individual items that repeat in generated pattern
        //    - use animation to cycle through all local solutions
        //    - use a rhythm to provide contextual order to repeat presentations in the sequence


        std::free((void *)line);
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
