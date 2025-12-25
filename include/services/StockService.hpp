#pragma once
#include <string>
#include <vector>
#include <functional>

namespace InfoDash {

struct StockData {
    std::string symbol;
    std::string price;
    std::string change;
    std::string changePercent;
    bool isUp;
    std::string name;
};

class StockService {
public:
    StockService();
    void fetchStock(const std::string& symbol, std::function<void(StockData)> callback);
    void fetchAllStocks(std::function<void(std::vector<StockData>)> callback);
};

}
