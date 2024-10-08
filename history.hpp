#include <string>
#include <filesystem>
#include <map>
#include <list>
#include <boost/intrusive/set.hpp>
#include <vector>
#include <functional>
#include <cassert>


extern bool verbose;


typedef struct {
    std::tm t; // when this entry was written (log time)
    std::string originator; // what log file it came from
    std::string type; // INFO, DEBUG, WARN, etc.
    std::string value; // string value of entry
} entry_t;

using namespace boost::intrusive;

// a simple data structure for storing login line information
class HistoryEntry : public set_base_hook<optimize_size<true> > {
    std::tm t_; // when this entry was written (log time)
    std::string originator_; // what log file it came from
    std::string type_; // INFO, DEBUG, WARN, etc.
    std::string value_; // string value of entry

    public:
    set_member_hook<> member_hook_;

    HistoryEntry(std::tm t, std::string originator, std::string type, std::string value) : t_(t), originator_(originator), type_(type), value_(value) {}
    friend bool operator< (const HistoryEntry &a, const HistoryEntry &b) {
        std::tm at = a.t_; // we need a non-const version because mktime will store something inside
        std::tm bt = b.t_;
        // if the two times are equal we should compare the content as well (sort by value string)
        if (std::mktime(&at) == std::mktime(&bt)) {
            return a.value_ < b.value_;
        }
        return std::mktime(&at) < std::mktime(&bt);
    }
    friend bool operator> (const HistoryEntry &a, const HistoryEntry &b) {
        std::tm at = a.t_;
        std::tm bt = b.t_;
        if (std::mktime(&at) == std::mktime(&bt)) {
            return a.value_ > b.value_;
        }
        return std::mktime(&at) > std::mktime(&bt);
    }
    friend bool operator== (const HistoryEntry &a, const HistoryEntry &b) {
        std::tm at = a.t_;
        std::tm bt = b.t_;
        // two events are only equal if they have the same time and the same content (value)
        return (std::mktime(&at) == std::mktime(&bt)) && (a.value_ == b.value_);
    }
    std::string toString() {
        std::stringstream bla; 
        bla << std::put_time(&t_, "%Y-%m-%d %H:%M:%S");
        std::string erg = bla.str() + std::string(": [") + originator_ + std::string("][") + type_ + std::string("] ") + value_;
        return erg;
    }
    std::string getOriginator() {
        return originator_;
    }
    std::string getType() {
        return type_;
    }
    std::string getValue() {
        return value_;
    }
};

typedef set< HistoryEntry, compare<std::greater<HistoryEntry> > > history_t;

// we only need to keep track of path/filename and the last write time
typedef struct {
    std::string filename;
    std::filesystem::file_time_type last_write_time;
    std::filesystem::file_time_type last_imported_time; // initially set this to before the last write time
    std::vector<HistoryEntry> linear_event_list; // store events as they are imported, sorted into a tree in history_t
    int num_imported;

} file_entry_t;

std::pair<std::tm, bool> parseDate(std::string str) {
    std::istringstream ss(str);
    std::tm t = {};
    ss.imbue(std::locale(""));
    ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
    if (ss.fail() || str.size() < 16) {
        // try again with a different format "Sat Sep  7 11:00:04 PM CEST 2024"
        ss = std::istringstream(str);
        t = {};
        ss >> std::get_time(&t, "%a %b %d %I:%M:%S %p %Z %Y");  // this fails because %Z is not part of get_time, at least for CEST
        if (ss.fail() || str.size() < 16) {
            // skip this event, could not read the log time
            //if (verbose)
            //    fprintf(stdout, "PARSE FAILED: \"%s\"\n", str.c_str());
            return std::make_pair(std::tm{}, false);
        }
    }
    return std::make_pair(t, true);
}

bool addEntry(std::vector<HistoryEntry> *values, std::string line, std::string originator) {
    // lets parse the date field and the type fields
    // this is tricky because the format for unstructured logs is not 'nice'
    //fprintf(stdout, "BLA: \"%s\"\n", line.substr(0,16).c_str());
    // line = std::string("Sat Sep  7 11:00:04 PM CEST 2024");
    std::string type = "UNKNOWN";
    if (line.find("INFO") != std::string::npos) {
        type = "INFO";
    }
    if (line.find("DEBUG") != std::string::npos) {
        type = "DEBUG";
    }
    if (line.find("ERROR") != std::string::npos) {
        type = "ERROR";
    }
    if (line.find("WARNING") != std::string::npos) {
        type = "WARNING";
    }

    std::pair<std::tm, bool> ret = parseDate(line);
    if (!ret.second) {
        // TODO: failed to detect the date for this line, we could use the last modification time as a worst case thing here?
        // for now just ignore this line
        return false; // do nothing
    }
    std::tm t = ret.first;
    // parsed time is now:
    //std::stringstream bla; 
    //bla << std::put_time(&t, "%c");
    //if (verbose)
    //    fprintf(stdout, "WORKING %s line from %s to add is: %s\n", bla.str().c_str(), originator.c_str(), line.c_str());

    //std::vector<HistoryEntry> values;
    values->push_back(HistoryEntry(t, originator, type, line));
    return true;
}

