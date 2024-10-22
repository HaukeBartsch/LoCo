#include <string>
#include <filesystem>
#include <map>
#include <list>
#include <boost/intrusive/set.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <functional>
#include <cassert>
#include "backend/seq2pat.hpp"
#include <ncurses.h>

extern bool verbose;


typedef struct {
    std::tm t; // when this entry was written (log time)
    std::string originator; // what log file it came from
    std::string type; // INFO, DEBUG, WARN, etc.
    std::string value; // string value of entry
} entry_t;

using namespace boost::intrusive;




#define min3(a,b,c) std::min(std::min(a, b), c) 

int editDistanceRecursive(std::string str1, std::string str2, int n, int m) {
    // If str1 is empty, insert all characters of str2
    if (m == 0)
        return n;
    // If str2 is empty, remove all characters of str1
    if (n == 0)
        return m;

    // If the last characters match, move to the next pair
    if (str1[m - 1] == str2[n - 1])
        return editDistanceRecursive(str1, str2, m - 1,
                                     n - 1);

    // If the last characters don't match, consider all
    // three operations
    return 1
           + min3(editDistanceRecursive(str1, str2, m,
                                       n - 1), // Insert
                 editDistanceRecursive(str1, str2, m - 1,
                                       n), // Remove
                 editDistanceRecursive(str1, str2, m - 1,
                                       n - 1) // Replace
           );
}

int editDistDP(const std::string &s1, const std::string &s2) {
  
    int m = s1.length();
    int n = s2.length();
    int largestAllowedDifference = 5;

    // Create a table to store results of subproblems
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

    // Fill the known entries in dp[][]
    // If one string is empty, then answer 
    // is length of the other string
    for (int i = 0; i <= m; i++) 
        dp[i][0] = i;
    for (int j = 0; j <= n; j++) 
        dp[0][j] = j; 

    // Fill the rest of dp[][]
    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (s1[i - 1] == s2[j - 1])
                dp[i][j] = dp[i - 1][j - 1];
            else
                dp[i][j] = 1 + std::min({dp[i][j - 1],  
                                 dp[i - 1][j],   
                                 dp[i - 1][j - 1]});
            //if (dp[i][j] > largestAllowedDifference)
            //    return dp[i][j];
        }
    }

    return dp[m][n];
}

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
        time_t tat = std::mktime(&at); // in case this is expensive to it only once
        time_t tbt = std::mktime(&bt);
        // if the two times are equal we should compare the content as well (sort by value string)
        if (tat == tbt) { // std::mktime(&at) == std::mktime(&bt)) {
            // TODO: we could add sorting here for events coming in from different originators (at the same time)
            if (a.value_ == b.value_)
                return a.originator_ < b.originator_;
            return a.value_ < b.value_;
        }
        return tat < tbt; // std::mktime(&at) < std::mktime(&bt);
    }
    friend bool operator> (const HistoryEntry &a, const HistoryEntry &b) {
        std::tm at = a.t_;
        std::tm bt = b.t_;
        time_t tat = std::mktime(&at); // in case this is expensive to it only once
        time_t tbt = std::mktime(&bt);
        if (tat == tbt) { // std::mktime(&at) == std::mktime(&bt)) {
            if (a.value_ == b.value_)
                return a.originator_ > b.originator_;
            return a.value_ > b.value_;
        }
        return tat > tbt; // std::mktime(&at) > std::mktime(&bt);
    }
    friend bool operator== (const HistoryEntry &a, const HistoryEntry &b) {
        std::tm at = a.t_;
        std::tm bt = b.t_;
        time_t tat = std::mktime(&at); // in case this is expensive to it only once
        time_t tbt = std::mktime(&bt);
        // two events are only equal if they have the same time and the same content (value)
        return (tat == tbt) && (a.value_ == b.value_) && (a.originator_ == b.originator_);
        //return (std::mktime(&at) == std::mktime(&bt)) && (a.value_ == b.value_);
    }
    std::string toString() {
        std::stringstream bla; 
        bla << std::put_time(&t_, "%Y-%m-%d %H:%M:%S");
        std::string erg = bla.str() + std::string(": [") + originator_ + std::string("][") + type_ + std::string("] ") + value_;
        return erg;
    }
    std::string getOriginator() {
        // instead return the filename only (assume that path is not important)
        boost::filesystem::path p(originator_);
        return p.stem().c_str();
    }
    std::string getType() {
        return type_;
    }
    std::string getValue() {
        return value_;
    }
    std::tm getTime() {
        return t_;
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

std::tuple<std::tm, bool, std::string> parseDate(std::string str) {
    // split string into date (beginning) and rest
    int spaces = 0;
    std::string front("");
    std::string rest("");
    for (int i = 0; i < str.size(); i++) {
        if (str[i] == ' ')
            spaces++;
        if (spaces == 2) {
            front = str.substr(0, i);
            rest = str.substr(i+1); //  + std::string("\"") + str + std::string("\"");
            boost::trim_left(rest);
            break;
        }
    }
    if (front.size() == 0) {
        return std::make_tuple(std::tm{}, false, std::string());
    }

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
            return std::make_tuple(std::tm{}, false, std::string());
        }
    }
    return std::make_tuple(t, true, rest);
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

    std::tuple<std::tm, bool, std::string> ret = parseDate(line);
    if (!std::get<1>(ret)) {
        // TODO: failed to detect the date for this line, we could use the last modification time as a worst case thing here?
        // for now just ignore this line
        return false; // do nothing
    }
    std::tm t = std::get<0>(ret);
    // parsed time is now:
    //std::stringstream bla; 
    //bla << std::put_time(&t, "%c");
    //if (verbose)
    //    fprintf(stdout, "WORKING %s line from %s to add is: %s\n", bla.str().c_str(), originator.c_str(), line.c_str());

    //std::vector<HistoryEntry> values;
    values->push_back(HistoryEntry(t, originator, type, std::get<2>(ret)));
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
            fprintf(stdout, "\033[A\033[2K\033[94;49m%s%d\033[37m parsed %d lines, skipped %d\033[39m\033[49m\n", spinner[(spinner_c/100)%(spinner.size())].c_str(), spinner_c, numLinesParsedNow, numLinesNotParsedNow);
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
        fprintf(stdout, "H-%02d %s\n", i+1, (*rbit).toString().c_str());
    }
}

