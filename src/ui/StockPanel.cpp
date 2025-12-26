#include "ui/StockPanel.hpp"
#include "utils/Config.hpp"
#include "utils/HttpClient.hpp"
#include <json-glib/json-glib.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <cctype>

namespace InfoDash {

static void onRemoveStockClicked(GtkButton* btn, gpointer userData) {
    auto* self = static_cast<StockPanel*>(userData);
    const char* sym = static_cast<const char*>(g_object_get_data(G_OBJECT(btn), "symbol"));
    if (sym) {
        Config::getInstance().removeStockSymbol(sym);
        self->refresh();
    }
}

StockPanel::StockPanel() : widget_(nullptr), tickerBox_(nullptr), stocksBox_(nullptr), symbolEntry_(nullptr), tickerTimerId_(0) {
    service_ = std::make_unique<StockService>();
    setupUI();
    refresh();
}

StockPanel::~StockPanel() {
    if (tickerTimerId_ > 0) g_source_remove(tickerTimerId_);
}

void StockPanel::setupUI() {
    widget_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(widget_, 16);
    gtk_widget_set_margin_end(widget_, 16);
    gtk_widget_set_margin_top(widget_, 8);
    gtk_widget_set_margin_bottom(widget_, 8);

    // Ticker tape at top
    GtkWidget* tickerFrame = gtk_frame_new(nullptr);
    gtk_widget_add_css_class(tickerFrame, "panel-card");
    tickerBox_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_start(tickerBox_, 8);
    gtk_widget_set_margin_end(tickerBox_, 8);
    gtk_widget_set_margin_top(tickerBox_, 8);
    gtk_widget_set_margin_bottom(tickerBox_, 8);
    gtk_frame_set_child(GTK_FRAME(tickerFrame), tickerBox_);
    gtk_box_append(GTK_BOX(widget_), tickerFrame);

    // Add stock section
    GtkWidget* addBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(addBox, "panel-card");

    symbolEntry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(symbolEntry_), "Enter stock symbol (e.g., AAPL)...");
    gtk_widget_set_hexpand(symbolEntry_, TRUE);
    gtk_box_append(GTK_BOX(addBox), symbolEntry_);

    GtkWidget* addBtn = gtk_button_new_with_label("Add Stock");
    gtk_widget_add_css_class(addBtn, "add-button");
    g_signal_connect(addBtn, "clicked", G_CALLBACK(onAddStockClicked), this);
    gtk_box_append(GTK_BOX(addBox), addBtn);

    gtk_box_append(GTK_BOX(widget_), addBox);

    // Stock cards area
    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);

    stocksBox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), stocksBox_);
    gtk_box_append(GTK_BOX(widget_), scroll);
}

void StockPanel::refresh() {
    service_->fetchAllStocks([this](std::vector<StockData> data) {
        pendingData_ = data;
        g_idle_add(updateUICallback, this);
    });
}

gboolean StockPanel::updateUICallback(gpointer userData) {
    auto* self = static_cast<StockPanel*>(userData);
    self->updateStocks(self->pendingData_);
    return FALSE;
}

