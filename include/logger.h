#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>
#include <fstream>

class Logger
{
public:
    static Logger &getInstance();
    void log(const std::string &clientIP, int clientPort,
             const std::string &host, int port,
             const std::string &requestLine,
             const std::string &action,
             int status,
             size_t size);

private:
    Logger();
    ~Logger();

    void rotate();
    std::string getCurrentTimestamp();

    std::ofstream logFile;
    std::mutex logMutex;
    const std::string logFileName = "proxy.log";
    const long maxLogSize = 5 * 1024 * 1024; // 5 MB
};

#endif // LOGGER_H
