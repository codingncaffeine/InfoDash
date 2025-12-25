#include "utils/HtmlParser.hpp"
#include <libxml/parser.h>
#include <libxml/xpathInternals.h>
#include <regex>

namespace InfoDash {

HtmlParser::HtmlParser() : doc_(nullptr), xpathCtx_(nullptr) { xmlInitParser(); }
HtmlParser::~HtmlParser() { cleanup(); xmlCleanupParser(); }

void HtmlParser::cleanup() {
    if (xpathCtx_) { xmlXPathFreeContext(xpathCtx_); xpathCtx_ = nullptr; }
    if (doc_) { xmlFreeDoc(doc_); doc_ = nullptr; }
}

bool HtmlParser::parse(const std::string& html) {
    cleanup();
    doc_ = htmlReadMemory(html.c_str(), html.size(), nullptr, "UTF-8",
                          HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (!doc_) return false;
    xpathCtx_ = xmlXPathNewContext(doc_);
    return xpathCtx_ != nullptr;
}

std::string HtmlParser::nodeToText(xmlNodePtr node) {
    if (!node) return "";
    xmlChar* content = xmlNodeGetContent(node);
    if (!content) return "";
    std::string result(reinterpret_cast<char*>(content));
    xmlFree(content);
    size_t start = result.find_first_not_of(" \t\n\r");
    size_t end = result.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : result.substr(start, end - start + 1);
}

std::string HtmlParser::getTextContent(const std::string& xpath) {
    if (!xpathCtx_) return "";
    xmlXPathObjectPtr result = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(xpath.c_str()), xpathCtx_);
    if (!result) return "";
    std::string text;
    if (result->nodesetval && result->nodesetval->nodeNr > 0)
        text = nodeToText(result->nodesetval->nodeTab[0]);
    xmlXPathFreeObject(result);
    return text;
}

std::vector<std::string> HtmlParser::getTextContents(const std::string& xpath) {
    std::vector<std::string> texts;
    if (!xpathCtx_) return texts;
    xmlXPathObjectPtr result = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(xpath.c_str()), xpathCtx_);
    if (!result) return texts;
    if (result->nodesetval)
        for (int i = 0; i < result->nodesetval->nodeNr; ++i)
            texts.push_back(nodeToText(result->nodesetval->nodeTab[i]));
    xmlXPathFreeObject(result);
    return texts;
}

std::string HtmlParser::getAttribute(const std::string& xpath, const std::string& attr) {
    if (!xpathCtx_) return "";
    xmlXPathObjectPtr result = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(xpath.c_str()), xpathCtx_);
    if (!result) return "";
    std::string value;
    if (result->nodesetval && result->nodesetval->nodeNr > 0) {
        xmlChar* attrVal = xmlGetProp(result->nodesetval->nodeTab[0], reinterpret_cast<const xmlChar*>(attr.c_str()));
        if (attrVal) { value = reinterpret_cast<char*>(attrVal); xmlFree(attrVal); }
    }
    xmlXPathFreeObject(result);
    return value;
}

// Extract image URL from HTML content (description field often contains images)
static std::string extractImageFromHtml(const std::string& html) {
    // Try to find img src
    std::regex imgRegex(R"(<img[^>]+src\s*=\s*[\"']([^\"']+)[\"'])");
    std::smatch match;
    if (std::regex_search(html, match, imgRegex)) {
        return match[1].str();
    }
    return "";
}

