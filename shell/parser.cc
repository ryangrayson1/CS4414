#include "parser.h"

//This method will split a string by WhiteSpaces  
std::vector <std::string> parse_from_line(const std::string &line) {
    std::istringstream ss(line);
    std::string token;
    std::vector<std::string> tokens;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}