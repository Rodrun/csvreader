#include "csvreader.hpp"

#include <fstream>
#include <string>
#include <iostream>
#include <algorithm>
#include <exception>

namespace csvreader {
// for debugging purposes
const char* ReaderStateStr_(ReaderState rs) {
    switch (rs) {
        case ReaderState::IS_QUOTE_OPEN:
            return "IS_QUOTE_OPEN";
            break;
        case ReaderState::IS_QUOTE_CLOSE:
            return "IS_QUOTE_CLOSE";
            break;
        case ReaderState::IS_DELIM:
            return "IS_DELIM";
            break;
        case ReaderState::IS_NEWLINE:
            return "IS_NEWLINE";
            break;
        case ReaderState::READ_QUOTE:
            return "READ_QUOTE";
            break;
        default:
            break;
    };
    return "READ_TOKEN";
}

// for debugging purposes, prettier newline printing
std::string cDebugVal_(char c) {
    if (c == '\n') {
        return std::string("\\n");
    }
    return std::string(1, c);
}

// for debugging purposes, prettier newline printing (string)
std::string tokenDebugVal_(std::string s) {
    for (std::size_t i = 0; i < s.size(); i++) {
        if (std::string(1, s.at(i)).compare("\n") == 0) {
            s.replace(i, 1, "\\n");
        }
    }
    return s;
}

CSVReader::CSVReader(std::istream& stream, bool column_labels, bool skip_first) {
    parse(stream, column_labels, skip_first);
}

CSVReader::CSVReader(const char* file_path, bool cl, bool sf) {
    std::ifstream f(file_path);
    if (f.is_open()) {
        parse(f, cl, sf);
    } else {
        throw std::runtime_error(std::string("CSVReader could not open file ").append(file_path));
    }
    //f.close();
}

void CSVReader::parse(std::istream& stream, bool column_labels, bool skip_first) {
    bool first_line = true; // At first line flag
    state = ReaderState::READ_TOKEN;
    std::string token("");
    Values line_values;
#ifdef CSV_FAST_COLUMNS
    std::size_t at_col = 0;
#endif
    char current;
    CSV_PRINTDEBUG << "last_state\tc\tnew_state\tdebug" << std::endl;
    while ((current = stream.get()) && !stream.eof()) {
        CSV_PRINTDEBUG << ReaderStateStr_(state) << "\t";
        state = readChar(stream, current); // new state
        CSV_PRINTDEBUG << cDebugVal_(current) << "\t" << ReaderStateStr_(state) << "\t";
        // go thru the state machine
        switch (state) {
            case ReaderState::READ_QUOTE:
            case ReaderState::READ_TOKEN:
                token += std::string(1, current);
                CSV_PRINTDEBUG << "token so far is: '" << tokenDebugVal_(token) << "'";
                break;
            case ReaderState::IS_DELIM:
#ifdef CSV_FAST_COLUMNS
                //forceAt(at_col).push_back(token);
                flushToken(line_values, token, at_col);
                at_col += 1;
#else
                flushToken(line_values, token);
#endif
                break;
            case ReaderState::IS_NEWLINE:
                // flush last token
#ifdef CSV_FAST_COLUMNS
                flushToken(line_values, token, at_col);
                // TODO: enforce column width consistency!
                col_size = at_col + 1;
                at_col = 0;
#else
                flushToken(line_values, token);
#endif
                // first line column labels
                if (first_line) {
                    for (auto i = 0; i < line_values.size(); i++) {
                        labels[line_values.at(i).data()] = i;
                    }
                    first_line = false;
                }
                // flush line
                rows.push_back(line_values);
                line_values.clear();
                CSV_PRINTDEBUG << ". flushed line, new rows size = " << rows.size();
                break;
            default:
                break;
        };
        CSV_PRINTDEBUG << std::endl;
    }
}

#ifdef CSV_FAST_COLUMNS
void CSVReader::flushToken(Values& r, std::string& token, std::size_t column) {
    CSV_PRINTDEBUG << "flushing (column = " << column << ") '" << tokenDebugVal_(token) << "' -> "; 
    forceAt(column).push_back(token);
    r.push_back(token);
    token = "";
    CSV_PRINTDEBUG << prettyRange(r);
}

Values& CSVReader::forceAt(std::size_t i) {
    if (i >= cols.size()) {
        CSV_PRINTDEBUG << "[forceAt(" << i << ") emplacing...]";
        cols.emplace_back();
    }
    return cols.at(i);
}
#else
void CSVReader::flushToken(Values& r, std::string& token) {
    CSV_PRINTDEBUG << "flushing '" << tokenDebugVal_(token) << "' -> "; 
    r.push_back(token);
    token = "";
    CSV_PRINTDEBUG << prettyRange(r);
}
#endif 

// return new state based off char
ReaderState CSVReader::readChar(std::istream& st, char c) {
    //c = st.get();
    ReaderState next_state = ReaderState::READ_TOKEN;
    if (c == '\"') {
        switch (state) { // last_state
            case ReaderState::READ_QUOTE:
            case ReaderState::IS_QUOTE_OPEN:
                next_state = ReaderState::IS_QUOTE_CLOSE;
                break;
            case ReaderState::IS_QUOTE_CLOSE:
                next_state = ReaderState::READ_QUOTE;
                break;
            case ReaderState::READ_TOKEN:
            case ReaderState::IS_DELIM:
            case ReaderState::IS_NEWLINE:
                next_state = ReaderState::IS_QUOTE_OPEN;
                break;
        };
    } else if (c == '\n') {
        switch (state) {
            case ReaderState::READ_QUOTE:
                next_state = ReaderState::READ_QUOTE;
                break;
            case ReaderState::IS_QUOTE_OPEN:
            case ReaderState::IS_QUOTE_CLOSE:
            case ReaderState::READ_TOKEN:
            case ReaderState::IS_DELIM:
            case ReaderState::IS_NEWLINE:
                next_state = ReaderState::IS_NEWLINE;
                break;
        };
    } else if (c == CSV_DELIM) {
        switch (state) {
            case ReaderState::IS_QUOTE_OPEN:
            case ReaderState::READ_QUOTE:
                next_state = ReaderState::READ_QUOTE;
                break;
            case ReaderState::IS_QUOTE_CLOSE:
            case ReaderState::READ_TOKEN:
            case ReaderState::IS_NEWLINE:
            case ReaderState::IS_DELIM:
                next_state = ReaderState::IS_DELIM;
                break;
        };
    } else { // ...
        switch (state) {
            case ReaderState::READ_QUOTE:
                next_state = ReaderState::READ_QUOTE;
                break;
            case ReaderState::IS_QUOTE_OPEN:
                next_state = ReaderState::READ_QUOTE;
                break;
            case ReaderState::IS_QUOTE_CLOSE:
            case ReaderState::READ_TOKEN:
            case ReaderState::IS_DELIM:
            case ReaderState::IS_NEWLINE:
                next_state = ReaderState::READ_TOKEN;
                break;
        };
    }
    return next_state;
}

std::string CSVReader::get(std::size_t col, std::size_t row) {
    return rows.at(row).at(col);
}

std::size_t CSVReader::columnLabel(std::string l) {
    return labels[l];
}

Values CSVReader::getColumn(std::size_t col) {
#ifdef CSV_FAST_COLUMNS
    return cols.at(col);
#else
    Values values;
    values.reserve(rows.size());
    for (auto row : rows) {
        values.push_back(row.at(col));
    }
    return values;
#endif
}

Values CSVReader::getRow(std::size_t row) {
    return rows.at(row);
}
}
