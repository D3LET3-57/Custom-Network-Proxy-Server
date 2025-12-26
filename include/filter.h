#ifndef FILTER_H
#define FILTER_H
#include <string>
#include <vector>

std::vector<std::string> loadBlockedHosts();
bool isBlocked(const std::string &host);

#endif // FILTER_H
