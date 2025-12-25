#include "utils/HttpClient.hpp"
#include <curl/curl.h>
#include <thread>

namespace InfoDash {

HttpClient::HttpClient() 
    : userAgent_("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"), timeout_(30) {
    curl_global_init(CURL_GLOBAL_ALL);
}

HttpClient::~HttpClient() { curl_global_cleanup(); }

size_t HttpClient::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

size_t HttpClient::writeBytesCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* vec = static_cast<std::vector<unsigned char>*>(userp);
    auto* data = static_cast<unsigned char*>(contents);
    vec->insert(vec->end(), data, data + size * nmemb);
    return size * nmemb;
}

size_t HttpClient::headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    std::string header(buffer, size * nitems);
    size_t pos = header.find(':');
    if (pos != std::string::npos) {
        std::string key = header.substr(0, pos);
        std::string val = header.substr(pos + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t\r\n") + 1);
        (*headers)[key] = val;
    }
    return size * nitems;
}

HttpClient::Response HttpClient::get(const std::string& url) {
    Response response{0, "", {}, false, ""};
    CURL* curl = curl_easy_init();
    if (!curl) { response.error = "CURL init failed"; return response; }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent_.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        long httpCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        response.statusCode = static_cast<int>(httpCode);
        response.success = (httpCode >= 200 && httpCode < 300);
    } else {
        response.error = curl_easy_strerror(res);
    }
    curl_easy_cleanup(curl);
    return response;
}

std::vector<unsigned char> HttpClient::getBytes(const std::string& url) {
    std::vector<unsigned char> data;
    CURL* curl = curl_easy_init();
    if (!curl) return data;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBytesCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent_.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        data.clear();
    }
    curl_easy_cleanup(curl);
    return data;
}

void HttpClient::getAsync(const std::string& url, std::function<void(Response)> callback) {
    std::thread([this, url, callback]() { callback(get(url)); }).detach();
}

void HttpClient::setUserAgent(const std::string& ua) { userAgent_ = ua; }
void HttpClient::setTimeout(long t) { timeout_ = t; }

}
