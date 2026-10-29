#pragma once
// CSV export of the event log (cycle 012): UTF-8 with BOM and CRLF so Excel shows Persian text correctly.
#include <stddef.h>
#include <stdint.h>

namespace events {

// BOM + "local_time,utc_time,type,detail\r\n". Returns the length, 0 if `len` is too small.
size_t csvHeader(char* buf, size_t len);

// One row. Times are ISO 8601: local = UTC + tzOffsetMin with the offset written (2026-10-29T17:37:23+03:30),
// UTC with a Z. An epoch of 0 means "time unknown" and gives empty time cells.
// Fields with commas, quotes, line breaks or a leading space are quoted. Returns the length, 0 if too small.
size_t csvRow(char* buf, size_t len, int64_t utcEpoch, int tzOffsetMin, const char* type, const char* detail);

}  // namespace events
