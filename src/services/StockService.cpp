#include "services/StockService.hpp"
#include "utils/HttpClient.hpp"
#include "utils/HtmlParser.hpp"
#include "utils/Config.hpp"
#include <thread>
#include <regex>

namespace InfoDash {

StockService::StockService() {}

void StockService::fetchStock(const std::string& symbol, std::function<void(StockData)> callback) {
    std::thread([symbol, callback]() {
        StockData data;
        data.symbol = symbol;
        data.isUp = true;
        
        HttpClient client;
        // Scrape from Yahoo Finance (no API)
        std::string url = "https://finance.yahoo.com/quote/" + symbol;
        auto response = client.get(url);
        
        if (response.success) {
            HtmlParser parser;
            if (parser.parse(response.body)) {
                // Try to extract price from page
                std::regex priceRegex("\"regularMarketPrice\":\\{\"raw\":([0-9.]+)");
                std::regex changeRegex("\"regularMarketChange\":\\{\"raw\":(-?[0-9.]+)");
                std::regex pctRegex("\"regularMarketChangePercent\":\\{\"raw\":(-?[0-9.]+)");
                std::regex nameRegex("\"shortName\":\"([^\"]+)\"");
                
                std::smatch match;
                if (std::regex_search(response.body, match, priceRegex))
                    data.price = "$" + match[1].str();
                if (std::regex_search(response.body, match, changeRegex)) {
                    data.change = match[1].str();
                    data.isUp = (data.change[0] != '-');
                }
                if (std::regex_search(response.body, match, pctRegex))
                    data.changePercent = match[1].str() + "%";
                if (std::regex_search(response.body, match, nameRegex))
                    data.name = match[1].str();
            }
        }
        
        // Fallback if scraping failed
        if (data.price.empty()) {
            data.price = "N/A";
            data.change = "0.00";
            data.changePercent = "0.00%";
            data.name = symbol;
        }
        
        callback(data);
    }).detach();
}

void StockService::fetchAllStocks(std::function<void(std::vector<StockData>)> callback) {
    auto symbols = Config::getInstance().getStockSymbols();
    auto results = std::make_shared<std::vector<StockData>>();
    auto remaining = std::make_shared<int>(symbols.size());
    auto mtx = std::make_shared<std::mutex>();

    if (symbols.empty()) { callback({}); return; }

    for (const auto& sym : symbols) {
        fetchStock(sym, [results, remaining, mtx, callback](StockData data) {
            std::lock_guard<std::mutex> lock(*mtx);
            results->push_back(data);
            (*remaining)--;
            if (*remaining == 0) callback(*results);
        });
    }
}

}
