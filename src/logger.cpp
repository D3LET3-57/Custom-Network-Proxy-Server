#include "../include/logger.h"
#include <iostream>
#include <ctime>
#include <sys/stat.h>

Logger &Logger::getInstance()
{
    static Logger instance;
    return instance;
}

Logger::Logger()
{
    logFile.open(logFileName, std::ios::app);
    if (!logFile.is_open())
    {
        std::cerr << "[-] Failed to open log file: " << logFileName << std::endl;
    }
}

Logger::~Logger()
{
    if (logFile.is_open())
    {
        logFile.close();
    }
}

std::string Logger::getCurrentTimestamp()
{
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tstruct);
    return std::string(buf);
}

void Logger::log(const std::string &clientIP, int clientPort,
                 const std::string &host, int port,
                 const std::string &requestLine,
                 const std::string &action,
                 int status,
                 size_t size)
{
    std::lock_guard<std::mutex> lock(logMutex);

    if (!logFile.is_open())
    {
        std::cerr << "[-] Log file not open, attempting to reopen..." << std::endl;
        logFile.open(logFileName, std::ios::app);
        if (!logFile.is_open())
        {
            std::cerr << "[-] Failed to reopen log file" << std::endl;
            return;
        }
    }

    // Check size for rotation
    long pos = logFile.tellp(); // Current position
    if (pos >= maxLogSize)
    {
        rotate();
    }

    logFile << "[" << getCurrentTimestamp() << "] "
            << clientIP << ":" << clientPort << " -> "
            << host << ":" << port << " "
            << "\"" << requestLine << "\" "
            << "Action: " << action << " "
            << "Status: " << status << " "
            << "Size: " << size << " bytes" << std::endl;
    logFile.flush();
}

void Logger::rotate()
{
    logFile.close();
    std::string oldLogName = logFileName + ".old";
    rename(logFileName.c_str(), oldLogName.c_str());
    logFile.open(logFileName, std::ios::app);
}
