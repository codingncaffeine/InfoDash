#include <iostream>
#include <memory>
#include <future>
#include "services/RSSService.hpp"

int main() {
    InfoDash::RSSService svc;
    auto prom = std::make_shared<std::promise<std::vector<InfoDash::RSSItem>>>();
    auto fut = prom->get_future();

    std::string testUrl = "https://www.cnn.com";
    std::cout << "Fetching: " << testUrl << "\n";

    svc.fetchFeed(testUrl, [prom](std::vector<InfoDash::RSSItem> items){
        prom->set_value(std::move(items));
    });

    auto items = fut.get();
    std::cout << "Received " << items.size() << " items\n";
    for (size_t i = 0; i < items.size() && i < 20; ++i) {
        const auto &it = items[i];
        std::cout << i+1 << ". " << (it.title.empty() ? "(no title)" : it.title) << "\n";
        std::cout << "   Link: " << it.link << "\n";
        std::cout << "   Image: " << (it.imageUrl.empty() ? "(none)" : it.imageUrl) << "\n";
        std::cout << "   Source: " << it.source << "\n";
    }

    return 0;
}