// how many bytes will we read from the end of the file?
#define READSIZE (2000*4096)
std::vector<std::string> spinner = std::vector<std::string>{"⣾ ", "⣽ ", "⣻ ", "⢿ ", "⡿ ", "⣟ ", "⣯ ", "⣷ "};

void updateHistory(history_t *history, file_entry_t *log_file) {
    // check if we need to open this file
    std::filesystem::file_time_type last_write_time = std::filesystem::last_write_time(log_file->filename);
    if (log_file->last_imported_time >= last_write_time)
        return; // nothing to be done
    // read the last 4096 byte from log_file.filename
    FILE *fp;
    int lastXBytes = READSIZE; // should correspond to block size on disk, but no alignment!    

    if ((fp = fopen(log_file->filename.c_str(), "rb")) == NULL) {
        fprintf(stderr, "Error: could not open file %s\n", log_file->filename.c_str());
        return;
    }
    fseek(fp, 0L, SEEK_END);
    int fileSize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    if (fileSize > lastXBytes) {
        fseek(fp, -lastXBytes, SEEK_END);
    }
    char *buf = new char[READSIZE];
    int bytesRead = fread(buf, sizeof(char), lastXBytes, fp);
    fclose(fp);
    //std::string lines = std::string(buf, bytesRead);
    std::stringstream ss(std::string(buf, READSIZE));
    std::string line;
    // we should ignore the first line in case we have a large file,
    if (fileSize > lastXBytes)
        std::getline(ss, line, '\n'); // read and ignore the first line, should be a partial line

    // collect all the events in log_file->linear_event_list
    int numLinesParsedNow = 0;
    int numLinesNotParsedNow = 0;
    if (verbose)
        fprintf(stdout, "\n");
    while (std::getline(ss, line,'\n')) { // it should be required to create a new string here.
        // process line now
        if (addEntry(&log_file->linear_event_list, line, log_file->filename))
            numLinesParsedNow++;
        else
            numLinesNotParsedNow++;
        if (verbose && ((numLinesParsedNow + numLinesNotParsedNow) % 100) == 0) {
            int spinner_c = numLinesParsedNow + numLinesNotParsedNow;
            // ("\033[A\033[2K\033[94;49m%s%d\033[37m [%.0f files / s] P %d S %d S %d [S %d]\033[39m\033[49m\n", spinner[(spinner_c)%len(spinner)], counter, (float64(counter))/time.Since(startTime).Seconds(), numPatients, numStudies, numSeries, counterError)
            fprintf(stdout, "\033[A\033[2K\033[94;49m%s%d\033[37m [parsed %d lines, (skipped %d)]\033[39m\033[49m\n", spinner[(spinner_c/100)%(spinner.size())].c_str(), spinner_c, numLinesParsedNow, numLinesNotParsedNow);
        }
    }
    // add them to the history (should be sorted now)
    typedef std::vector<HistoryEntry>::iterator VectIt;
    for (VectIt it(log_file->linear_event_list.begin()), itend(log_file->linear_event_list.end()); it != itend; ++it) {
        // we should insert if its not already in there (same date/time and value)
        auto done = history->insert(*it);
        if (done.second)
            log_file->num_imported++;
    }

    // now update the log_file time entry
    log_file->last_imported_time = last_write_time; // safe as now time instead?
    return;
}

// Print out the whole history, leave nothing out.
void printHistory(history_t *history) {
    history_t::reverse_iterator rbit(history->rbegin());
    for (int i = 0; rbit != history->rend(); ++rbit, i++) {
        fprintf(stdout, "H-%02d %s\n", i, (*rbit).toString().c_str());
    }
}


// find neighbors using https://www.geeksforgeeks.org/convert-binary-tree-to-circular-doubly-linked-list-using-linear-extra-space/?ref=oin_asr7

// Print out a specific section of the history.
std::vector<HistoryEntry> getLocalHistory(history_t *history, int location, int window=3) {
    std::vector<HistoryEntry> entries;
    history_t::iterator here(history->begin());

    if (location < 0)
        location = history->size() - location;

    if (window < 0)
        window = -window;

    if (history->size() <= location + window)
        return entries;
    if (history->size()-window < 0)
        return entries;

    std::advance(here, location - window);
    for (int i = 0; i < window*2 + 1; i++) {
        entries.push_back(*here);
        ++here;
    }

    return entries;
}