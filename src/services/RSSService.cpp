#include "services/RSSService.hpp"
#include "utils/HttpClient.hpp"
#include "utils/HtmlParser.hpp"
#include "utils/Config.hpp"
#include <thread>
#include <algorithm>
#include <memory>
#include <mutex>
#include <algorithm>

namespace InfoDash {

// Sanitize string to valid UTF-8
static std::string sanitizeUtf8(const std::string& input) {
    std::string result;
    const char* p = input.c_str();
    while (*p) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x80) {
            // ASCII
            result += *p;
            p++;
        } else if ((c & 0xE0) == 0xC0 && p[1]) {
            // 2-byte UTF-8
            if ((p[1] & 0xC0) == 0x80) {
                result += p[0];
                result += p[1];
                p += 2;
            } else {
                p++; // Skip invalid
            }
        } else if ((c & 0xF0) == 0xE0 && p[1] && p[2]) {
            // 3-byte UTF-8
            if ((p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
                result += p[0];
                result += p[1];
                result += p[2];
                p += 3;
            } else {
                p++;
            }
        } else if ((c & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
            // 4-byte UTF-8
            if ((p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
                result += p[0];
                result += p[1];
                result += p[2];
                result += p[3];
                p += 4;
            } else {
                p++;
            }
        } else {
            p++; // Skip invalid byte
        }
    }
    return result;
}

RSSService::RSSService() {}

void RSSService::fetchFeed(const std::string& url, std::function<void(std::vector<RSSItem>)> callback) {
    std::thread([url, callback]() {
        HttpClient client;
        auto response = client.get(url);
        std::vector<RSSItem> items;
        
        auto tryParse = [&](const std::string &body) -> std::vector<std::map<std::string, std::string>> {
            try {
                return HtmlParser::parseRSSItems(body);
            } catch (...) {
                return {};
            }
        };

        if (response.success) {
            auto parsed = HtmlParser::parseRSSItems(response.body);
            // If no items found, try autodiscovering an RSS/Atom link from HTML
            if (parsed.empty()) {
                HtmlParser parser;
                if (parser.parse(response.body)) {
                    // Look for RSS/Atom link elements
                    std::string href = parser.getAttribute("//link[@rel='alternate' and contains(@type,'rss') or contains(@type,'atom')]", "href");
                    if (href.empty()) {
                        // fallback: any link containing 'rss' or 'feed'
                        href = parser.getAttribute("//link[contains(@href,'rss') or contains(@href,'feed')]", "href");
                    }
                    if (!href.empty()) {
                        // Resolve relative URLs to absolute
                        auto resolveUrl = [](const std::string &base, const std::string &href) {
                            if (href.rfind("http://", 0) == 0 || href.rfind("https://", 0) == 0) return href;
                            // extract scheme and host
                            size_t s = base.find("://");
                            std::string scheme = "https";
                            std::string host = base;
                            if (s != std::string::npos) {
                                scheme = base.substr(0, s);
                                size_t start = s + 3;
                                size_t end = base.find('/', start);
                                host = (end == std::string::npos) ? base.substr(start) : base.substr(start, end - start);
                            }
                            if (href.rfind("//", 0) == 0) return scheme + ":" + href;
                            if (href.rfind("/", 0) == 0) return scheme + "://" + host + href;
                            // relative to base path
                            size_t pos = base.rfind('/');
                            std::string basepath = (pos == std::string::npos) ? base : base.substr(0, pos + 1);
                            return basepath + href;
                        };

                        std::string feedUrl = resolveUrl(url, href);
                        auto feedResp = client.get(feedUrl);
                        if (feedResp.success) parsed = tryParse(feedResp.body);
                    }
                }
            }
            for (const auto& p : parsed) {
                RSSItem item;
                item.title = sanitizeUtf8(p.count("title") ? p.at("title") : "");
                item.link = p.count("link") ? p.at("link") : "";
                item.description = sanitizeUtf8(p.count("description") ? p.at("description") : "");
                item.pubDate = p.count("pubDate") ? p.at("pubDate") : "";
                item.imageUrl = p.count("imageUrl") ? p.at("imageUrl") : "";
                item.author = sanitizeUtf8(p.count("author") ? p.at("author") : "");
                
                // Extract source from URL
                size_t start = url.find("://");
                if (start != std::string::npos) {
                    start += 3;
                    size_t end = url.find("/", start);
                    item.source = url.substr(start, end - start);
                }
                items.push_back(item);
            }
        }
        callback(items);
    }).detach();
}

void RSSService::fetchAllFeeds(std::function<void(std::vector<RSSItem>)> callback) {
    auto feeds = Config::getInstance().getRSSFeeds();
    auto results = std::make_shared<std::vector<RSSItem>>();
    auto remaining = std::make_shared<int>(feeds.size());
    auto mtx = std::make_shared<std::mutex>();

    if (feeds.empty()) { callback({}); return; }

    for (const auto& feed : feeds) {
        fetchFeed(feed, [results, remaining, mtx, callback](std::vector<RSSItem> items) {
            std::lock_guard<std::mutex> lock(*mtx);
            results->insert(results->end(), items.begin(), items.end());
            (*remaining)--;
            if (*remaining == 0) {
                std::sort(results->begin(), results->end(), [](const RSSItem& a, const RSSItem& b) {
                    return a.pubDate > b.pubDate;
                });
                callback(*results);
            }
        });
    }
}

}