std::vector<std::map<std::string, std::string>> HtmlParser::parseRSSItems(const std::string& xml) {
    std::vector<std::map<std::string, std::string>> items;
    xmlDocPtr doc = xmlReadMemory(xml.c_str(), xml.size(), nullptr, "UTF-8",
                                   XML_PARSE_RECOVER | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc) return items;
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) { xmlFreeDoc(doc); return items; }

    // Register common namespaces
    xmlXPathRegisterNs(ctx, reinterpret_cast<const xmlChar*>("media"), 
                      reinterpret_cast<const xmlChar*>("http://search.yahoo.com/mrss/"));
    xmlXPathRegisterNs(ctx, reinterpret_cast<const xmlChar*>("content"),
                      reinterpret_cast<const xmlChar*>("http://purl.org/rss/1.0/modules/content/"));
    xmlXPathRegisterNs(ctx, reinterpret_cast<const xmlChar*>("dc"),
                      reinterpret_cast<const xmlChar*>("http://purl.org/dc/elements/1.1/"));

    xmlXPathObjectPtr result = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//item"), ctx);
    bool isAtom = false;
    
    if (!result || !result->nodesetval || result->nodesetval->nodeNr == 0) {
        if (result) xmlXPathFreeObject(result);
        xmlXPathRegisterNs(ctx, reinterpret_cast<const xmlChar*>("atom"),
                          reinterpret_cast<const xmlChar*>("http://www.w3.org/2005/Atom"));
        result = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//atom:entry"), ctx);
        isAtom = true;
    }

    if (result && result->nodesetval) {
        for (int i = 0; i < result->nodesetval->nodeNr; ++i) {
            xmlNodePtr itemNode = result->nodesetval->nodeTab[i];
            std::map<std::string, std::string> item;
            std::string descriptionHtml;
            
            for (xmlNodePtr child = itemNode->children; child; child = child->next) {
                if (child->type != XML_ELEMENT_NODE) continue;
                
                std::string name(reinterpret_cast<const char*>(child->name));
                std::string content = nodeToText(child);
                
                // Get namespace
                std::string ns = child->ns && child->ns->prefix ? 
                    reinterpret_cast<const char*>(child->ns->prefix) : "";
                
                if (name == "title") {
                    item["title"] = content;
                }
                else if (name == "link") {
                    xmlChar* href = xmlGetProp(child, reinterpret_cast<const xmlChar*>("href"));
                    if (href) {
                        item["link"] = reinterpret_cast<char*>(href);
                        xmlFree(href);
                    } else if (!content.empty()) {
                        item["link"] = content;
                    }
                }
                else if (name == "description" || name == "summary") {
                    item["description"] = content;
                    descriptionHtml = content;
                }
                else if (name == "encoded" && ns == "content") {
                    // content:encoded often has full HTML with images
                    descriptionHtml = content;
                    if (item["description"].empty()) item["description"] = content;
                }
                else if (name == "pubDate" || name == "published" || name == "updated" || name == "date") {
                    item["pubDate"] = content;
                }
                else if (name == "creator" || name == "author") {
                    item["author"] = content;
                }
                else if (name == "enclosure") {
                    // RSS enclosure for media
                    xmlChar* type = xmlGetProp(child, reinterpret_cast<const xmlChar*>("type"));
                    if (type) {
                        std::string typeStr(reinterpret_cast<char*>(type));
                        xmlFree(type);
                        if (typeStr.find("image") != std::string::npos) {
                            xmlChar* url = xmlGetProp(child, reinterpret_cast<const xmlChar*>("url"));
                            if (url) {
                                item["imageUrl"] = reinterpret_cast<char*>(url);
                                xmlFree(url);
                            }
                        }
                    }
                }
                else if ((name == "thumbnail" || name == "content") && ns == "media") {
                    // media:thumbnail or media:content
                    xmlChar* url = xmlGetProp(child, reinterpret_cast<const xmlChar*>("url"));
                    if (url) {
                        item["imageUrl"] = reinterpret_cast<char*>(url);
                        xmlFree(url);
                    }
                }
                else if (name == "image") {
                    // Some feeds have direct image element
                    xmlChar* href = xmlGetProp(child, reinterpret_cast<const xmlChar*>("href"));
                    if (href) {
                        item["imageUrl"] = reinterpret_cast<char*>(href);
                        xmlFree(href);
                    } else if (!content.empty() && content.find("http") == 0) {
                        item["imageUrl"] = content;
                    }
                }
            }
            
            // If no image found, try to extract from description HTML
            if (item["imageUrl"].empty() && !descriptionHtml.empty()) {
                item["imageUrl"] = extractImageFromHtml(descriptionHtml);
            }
            
            // Strip HTML from description for display
            if (!item["description"].empty()) {
                std::string& desc = item["description"];
                // Simple HTML tag removal
                std::regex tagRegex("<[^>]*>");
                desc = std::regex_replace(desc, tagRegex, "");
                // Decode common entities
                size_t pos;
                while ((pos = desc.find("&amp;")) != std::string::npos) desc.replace(pos, 5, "&");
                while ((pos = desc.find("&lt;")) != std::string::npos) desc.replace(pos, 4, "<");
                while ((pos = desc.find("&gt;")) != std::string::npos) desc.replace(pos, 4, ">");
                while ((pos = desc.find("&quot;")) != std::string::npos) desc.replace(pos, 6, "\"");
                while ((pos = desc.find("&nbsp;")) != std::string::npos) desc.replace(pos, 6, " ");
                while ((pos = desc.find("&#39;")) != std::string::npos) desc.replace(pos, 5, "'");
                // Trim
                size_t start = desc.find_first_not_of(" \t\n\r");
                size_t end = desc.find_last_not_of(" \t\n\r");
                if (start != std::string::npos) desc = desc.substr(start, end - start + 1);
                // Truncate long descriptions
                if (desc.length() > 200) desc = desc.substr(0, 200) + "...";
            }
            
            if (!item["title"].empty()) items.push_back(item);
        }
    }
    if (result) xmlXPathFreeObject(result);
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);
    return items;
}

}