void StockPanel::updateStocks(const std::vector<StockData>& data) {
    // Clear ticker
    GtkWidget* child;
    while ((child = gtk_widget_get_first_child(tickerBox_)) != nullptr)
        gtk_box_remove(GTK_BOX(tickerBox_), child);

    // Clear stocks
    while ((child = gtk_widget_get_first_child(stocksBox_)) != nullptr)
        gtk_box_remove(GTK_BOX(stocksBox_), child);

    for (const auto& s : data) {
        // Ticker item
        GtkWidget* tickerItem = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(tickerItem, "stock-ticker");

        GtkWidget* sym = gtk_label_new(s.symbol.c_str());
        gtk_widget_add_css_class(sym, "stock-symbol");
        gtk_box_append(GTK_BOX(tickerItem), sym);

        GtkWidget* price = gtk_label_new(s.price.c_str());
        gtk_widget_add_css_class(price, "stock-price");
        gtk_box_append(GTK_BOX(tickerItem), price);

        std::string changeStr = s.change + " (" + s.changePercent + ")";
        GtkWidget* change = gtk_label_new(changeStr.c_str());
        gtk_widget_add_css_class(change, s.isUp ? "stock-up" : "stock-down");
        gtk_box_append(GTK_BOX(tickerItem), change);

        gtk_box_append(GTK_BOX(tickerBox_), tickerItem);

        // Detail card
        GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_add_css_class(card, "article-card");

        GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        // Show symbol only to avoid incorrect or placeholder company names
        std::string displayName = s.symbol;
        GtkWidget* nameLabel = gtk_label_new(displayName.c_str());
        gtk_widget_add_css_class(nameLabel, "article-title");
        gtk_box_append(GTK_BOX(header), nameLabel);
        // Remove button
        GtkWidget* removeBtn = gtk_button_new_with_label("Remove");
        // store symbol on the button so the callback can find which symbol to remove
        g_object_set_data_full(G_OBJECT(removeBtn), "symbol", g_strdup(s.symbol.c_str()), g_free);
        g_signal_connect(removeBtn, "clicked", G_CALLBACK(onRemoveStockClicked), this);
        gtk_box_append(GTK_BOX(header), removeBtn);
        gtk_box_append(GTK_BOX(card), header);

        GtkWidget* priceRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
        GtkWidget* priceLabel = gtk_label_new(s.price.c_str());
        gtk_widget_add_css_class(priceLabel, "weather-temp");
        gtk_box_append(GTK_BOX(priceRow), priceLabel);

        GtkWidget* changeLabel = gtk_label_new(changeStr.c_str());
        gtk_widget_add_css_class(changeLabel, s.isUp ? "stock-up" : "stock-down");
        gtk_box_append(GTK_BOX(priceRow), changeLabel);
        gtk_box_append(GTK_BOX(card), priceRow);

        gtk_box_append(GTK_BOX(stocksBox_), card);
    }
}

void StockPanel::onAddStockClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<StockPanel*>(userData);
    const char* sym = gtk_editable_get_text(GTK_EDITABLE(self->symbolEntry_));
    if (sym && strlen(sym) > 0) {
        std::string input(sym);

        auto urlEncode = [](const std::string& s) {
            std::ostringstream o;
            for (auto c : s) {
                if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') o << c;
                else o << '%' << std::uppercase << std::hex << int((unsigned char)c) << std::nouppercase << std::dec;
            }
            return o.str();
        };

        auto lookupSymbol = [&](const std::string& query)->std::string {
            HttpClient client;
            std::string url = "https://query2.finance.yahoo.com/v1/finance/search?q=" + urlEncode(query) + "&quotesCount=3&newsCount=0";
            auto resp = client.get(url);
            if (!resp.success) return "";

            JsonParser* parser = json_parser_new();
            GError* err = nullptr;
            if (!json_parser_load_from_data(parser, resp.body.c_str(), resp.body.size(), &err)) {
                if (err) g_error_free(err);
                g_object_unref(parser);
                return "";
            }
            JsonNode* root = json_parser_get_root(parser);
            if (!root || !JSON_NODE_HOLDS_OBJECT(root)) { g_object_unref(parser); return ""; }
            JsonObject* obj = json_node_get_object(root);
            if (!json_object_has_member(obj, "quotes")) { g_object_unref(parser); return ""; }
            JsonArray* arr = json_object_get_array_member(obj, "quotes");
            guint len = json_array_get_length(arr);
            for (guint i = 0; i < len; ++i) {
                JsonObject* q = json_array_get_object_element(arr, i);
                if (!q) continue;
                if (json_object_has_member(q, "symbol")) {
                    const char* s = json_object_get_string_member(q, "symbol");
                    if (s && strlen(s) > 0) {
                        std::string symStr(s);
                        g_object_unref(parser);
                        return symStr;
                    }
                }
            }
            g_object_unref(parser);
            return "";
        };

        std::string symbol = lookupSymbol(input);
        if (symbol.empty()) {
            // fallback: treat input as symbol
            symbol = input;
            for (auto& c : symbol) c = toupper((unsigned char)c);
        }

        Config::getInstance().addStockSymbol(symbol);
        Config::getInstance().save();
        gtk_editable_set_text(GTK_EDITABLE(self->symbolEntry_), "");
        self->refresh();
    }
}

gboolean StockPanel::tickerUpdateCallback(gpointer userData) {
    auto* self = static_cast<StockPanel*>(userData);
    self->refresh();
    return TRUE;
}

}
