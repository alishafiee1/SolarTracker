#include "csv_stream.h"

#include <string.h>

#include "csv_format.h"
#include "event_format.h"

namespace drivers {

CsvStream::CsvStream(LittleFsEventLog& log, int tzOffsetMin) : log_(log), tz_(tzOffsetMin) { log_.beginRead(); }

CsvStream::~CsvStream() { log_.endRead(); }

size_t CsvStream::next(char* out, size_t max) {
    size_t n = 0;
    if (!headerSent_) {
        char h[48];
        const size_t hl = events::csvHeader(h, sizeof(h));
        if (hl > max) return 0;  // callers pass at least a few hundred bytes
        memcpy(out, h, hl);
        n = hl;
        headerSent_ = true;
    }
    while (n < max) {
        if (rowPos_ >= rowLen_) {  // the previous row is done: convert the next stored line
            rowPos_ = rowLen_ = 0;
            char line[events::kMaxLine];
            if (eof_ || !log_.readLine(line, sizeof(line))) {
                eof_ = true;
                break;
            }
            int64_t t;
            char type[24], detail[96];
            if (!events::parseLine(line, t, type, sizeof(type), detail, sizeof(detail))) continue;  // a damaged line is skipped
            rowLen_ = events::csvRow(row_, sizeof(row_), t, tz_, type, detail);
            continue;
        }
        const size_t take = rowLen_ - rowPos_ < max - n ? rowLen_ - rowPos_ : max - n;
        memcpy(out + n, row_ + rowPos_, take);
        rowPos_ += take;
        n += take;
    }
    return n;
}

}  // namespace drivers
