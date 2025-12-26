#include <iostream>
#include <vector>
#include "services/StockService.hpp"
#include <future>

int main() {
    InfoDash::StockService svc;
    std::vector<std::string> syms = {"AAPL", "GOOGL", "MSFT", "NVDA"};

    for (const auto &s : syms) {
        std::promise<InfoDash::StockData> p;
        auto f = p.get_future();
        svc.fetchStock(s, [&p](InfoDash::StockData d){ p.set_value(d); });
        auto d = f.get();
        std::cout << "Symbol: " << s << " -> name: " << d.name << ", price: " << d.price << ", change: " << d.change << "\n";
    }
    return 0;
}
