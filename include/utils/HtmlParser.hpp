#pragma once
#include <string>
#include <vector>
#include <map>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

namespace InfoDash {

class HtmlParser {
public:
    HtmlParser();
    ~HtmlParser();
    bool parse(const std::string& html);
    std::string getTextContent(const std::string& xpath);
    std::vector<std::string> getTextContents(const std::string& xpath);
    std::string getAttribute(const std::string& xpath, const std::string& attr);
    static std::vector<std::map<std::string, std::string>> parseRSSItems(const std::string& xml);

private:
    htmlDocPtr doc_;
    xmlXPathContextPtr xpathCtx_;
    static std::string nodeToText(xmlNodePtr node);
    void cleanup();
};

}