// Print out a specific section of the history with a symmetric window.
std::vector<HistoryEntry> getLocalHistory(history_t *history, int location, int window=3) {
    std::vector<HistoryEntry> entries;
    history_t::reverse_iterator here(history->rbegin());
    // history_t::iterator here(history->begin());

    if (location < 0)
        location = history->size() - location;

    if (window < 0)
        window = -window;

    if (history->size() <= location + window) {
        // do the best we can
        window = history->size() - location - 1;
    }
    if (history->size()-window < 0) {
        // do the best we can
        window = location - 1;
    }

    std::advance(here, location - window);
    for (int i = 0; i < window*2 + 1; i++) {
        entries.push_back(*here);
        ++here;
    }

    return entries;
}

// Print out a specific section of the history.
std::vector<HistoryEntry> getLocalHistoryDuration(history_t *history, int location, int secondsAroundLocation=(60*24)) {
    std::vector<HistoryEntry> entries;
    history_t::iterator here(history->begin());

    if (location < 0)
        location = history->size() - location;

    std::advance(here, location);
    std::tm mid_time = (*here).getTime();
    mid_time.tm_sec -= secondsAroundLocation;
    std::mktime(&mid_time); // should fix the overflow due to seconds subtracted
    // go backwards
    HistoryEntry stop(mid_time, std::string(""), std::string(""), std::string(""));
    while (here != history->end() && stop < *here) {
        entries.insert(entries.begin(), *here);
        here++;
    }
    here = history->begin();
    std::advance(here, location);
    mid_time = (*here).getTime();
    mid_time.tm_sec += secondsAroundLocation+1;
    std::mktime(&mid_time); // should fix the overflow due to seconds added
    // skip the mid location, already done in the previous loop
    here = history->begin();
    std::advance(here, location-1);

    // go backwards
    stop = HistoryEntry(mid_time, std::string(""), std::string(""), std::string(""));
    while (here != history->begin() && stop > *here) {
        entries.push_back(*here);
        here--;
    }

    return entries;
}

