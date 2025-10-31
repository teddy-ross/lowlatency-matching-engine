// Simple single-threaded matching engine
// Protocol (text lines, newline-terminated):
// SUBMIT <id> <B|S> <price> <qty>
// CANCEL <id>
//
// Responses (text lines):
// ACK <id>
// FILL <incoming_id> <matched_id> <price> <qty>
// DONE

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>


int main(){


}
