#include <algorithm>
#include <cctype>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <ctime> // For timestamp
#include <filesystem>
#include <iostream>
#include <netinet/in.h>
#include <fstream>
#include <csignal>
#include <string>
#include <syslog.h>
#include <thread>
#include <sys/socket.h>
#include <unistd.h>

using namespace std::chrono_literals;

void setLogLevel() {
    std::string config;
    config.append("filter f_nvsa { level(debug) and facility(local0); };\n");
    config.append("filter f_nvsb { level(info, warn, err, crit) and facility(local0); };\n");
    config.append("log { source(s_net); filter(f_nvsa); rewrite(r_app_name); destination(d_nvsa); };\n");
    config.append("log { source(s_net); filter(f_nvsb); rewrite(r_app_name); destination(d_nvsb); };\n");

    std::filesystem::path config_path{"/etc/syslog-ng/conf.d/config.cfg"};
    std::ofstream of{config_path};
    of << config;
    of.close();
}

void restartSyslogNg() {
    const int result = std::system("syslog-ng-ctl reload > reload.txt");
    std::cout << std::fstream("reload.txt").rdbuf();
    if (result == 0) {
        std::cout << "Restarted syslog-ng successful\n";
    } else {
        std::cerr << "Failed to restart syslog-ng: " << result << "\n";
    }
}

int connect(const std::string &remoteIp, int remotePort) {
    int sockfd;
    struct sockaddr_in servaddr{};

    // Create TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        exit(-1);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(remotePort);
    inet_pton(AF_INET, remoteIp.c_str(), &servaddr.sin_addr);

    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) != 0) {
        std::cerr << "connection to host failed" << std::endl;
        exit(-1);
    }
    return sockfd;
}

void sendSyslogMessage(int sockfd, const int facility, const std::string &message) {

    // Get current time for timestamp
    time_t rawtime;
    struct tm *timeinfo;
    char buffer[128];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    std::string onelineMessage = message;
    std::replace(onelineMessage.begin(), onelineMessage.end(), '\n', ';');

    // https://datatracker.ietf.org/doc/html/rfc5424
    // Construct syslog message (example with facility LOG_LOCAL0 and severity LOG_INFO)
    std::string fullMessage = "<" + std::to_string(facility) + ">" + std::string(buffer) + "app-name: " + onelineMessage + "\n";

    ssize_t bytes_sent = send(sockfd, fullMessage.c_str(), fullMessage.size(), 0);
    std::cout << bytes_sent << std::endl;
}

void sendDebugMessage(int sockfd, const std::string &message) {
    constexpr int facility = LOG_LOCAL0 + LOG_DEBUG;
    sendSyslogMessage(sockfd, facility, message);
}

void sendInfoMessage(int sockfd, const std::string &message) {
    constexpr int facility = LOG_LOCAL0 + LOG_INFO;
    sendSyslogMessage(sockfd, facility, message);
}

void sendWarningMessage(int sockfd, const std::string &message) {
    constexpr int facility = LOG_LOCAL0 + LOG_WARNING;
    sendSyslogMessage(sockfd, facility, message);
}

void sendErrorMessage(int sockfd, const std::string &message) {
    constexpr int facility = LOG_LOCAL0 + LOG_ERR;
    sendSyslogMessage(sockfd, facility, message);
}

void sendCriticalMessage(int sockfd, const std::string &message) {
    constexpr int facility = LOG_LOCAL0 + LOG_CRIT;
    sendSyslogMessage(sockfd, facility, message);
}

int main(int argc, char *argv[]) {
    //setLogLevel();
    restartSyslogNg();
    int sockfd = connect("127.0.0.1", 514);
    sendDebugMessage(sockfd, "This is a super debug message");
    for (int i = 0; i < 20; ++i) {
        sendDebugMessage(sockfd, "Begining of " + std::to_string(i));
        sendInfoMessage(sockfd, std::to_string(i) + ": This is a test syslog message from C++!!!");
        sendDebugMessage(sockfd, "Ending of " + std::to_string(i));
        sendDebugMessage(sockfd, "Check for warn " + std::to_string(i));
        if(i % 3) {
            sendWarningMessage(sockfd, "Warning... weapons energy low!");
        }
    }
    sendErrorMessage(sockfd, "Trying out error");
    sendCriticalMessage(sockfd, "Trying out critical");

    sendInfoMessage(sockfd, "Multi-\nline\nmessage!!!");
    close(sockfd);
    return 0;
}