// do a simple alignment and compute the best matching fit
std::vector<int> computePatternShift(std::vector< std::vector<int> > erg) {
    std::vector<int> result(erg.size());
    if (erg.size() == 0)
        return result;

    int bestShift = 0;
    int bestShiftVal = 0;
    result[0] = 0; // no shift with itself
    for (int i = 1; i < erg.size(); i++) { // next pattern
        // shift to the next pattern
        int j = 0; // always compare with first pattern
        int bestShift = 0;
        float bestSumChange = 0.0;
        int startShift = -(int)(erg[j].size()-1)/2;
        for (int shift = startShift; shift < (int)erg[i].size()-1; shift++) { // do not count same pattern
            int sumChange = 0;
            int comparisons = 0;
            for (int c = 0; c < erg[j].size()-1; c++) {
                int idx = c + shift;
                if (idx < 0)
                    continue;
                if (idx > erg[i].size()-1)
                    continue;
                int a = erg[i][idx];
                int b = erg[j][c];
                sumChange += (a!=b?1:0);
                comparisons++;
            }
            float sc = sumChange/(comparisons>0?(float)comparisons:1.0f);
            if (shift == startShift) { // init the values
                bestShift = shift;
                bestSumChange = sc;
            } else {
                if (bestSumChange > sc) {
                    bestShift = shift;
                    bestSumChange = sc;
                }
            }
        }
        result[i] = bestShift;
    }
    return result;
}


