#pragma once
#include <string>
#include <functional>
#include <map>
#include <vector>

namespace InfoDash {

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    struct Response {
        int statusCode;
        std::string body;
        std::map<std::string, std::string> headers;
        bool success;
        std::string error;
    };

    Response get(const std::string& url);
    std::vector<unsigned char> getBytes(const std::string& url);
    void getAsync(const std::string& url, std::function<void(Response)> callback);
    void setUserAgent(const std::string& userAgent);
    void setTimeout(long timeoutSeconds);

private:
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t writeBytesCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata);
    std::string userAgent_;
    long timeout_;
};

}
