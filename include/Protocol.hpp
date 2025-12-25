#ifndef PROTOCOL_H
#define PROTOCOL_H
#pragma once


#include <string>

class MatchingEngine;

/**
 * Parse a single textual protocol line and apply the command to the
 * provided `MatchingEngine` instance.
 *
 * Supported commands:
 *   - `SUBMIT <id> <side> <price> <quantity>`
 *   - `CANCEL <id>`
 *   - `DUMP`
 *
 * The parser tolerates extra whitespace. An empty or whitespace-only
 * input is treated as a no-op and the function returns `false`.
 *
 * @param line     Input line (may or may not include a trailing '\n').
 * @param engine   MatchingEngine to apply the parsed command to.
 * @param response Output string that will receive one or more lines of
 *                 textual response (each terminated with '\n'). If the
 *                 function returns `false`, `response` will be empty.
 * @return `true` if a response (possibly empty) should be sent back to
 *         the client; `false` if the input was empty/no-op.
 */
bool process_line(const std::string& line, MatchingEngine& engine, std::string& response);

#endif