// Find unique sequences of events that repeat at least minNumberObservations times.
// - numSplits[3]: split the single long history into equal length chunks of repeating events
// - limit[20]: maximum allowed distance between log entries (in merged log history)
// - minNumberOfObservations[3]: can be set the same as numSplits
std::pair<std::vector<std::vector< std::string > >, std::vector<int> > detectEvent(std::vector<HistoryEntry> *horizon, int numSplits = 3, int limit = 20, int minNumberObservations = 3, int maxNumberOfPattern = 10000) {
    // return a number of events that happen more than once
    std::vector<std::vector<std::string> > events;
    std::vector<std::string> repeating_events_list;

    // create a list of repeating events (based on string comparisons)
    // if an event does not repeat at least 2 times its not an event
    struct cmpByEditDistance {
        bool operator()(const std::string& a, const std::string& b) const {
            return a < b;
            //return editDistDP(a, b);
            // return editDistanceRecursive(a, b, a.size(), b.size());
            // return a.length() < b.length();
        }
    };

    bool valuePlusOriginator = true;

    std::map<std::string, int, cmpByEditDistance> repeatingEvents;
    for (int i = 0; i < horizon->size(); i++) {
        std::string v = (*horizon)[i].getValue();
        if (valuePlusOriginator) 
            v += std::string(" [") + (*horizon)[i].getOriginator() + std::string("]");
        if (repeatingEvents.find( v ) == repeatingEvents.end())
            repeatingEvents.insert(std::pair<std::string, int>(v, 0));
        repeatingEvents.insert(std::pair<std::string, int>(v, ++repeatingEvents[v]));
    }
    auto it = repeatingEvents.begin();
    while (it != repeatingEvents.end()) {
        if ((*it).second > 1)
            repeating_events_list.push_back((*it).first);
        it++;
    }
    if (verbose)
        fprintf(stdout, "%zu unique event%s, repeating events in this batch: %zu\n", repeatingEvents.size(), (repeatingEvents.size()!=1?"s":""), repeating_events_list.size());

    // create an alternative history based on our repeating events only, store position in repeating_events_list as identity of the string
    std::vector<std::vector<int> > alternativeHistory;
    std::vector<std::vector<int> > idx_attr;
    bool testing = false;
    int L = 0; // max value in history
    int splits = numSplits; // make 3 sequences out of history, store in alternativeHistory
    int counter = 0;
    // split the history into separate pieces
    int half = horizon->size()/splits;
    for (int split = 0; split < splits; split++) {
        int start = split * half;
        int end = (split + 1) * half;
        if (split == splits-1)
            end = horizon->size();
        alternativeHistory.push_back(std::vector<int>()); // we have only a single history here, we could have more for parallel processing?
        idx_attr.push_back(std::vector<int>());
        for (int i = start; i < end; i++) {
            std::string v = (*horizon)[i].getValue();
            if (valuePlusOriginator)
                v += std::string(" [") + (*horizon)[i].getOriginator() + std::string("]");
            auto it = std::find(repeating_events_list.begin(), repeating_events_list.end(), v);
            if (it != repeating_events_list.end()) {
                int idx = it - repeating_events_list.begin();
                alternativeHistory[split].push_back(idx+1);
                if (L < idx+1)
                    L = idx+1;
                idx_attr[split].push_back(counter++); // store a time, TODO: what happens if we do not find the event in the list, in that case i gets updated and we have a gap here, best to use a separate counter.. 
            }
        }
    }

    // now use SPMF to find pattern
    // maybe easier to use the default: https://github.com/aminhn/HTMiner/blob/main/BDTrie/load_inst.cpp
    patterns::Seq2pat algo = patterns::Seq2pat();
    algo.M = alternativeHistory[0].size(); // Length of the largest sequence in items
    for (int i = 1; i < alternativeHistory.size(); i++)
        if (algo.M < alternativeHistory[i].size())
            algo.M = alternativeHistory[i].size();
    algo.N = alternativeHistory.size(); // Number of sequences in items
    algo.L = L; //repeating_events_list.size(); // Maximum value in events list (number of repeating events)
    algo.items = alternativeHistory;
    algo.theta = minNumberObservations; // algo.M * 0.00001; // 2; // at least observe twice
    // no attributes, no constrains?
    algo.tot_gap.push_back(1); // not sure why we define this... its needed to have the ugap test apply
    algo.attrs.push_back(idx_attr);
    //algo.lgapi.push_back(0); // which attribute to use
    //algo.lgap.push_back(limit); // try to fix this again
    algo.ugapi.push_back(0); // what attr value to use
    algo.ugap.push_back(limit); // max distance in number of entries between log entries (speed improvement)
    algo.max_number_of_pattern = maxNumberOfPattern;
    std::vector< std::vector<int> > erg = algo.mine();

    // erg contains our pattern, print those now
    if (erg.size() == 0) {
        fprintf(stdout, "\033[31mNo pattern detected...\033[0m\n");
    }
    std::vector<int> patternShift = computePatternShift(erg);

    for (int i = 0; i < erg.size(); i++) {
        events.push_back(std::vector<std::string>());
        fprintf(stdout, "pattern \033[32m%02d\033[0m, length: %zu, %d times\n", i+1, erg[i].size()-1, erg[i][erg[i].size()-1]);
        for (int j = 0; j < erg[i].size()-1; j++) { // last element is number of matches, don't display that one
            // where is the best match?
            int match_location = patternShift[i];
            fprintf(stdout, "\t%s[%d] %s\n", (j==match_location?"*":" "), j+1, repeating_events_list[erg[i][j]-1].c_str());
            events[events.size()-1].push_back(repeating_events_list[erg[i][j]-1]);
        }
    }

    return std::make_pair(events, patternShift);
}

void displayPattern(std::vector<std::vector<std::string> > events, std::vector<int> patternShift) {
    // use ncurses to display the pattern until we kill it
    initscr();
    cbreak(); noecho();
    char cc[256];
    while (true) {
        usleep(200);
        clear();
        refresh();
        for (int i = 0; i < events.size(); i++) {
            clear();
            move(10-1,5);
            snprintf(cc, 256, "[%03d]", i);
            printw(cc);
            for (int j = 0; j < events[i].size(); j++) {
                move(10+j,5);
                printw(events[i][j].c_str());
            }
            refresh();
            usleep(200);
        }
    }

    endwin();
}