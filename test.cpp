#include <regex>
#include <iostream>

bool matchesTopic(const std::string& topic, const std::string& pattern) {
    // Step 1: Replace '+' with a regex pattern that matches one topic level
    std::regex plusWildcard("\\+"); // Regex to find '+'
    std::string match_one_level_wildcard = std::regex_replace(pattern, plusWildcard, "[^/]+");
    std::cout << match_one_level_wildcard << "\n"; 
    // Step 2: Replace '*' with a regex pattern that matches multiple topic levels
    std::regex starWildcard("\\*"); // Regex to find '*'
    std::string match_multi_level_wildcard = std::regex_replace(match_one_level_wildcard, starWildcard, ".*");

    // Step 3: Create a complete regex pattern that matches from the start to the end of the string
    std::string fullRegexPattern = "^" + match_multi_level_wildcard + "$";
    std::regex patternRegex(fullRegexPattern);

    // Step 4: Match the full topic string against the regex pattern
    return std::regex_match(topic, patternRegex);
}


int main() {
    std::string topic = "hai_iaone";
    std::string pattern = "hai_iaone";
    std::cout << "Match result: " << matchesTopic(topic, pattern) << std::endl;
    return 0;
}
