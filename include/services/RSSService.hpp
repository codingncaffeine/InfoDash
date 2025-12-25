#pragma once
#include <string>
#include <vector>
#include <functional>

namespace InfoDash {

struct RSSItem {
    std::string title;
    std::string link;
    std::string description;
    std::string pubDate;
    std::string source;
    std::string imageUrl;
    std::string author;
};

class RSSService {
public:
    RSSService();
    void fetchFeed(const std::string& url, std::function<void(std::vector<RSSItem>)> callback);
    void fetchAllFeeds(std::function<void(std::vector<RSSItem>)> callback);
};

}
