#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string>

class MatchingEngine;

// Parse one line of text, apply it to engine, and return response text.
// Returns false if the line is empty (no response).
bool process_line(const std::string& line, MatchingEngine& engine, std::string& response);


#endif