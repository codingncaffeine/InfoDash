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
                    std::regex longNameRegex("\"longName\":\"([^\"]+)\"");

                    std::smatch match;
                    // Narrow the search to the JSON block for the requested symbol to avoid matching other instruments
                    std::string marker = "\"symbol\":\"" + symbol + "\"";
                    size_t pos = response.body.find(marker);
                    std::string scope = response.body;
                    bool usedQuotedKey = false;
                    std::string quotedSymbol = "\"" + symbol + "\"";
                    if (pos != std::string::npos) {
                        // take a slice starting at the marker to reduce false positives
                        scope = response.body.substr(pos, std::min<size_t>(response.body.size() - pos, 12000));
                    } else {
                        // Try to find the symbol as a JSON key (e.g. "AAPL":{...}) and extract that object
                        // Find the quoted symbol key (e.g. "AAPL":{...}) without using regex
                        std::string quotedSymbol = "\"" + symbol + "\"";
                        size_t keyPos = response.body.find(quotedSymbol);
                        if (keyPos != std::string::npos) {
                            usedQuotedKey = true;
                            // locate the ':' after the key, then the opening '{'
                            size_t colonPos = response.body.find(':', keyPos + quotedSymbol.size());
                            if (colonPos != std::string::npos) {
                                size_t bracePos = response.body.find('{', colonPos);
                                if (bracePos != std::string::npos) {
                                    int depth = 0;
                                    size_t i = bracePos;
                                    for (; i < response.body.size(); ++i) {
                                        if (response.body[i] == '{') depth++;
                                        else if (response.body[i] == '}') {
                                            depth--;
                                            if (depth == 0) { i++; break; }
                                        }
                                    }
                                    size_t len = (i > bracePos) ? (i - bracePos) : std::min<size_t>(response.body.size() - bracePos, 12000);
                                    scope = response.body.substr(bracePos, std::min<size_t>(len, 12000));
                                }
                            }
                        } else {
                            // Fallback: try to extract the large JS data object (root.App.main) and scope to it
                            std::string rootMarker = "root.App.main";
                            size_t rootPos = response.body.find(rootMarker);
                            if (rootPos != std::string::npos) {
                                size_t bracePos = response.body.find('{', rootPos);
                                if (bracePos != std::string::npos) {
                                    int depth = 0;
                                    size_t i = bracePos;
                                    for (; i < response.body.size(); ++i) {
                                        if (response.body[i] == '{') depth++;
                                        else if (response.body[i] == '}') {
                                            depth--;
                                            if (depth == 0) { i++; break; }
                                        }
                                    }
                                    size_t len = (i > bracePos) ? (i - bracePos) : std::min<size_t>(response.body.size() - bracePos, 12000);
                                    scope = response.body.substr(bracePos, std::min<size_t>(len, 12000));
                                }
                            }
                        }
                    }

                    if (std::regex_search(scope, match, priceRegex))
                        data.price = "$" + match[1].str();
                    if (std::regex_search(scope, match, changeRegex)) {
                        data.change = match[1].str();
                        data.isUp = (data.change[0] != '-');
                    }
                    if (std::regex_search(scope, match, pctRegex))
                        data.changePercent = match[1].str() + "%";
                    if (std::regex_search(scope, match, nameRegex))
                        data.name = match[1].str();
                    else if (std::regex_search(scope, match, longNameRegex))
                        data.name = match[1].str();

                    // If name still missing and we didn't extract a symbol-scoped object, try locating the symbol under quoteData in root.App.main
                    if (data.name.empty() && !usedQuotedKey) {
                        std::string quoteDataMarker = "\"quoteData\"";
                        size_t qdPos = response.body.find(quoteDataMarker);
                        if (qdPos != std::string::npos) {
                            size_t symKeyPos = response.body.find(quotedSymbol, qdPos);
                            if (symKeyPos != std::string::npos) {
                                size_t colonPos = response.body.find(':', symKeyPos + quotedSymbol.size());
                                if (colonPos != std::string::npos) {
                                    size_t bracePos = response.body.find('{', colonPos);
                                    if (bracePos != std::string::npos) {
                                        int depth = 0;
                                        size_t i = bracePos;
                                        for (; i < response.body.size(); ++i) {
                                            if (response.body[i] == '{') depth++;
                                            else if (response.body[i] == '}') {
                                                depth--;
                                                if (depth == 0) { i++; break; }
                                            }
                                        }
                                        size_t len = (i > bracePos) ? (i - bracePos) : std::min<size_t>(response.body.size() - bracePos, 12000);
                                        std::string qscope = response.body.substr(bracePos, std::min<size_t>(len, 12000));
                                        if (std::regex_search(qscope, match, nameRegex)) data.name = match[1].str();
                                        else if (std::regex_search(qscope, match, longNameRegex)) data.name = match[1].str();
                                    }
                                }
                            }
                        }
                    }

                        // As a more robust fallback, extract company name from HTML meta/title
                        if (data.name.empty()) {
                            std::regex metaOgRegex("<meta[^>]+property=[\"']og:title[\"'][^>]+content=[\"']([^\"']+)[\"']", std::regex::icase);
                            std::regex titleRegex("<title>([^<]+)</title>", std::regex::icase);
                            if (std::regex_search(response.body, match, metaOgRegex)) {
                                std::string og = match[1].str();
                                size_t p = og.find(" (");
                                if (p != std::string::npos) og = og.substr(0, p);
                                data.name = og;
                            } else if (std::regex_search(response.body, match, titleRegex)) {
                                std::string t = match[1].str();
                                size_t p = t.find(" (");
                                if (p != std::string::npos) t = t.substr(0, p);
                                // strip trailing hyphen suffix like " - Yahoo Finance"
                                size_t hy = t.find(" - ");
                                if (hy != std::string::npos) t = t.substr(0, hy);
                                data.name = t;
                            }
                        }

                    // If price wasn't found in the scoped JSON, try a nearby window around the symbol in the full page
                    if (data.price.empty()) {
                        std::string quotedSymbol2 = "\"" + symbol + "\"";
                        size_t symPos = response.body.find(quotedSymbol2);
                        // Find the closest price/change/pct matches to the symbol occurrence
                        if (symPos != std::string::npos) {
                            // price
                            size_t bestPos = std::string::npos;
                            std::string bestPrice;
                            for (std::sregex_iterator it(response.body.begin(), response.body.end(), priceRegex), end; it != end; ++it) {
                                size_t p = it->position();
                                if (bestPos == std::string::npos || std::abs((long long)p - (long long)symPos) < (long long)std::abs((long long)bestPos - (long long)symPos)) {
                                    bestPos = p;
                                    bestPrice = (*it)[1].str();
                                }
                            }
                            if (!bestPrice.empty()) data.price = "$" + bestPrice;

                            // change
                            bestPos = std::string::npos;
                            std::string bestChange;
                            for (std::sregex_iterator it(response.body.begin(), response.body.end(), changeRegex), end; it != end; ++it) {
                                size_t p = it->position();
                                if (bestPos == std::string::npos || std::abs((long long)p - (long long)symPos) < (long long)std::abs((long long)bestPos - (long long)symPos)) {
                                    bestPos = p;
                                    bestChange = (*it)[1].str();
                                }
                            }
                            if (!bestChange.empty()) {
                                data.change = bestChange;
                                data.isUp = (data.change[0] != '-');
                            }

                            // pct
                            bestPos = std::string::npos;
                            std::string bestPct;
                            for (std::sregex_iterator it(response.body.begin(), response.body.end(), pctRegex), end; it != end; ++it) {
                                size_t p = it->position();
                                if (bestPos == std::string::npos || std::abs((long long)p - (long long)symPos) < (long long)std::abs((long long)bestPos - (long long)symPos)) {
                                    bestPos = p;
                                    bestPct = (*it)[1].str();
                                }
                            }
                            if (!bestPct.empty()) data.changePercent = bestPct + "%";
                                // try to find a nearby name if we didn't get one
                                if (data.name.empty()) {
                                    size_t bestPosN = std::string::npos;
                                    std::string bestName;
                                    for (std::sregex_iterator it(response.body.begin(), response.body.end(), nameRegex), end; it != end; ++it) {
                                        size_t p = it->position();
                                        if (bestPosN == std::string::npos || std::abs((long long)p - (long long)symPos) < (long long)std::abs((long long)bestPosN - (long long)symPos)) {
                                            bestPosN = p;
                                            bestName = (*it)[1].str();
                                        }
                                    }
                                    if (bestName.empty()) {
                                        for (std::sregex_iterator it(response.body.begin(), response.body.end(), longNameRegex), end; it != end; ++it) {
                                            size_t p = it->position();
                                            if (bestPosN == std::string::npos || std::abs((long long)p - (long long)symPos) < (long long)std::abs((long long)bestPosN - (long long)symPos)) {
                                                bestPosN = p;
                                                bestName = (*it)[1].str();
                                            }
                                        }
                                    }
                                    if (!bestName.empty()) data.name = bestName;
                                }
                        } else {
                            // Last resort: search entire body for first occurrence
                            if (std::regex_search(response.body, match, priceRegex))
                                data.price = "$" + match[1].str();
                            if (std::regex_search(response.body, match, changeRegex)) {
                                data.change = match[1].str();
                                data.isUp = (data.change[0] != '-');
                            }
                            if (std::regex_search(response.body, match, pctRegex))
                                data.changePercent = match[1].str() + "%";
                        }
                    }
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
