#pragma once
// Streams the stored event log as CSV, oldest line first, in chunks of any size (cycle 012, R2).
// Memory use is one row, however long the log is.
#include "littlefs_event_log.h"

namespace drivers {

class CsvStream {
public:
    CsvStream(LittleFsEventLog& log, int tzOffsetMin);
    ~CsvStream();

    // Fills `out` with up to `max` bytes. Returns 0 when everything has been delivered.
    size_t next(char* out, size_t max);

private:
    LittleFsEventLog& log_;
    int tz_;
    bool headerSent_ = false;
    bool eof_ = false;
    char row_[320];
    size_t rowLen_ = 0;
    size_t rowPos_ = 0;
};

}  // namespace drivers
