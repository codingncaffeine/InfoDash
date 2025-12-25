#include "services/RSSService.hpp"
#include "utils/HttpClient.hpp"
#include "utils/HtmlParser.hpp"
#include "utils/Config.hpp"
#include <thread>
#include <algorithm>
#include <memory>
#include <mutex>

namespace InfoDash {

RSSService::RSSService() {}

void RSSService::fetchFeed(const std::string& url, std::function<void(std::vector<RSSItem>)> callback) {
    std::thread([url, callback]() {
        HttpClient client;
        auto response = client.get(url);
        std::vector<RSSItem> items;
        
        if (response.success) {
            auto parsed = HtmlParser::parseRSSItems(response.body);
            for (const auto& p : parsed) {
                RSSItem item;
                item.title = p.count("title") ? p.at("title") : "";
                item.link = p.count("link") ? p.at("link") : "";
                item.description = p.count("description") ? p.at("description") : "";
                item.pubDate = p.count("pubDate") ? p.at("pubDate") : "";
                item.imageUrl = p.count("imageUrl") ? p.at("imageUrl") : "";
                item.author = p.count("author") ? p.at("author") : "";
                
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
