#pragma once
#include <gtk/gtk.h>
#include <memory>
#include <vector>
#include "services/StockService.hpp"

namespace InfoDash {

class StockPanel {
public:
    StockPanel();
    ~StockPanel();
    GtkWidget* getWidget() const { return widget_; }
    void refresh();

private:
    void setupUI();
    void updateStocks(const std::vector<StockData>& data);
    static void onAddStockClicked(GtkButton* button, gpointer userData);
    static gboolean updateUICallback(gpointer userData);
    static gboolean tickerUpdateCallback(gpointer userData);

    GtkWidget* widget_;
    GtkWidget* tickerBox_;
    GtkWidget* stocksBox_;
    GtkWidget* symbolEntry_;
    std::unique_ptr<StockService> service_;
    std::vector<StockData> pendingData_;
    guint tickerTimerId_;
};

}
