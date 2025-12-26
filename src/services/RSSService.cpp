#include "services/RSSService.hpp"
#include "utils/HttpClient.hpp"
#include "utils/HtmlParser.hpp"
#include "utils/Config.hpp"
#include <thread>
#include <algorithm>
#include <memory>
#include <mutex>
#include <algorithm>
#include <regex>

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
        const size_t MAX_AUTODISCOVER_ITEMS = 20;
        
        auto tryParse = [&](const std::string &body) -> std::vector<std::map<std::string, std::string>> {
            try {
                return HtmlParser::parseRSSItems(body);
            } catch (...) {
                return {};
            }
        };

        if (response.success) {
            auto parsed = HtmlParser::parseRSSItems(response.body);
            bool autodiscovered = false;
            // If no items found, try autodiscovering an RSS/Atom link from HTML
            if (parsed.empty()) {
                autodiscovered = true;
                HtmlParser parser;
                if (parser.parse(response.body)) {
                    // Try <link rel="alternate" type="application/rss+xml|atom" href="...">
                    std::string href = parser.getAttribute("//link[@rel='alternate' and (contains(@type,'rss') or contains(@type,'atom'))]", "href");
                    if (href.empty()) {
                        // fallback: any link element with rss/feed in href
                        href = parser.getAttribute("//link[contains(translate(@href,'RSS','rss'),'rss') or contains(translate(@href,'FEED','feed'),'feed')]", "href");
                    }

                    auto resolveUrl = [](const std::string &base, const std::string &href) {
                        if (href.rfind("http://", 0) == 0 || href.rfind("https://", 0) == 0) return href;
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
                        size_t pos = base.rfind('/');
                        std::string basepath = (pos == std::string::npos) ? base : base.substr(0, pos + 1);
                        return basepath + href;
                    };

                    auto tryFeedUrl = [&](const std::string &candidate) -> bool {
                        std::string u = resolveUrl(url, candidate);
                        auto r = client.get(u);
                        if (r.success) {
                            auto parsed2 = tryParse(r.body);
                            if (!parsed2.empty()) { parsed = parsed2; return true; }

                            // If the candidate returned an HTML index (like CNN's services/rss),
                            // look for link elements inside that page and probe them.
                            HtmlParser subparser;
                            if (subparser.parse(r.body)) {
                                std::string found = subparser.getAttribute("//link[@rel='alternate' and (contains(@type,'rss') or contains(@type,'atom'))]", "href");
                                if (found.empty()) {
                                    found = subparser.getAttribute("//link[contains(translate(@href,'RSS','rss'),'rss') or contains(translate(@href,'FEED','feed'),'feed')]", "href");
                                }
                                if (!found.empty()) {
                                    std::string inner = resolveUrl(u, found);
                                    auto r2 = client.get(inner);
                                    if (r2.success) {
                                        auto parsed3 = tryParse(r2.body);
                                        if (!parsed3.empty()) { parsed = parsed3; return true; }
                                    }
                                }
                            }
                        }
                        return false;
                    };

                    // If a link was found, try it first
                    if (!href.empty()) {
                        if (tryFeedUrl(href)) {
                            // found, parsed now set
                        }
                    }

                    // If still empty, try scanning raw HTML for RSS-like hrefs
                    if (parsed.empty()) {
                        // Scan raw HTML for hrefs containing 'rss' or 'feed' (case-insensitive)
                        std::string body = response.body;
                        std::string lower = body;
                        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                        size_t pos = 0;
                        while (pos != std::string::npos) {
                            pos = lower.find("href", pos);
                            if (pos == std::string::npos) break;
                            // find '=' after href
                            size_t eq = lower.find('=', pos);
                            if (eq == std::string::npos) { pos += 4; continue; }
                            // find opening quote
                            size_t q = lower.find_first_of("'\"", eq);
                            if (q == std::string::npos) { pos = eq + 1; continue; }
                            char quote = body[q];
                            size_t endq = body.find(quote, q + 1);
                            if (endq == std::string::npos) { pos = q + 1; continue; }
                            std::string candidate = body.substr(q + 1, endq - (q + 1));
                            std::string lowerCand = candidate;
                            std::transform(lowerCand.begin(), lowerCand.end(), lowerCand.begin(), ::tolower);
                            if (lowerCand.find("rss") != std::string::npos || lowerCand.find("feed") != std::string::npos) {
                                if (tryFeedUrl(candidate)) break;
                            }
                            pos = endq + 1;
                        }
                    }

                    // Still empty: probe common candidate feed paths
                    if (parsed.empty()) {
                        std::vector<std::string> candidates = {
                            "/rss", "/feed", "/feeds", "/rss.xml", "/feed.xml", "/feeds.xml",
                            "/index.rss", "/feeds/rss.xml", "/services/rss/?no_redirect=true"
                        };
                        for (const auto &c : candidates) {
                            if (tryFeedUrl(c)) break;
                        }
                    }
                    }
                }
            // If we autodiscovered from a non-feed page, cap number of items
            if (!parsed.empty() && autodiscovered && parsed.size() > MAX_AUTODISCOVER_ITEMS) {
                parsed.resize(MAX_AUTODISCOVER_ITEMS);
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
                // If no image was found in the feed entry, try fetching the article
                // page and look for OpenGraph/Twitter meta tags or a main image.
                if (item.imageUrl.empty() && !item.link.empty()) {
                    auto artResp = client.get(item.link);
                    if (artResp.success && !artResp.body.empty()) {
                        HtmlParser artParser;
                        if (artParser.parse(artResp.body)) {
                            std::string img = artParser.getAttribute("//meta[@property='og:image']", "content");
                            if (img.empty()) img = artParser.getAttribute("//meta[@name='twitter:image']", "content");
                            if (img.empty()) img = artParser.getAttribute("//link[@rel='image_src']", "href");
                            if (img.empty()) img = artParser.getAttribute("//img[1]", "src");
                            if (!img.empty()) {
                                // resolve relative URLs
                                auto resolveRel = [](const std::string &base, const std::string &href){
                                    if (href.rfind("http://",0) == 0 || href.rfind("https://",0) == 0) return href;
                                    size_t s = base.find("://");
                                    std::string scheme = "https";
                                    std::string host = base;
                                    if (s != std::string::npos) {
                                        scheme = base.substr(0, s);
                                        size_t start = s + 3;
                                        size_t end = base.find('/', start);
                                        host = (end == std::string::npos) ? base.substr(start) : base.substr(start, end - start);
                                    }
                                    if (href.rfind("//",0) == 0) return scheme + ":" + href;
                                    if (href.rfind("/",0) == 0) return scheme + "://" + host + href;
                                    size_t pos = base.rfind('/');
                                    std::string basepath = (pos == std::string::npos) ? base : base.substr(0, pos + 1);
                                    return basepath + href;
                                };
                                item.imageUrl = resolveRel(item.link, img);
                            }
                        }
                    }
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
