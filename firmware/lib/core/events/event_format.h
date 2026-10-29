#pragma once
// Text form of log events (cycle 007, R6): "utc_epoch,type,detail\n", detail at most 80 characters.
#include <hal.h>
#include <stddef.h>
#include <stdint.h>

namespace events {

constexpr size_t kMaxDetail = 80;
constexpr size_t kMaxLine = 20 + 16 + kMaxDetail + 4;

const char* typeName(hal::EventType type);

// Commas and line breaks in the detail are replaced so one event is always one CSV line.
// Returns the line length (including the newline), or 0 if `len` is too small.
size_t formatLine(char* buf, size_t len, int64_t utcEpoch, hal::EventType type, const char* detail);

// Parse a line written by formatLine. Returns false for a malformed line.
bool parseLine(const char* line, int64_t& utcEpoch, char* typeOut, size_t typeLen, char* detailOut, size_t detailLen);

}  // namespace events
