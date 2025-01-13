#pragma once

#include <string_view>
#include <vector>
#include <array>
#include <unordered_map>
#include <iostream>
#include <iterator>
#include <utility>
#include <ranges>

#ifndef CSV_DELIM
#define CSV_DELIM ','
#endif

namespace csvreader {
/* Reader State Machine */
enum class ReaderState {
    /**
     * Read current character as part of token.
     */
    READ_TOKEN,
    /**
     * Current character is quote, indicating quoted token.
     */
    IS_QUOTE_OPEN,
    /**
     * Current character is quote, indicating end of quoted token.
     */
    IS_QUOTE_CLOSE,
    /**
     * Current character is delim, flush token to line value vector.
     */
    IS_DELIM,
    /**
     * Current character is newline, flush line value vector to values vector (row).
     */
    IS_NEWLINE,
    /**
     * Read current character as part of token, which is within quotes.
     */
    READ_QUOTE
};

/* debugging functions */
const char* ReaderStateStr_(ReaderState);
std::string cDebugVal_(char);
std::string tokenDebugVal_(std::string);
/**
 * Converts iterable into comma separated string of its values.
 */
//template<typename Iter> std::string prettyRange(Iter begin, Iter end);
template<class T>
concept Contiguous = requires(T t) {
    //std::contiguous_iterator<decltype(std::declval<T>().begin())>;
    { std::ranges::begin(t) } -> std::contiguous_iterator;
};
template<Contiguous C> std::string prettyRange(C const& c) {
    std::string op("{");
    auto begin = c.begin();
    auto end = c.end();
    for (; begin != end; ++begin) {
        op += *begin;
        op += ",";
    }
    if (op.ends_with(",")) {
        op.pop_back();
    }
    return op.append("}");
}
// General debug options
#define INVALIDATE_OSTREAM if (false) std::cout
#ifdef UTILS_PRINT_DEBUG
#define PRINTDEBUG std::cout
#pragma message "UTILS_PRINT_DEBUG is defined."
#else
#define PRINTDEBUG INVALIDATE_OSTREAM
#endif
// CSVReader specific debug options
#ifdef CSV_PRINT_DEBUG
#pragma message "CSV_PRINT_DEBUG is defined."
#define CSV_PRINTDEBUG PRINTDEBUG
#else
#define CSV_PRINTDEBUG INVALIDATE_OSTREAM
#endif

typedef std::vector<std::string> Values;
class CSVReader {
public:
    CSVReader() { }
    //CSVReader(CSVReader&);
    CSVReader(std::istream& stream, bool column_labels=true, bool skip_first=true);
    CSVReader(const char* file_path, bool cl=true, bool sf=true);

    /**
     * Read the CSV data from given stream.
     * stream: input stream to read from.
     * column_labels: interpret first row as column labels.
     * skip_first: do not store first line as accessible values.
     */
    void parse(std::istream& stream, bool column_labels=true, bool skip_first=true);
    /**
     * Convert string column label to column index.
     */
    std::size_t columnLabel(std::string l);
    /**
     * Get label info in a (label string, index) map format.
     */
    std::unordered_map<std::string, std::size_t> getLabels() const { return labels; }
    /**
     * Get value at col x row.
     */
    std::string get(std::size_t col, std::size_t row);
    /**
     * Get all values in column.
     */
    Values getColumn(std::size_t col);
    /**
     * Get all values in row.
     */
    Values getRow(std::size_t row);
    std::vector<Values> allRows() const { return rows; }
    std::size_t rowsSize() { return rows.size(); }
    std::size_t columnSize() { return col_size; }

private:
    ReaderState state = ReaderState::READ_TOKEN;
    /**
     * Column labels, each contains value of column position (e.g. "id":0, "name": 1).
     * Useful if CSV with known labels are expected, such as in gtfs/gtfs_subway/stops.txt.
     */
    std::unordered_map<std::string, std::size_t> labels;
    std::size_t col_size = 0;
    // Values of each row
    std::vector<Values> rows;
#ifdef CSV_FAST_COLUMNS
#pragma message "CSV_FAST_COLUMNS is defined."
    std::vector<Values> cols;

    /**
     * Push an empty Values to cols if i > cols.size();
     * WARNING: i - cols.size() > 1 may produce unexpected results.
     * Returns: Existing or newly created Values reference in cols[i].
     */
    Values& forceAt(std::size_t i);
    void flushToken(Values&, std::string&, std::size_t);
#else
    void flushToken(Values&, std::string&);
#endif

    ReaderState readChar(std::istream& st, char c);
};
}
