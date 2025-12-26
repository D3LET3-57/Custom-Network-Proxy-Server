#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

std::vector<std::string> blockedHosts;

std::vector<std::string> loadBlockedHosts()
{
    std::ifstream file("config/blocked_domains.txt");
    if (!file.is_open())
    {
        std::cerr << "[-] Could not open blocked hosts file: " << "config/blocked_domains.txt" << "\n";
        return blockedHosts;
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty())
        {
            blockedHosts.push_back(line);
        }
    }
    file.close();
    return blockedHosts;
}

bool isBlocked(const std::string &host)
{
    for (const auto &blockedHost : blockedHosts)
    {
        std::string lowerHost = host;
        std::string lowerBlockedHost = blockedHost;
        std::transform(lowerHost.begin(), lowerHost.end(), lowerHost.begin(), ::tolower);
        std::transform(lowerBlockedHost.begin(), lowerBlockedHost.end(), lowerBlockedHost.begin(), ::tolower);
        if (lowerHost == lowerBlockedHost)
        {
            return true;
        }
    }
    return false;
}
