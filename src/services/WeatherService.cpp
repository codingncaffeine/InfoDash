#include "services/WeatherService.hpp"
#include "utils/HttpClient.hpp"
#include "utils/Config.hpp"
#include <json-glib/json-glib.h>
#include <thread>
#include <regex>
#include <cmath>
#include <mutex>
#include <atomic>

namespace InfoDash {

WeatherService::WeatherService() {}

// Convert Celsius to Fahrenheit
static std::string celsiusToFahrenheit(const std::string& celsius) {
    try {
        int c = std::stoi(celsius);
        int f = static_cast<int>(std::round(c * 9.0 / 5.0 + 32));
        return std::to_string(f);
    } catch (...) {
        return celsius;
    }
}

// Format temperature with unit
static std::string formatTemp(const std::string& celsius, TempUnit unit) {
    if (unit == TempUnit::Fahrenheit) {
        return celsiusToFahrenheit(celsius) + "F";
    }
    return celsius + "C";
}

// Sanitize string to valid UTF-8
static std::string sanitizeUtf8(const char* str) {
    if (!str) return "";
    
    std::string result;
    const char* p = str;
    while (*p) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x80) {
            result += *p;
            p++;
        } else if ((c & 0xE0) == 0xC0 && p[1]) {
            if ((p[1] & 0xC0) == 0x80) {
                result += p[0];
                result += p[1];
                p += 2;
            } else {
                p++;
            }
        } else if ((c & 0xF0) == 0xE0 && p[1] && p[2]) {
            if ((p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
                result += p[0];
                result += p[1];
                result += p[2];
                p += 3;
            } else {
                p++;
            }
        } else if ((c & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
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
            p++;
        }
    }
    return result;
}

static std::string safeGetString(JsonObject* obj, const char* member) {
    if (!json_object_has_member(obj, member)) return "";
    const char* val = json_object_get_string_member(obj, member);
    return sanitizeUtf8(val);
}

// URL-encode a string for use in URLs
static std::string urlEncode(const std::string& str) {
    std::string result;
    for (unsigned char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else if (c == ' ') {
            result += "%20";
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            result += buf;
        }
    }
    return result;
}

// Map wttr.in weather codes to icon names
std::string WeatherService::getWeatherIcon(const std::string& conditionCode) {
    // wttr.in weather codes: https://github.com/chubin/wttr.in/blob/master/lib/constants.py
    int code = 0;
    try { code = std::stoi(conditionCode); } catch (...) { return "weather-few-clouds-symbolic"; }
    
    // Sunny/Clear
    if (code == 113) return "weather-clear-symbolic";
    // Partly cloudy
    if (code == 116) return "weather-few-clouds-symbolic";
    // Cloudy
    if (code == 119) return "weather-overcast-symbolic";
    // Very cloudy
    if (code == 122) return "weather-overcast-symbolic";
    // Fog/Mist
    if (code == 143 || code == 248 || code == 260) return "weather-fog-symbolic";
    // Light rain/drizzle
    if (code == 176 || code == 263 || code == 266 || code == 293 || code == 296 || code == 353) 
        return "weather-showers-scattered-symbolic";
    // Rain
    if (code == 299 || code == 302 || code == 305 || code == 308 || code == 356 || code == 359) 
        return "weather-showers-symbolic";
    // Snow
    if (code == 179 || code == 182 || code == 185 || code == 227 || code == 230 ||
        code == 317 || code == 320 || code == 323 || code == 326 || code == 329 ||
        code == 332 || code == 335 || code == 338 || code == 350 || code == 362 ||
        code == 365 || code == 368 || code == 371 || code == 374 || code == 377)
        return "weather-snow-symbolic";
    // Thunderstorm
    if (code == 200 || code == 386 || code == 389 || code == 392 || code == 395)
        return "weather-storm-symbolic";
    
    return "weather-few-clouds-symbolic";
}

void WeatherService::fetchWeather(const std::string& zipCode, std::function<void(WeatherData)> callback) {
    std::thread([zipCode, callback]() {
        WeatherData data;
        data.zipCode = zipCode;
        HttpClient client;
        TempUnit unit = Config::getInstance().getTempUnit();
        
        // URL-encode the location for the API request
        std::string encodedLocation = urlEncode(zipCode);
        
        // Fetch JSON format from wttr.in
        std::string url = "https://wttr.in/" + encodedLocation + "?format=j1";
        auto response = client.get(url);
        
        if (response.success && !response.body.empty()) {
            JsonParser* parser = json_parser_new();
            GError* error = nullptr;
            
            if (json_parser_load_from_data(parser, response.body.c_str(), -1, &error)) {
                JsonNode* root = json_parser_get_root(parser);
                if (root && JSON_NODE_HOLDS_OBJECT(root)) {
                    JsonObject* obj = json_node_get_object(root);
                    
                    // Get current conditions
                    if (json_object_has_member(obj, "current_condition")) {
                        JsonArray* current = json_object_get_array_member(obj, "current_condition");
                        if (json_array_get_length(current) > 0) {
                            JsonObject* cc = json_array_get_object_element(current, 0);
                            
                            std::string tempC = safeGetString(cc, "temp_C");
                            data.temperature = formatTemp(tempC, unit);
                            
                            std::string feelsC = safeGetString(cc, "FeelsLikeC");
                            data.feelsLike = formatTemp(feelsC, unit);
                            
                            // Get weather code for icon
                            data.conditionCode = safeGetString(cc, "weatherCode");
                            
                            // Get weather description
                            if (json_object_has_member(cc, "weatherDesc")) {
                                JsonArray* descArr = json_object_get_array_member(cc, "weatherDesc");
                                if (json_array_get_length(descArr) > 0) {
                                    JsonObject* descObj = json_array_get_object_element(descArr, 0);
                                    data.condition = safeGetString(descObj, "value");
                                }
                            }
                            
                            data.humidity = safeGetString(cc, "humidity") + "%";
                            
                            std::string windSpeed = safeGetString(cc, "windspeedMiles");
                            std::string windDir = safeGetString(cc, "winddir16Point");
                            data.wind = windSpeed + " mph " + windDir;
                        }
                    }
                    
                    // Get location name and country
                    if (json_object_has_member(obj, "nearest_area")) {
                        JsonArray* areas = json_object_get_array_member(obj, "nearest_area");
                        if (json_array_get_length(areas) > 0) {
                            JsonObject* area = json_array_get_object_element(areas, 0);
                            
                            std::string city = "";
                            std::string country = "";
                            
                            if (json_object_has_member(area, "areaName")) {
                                JsonArray* nameArr = json_object_get_array_member(area, "areaName");
                                if (json_array_get_length(nameArr) > 0) {
                                    JsonObject* nameObj = json_array_get_object_element(nameArr, 0);
                                    city = safeGetString(nameObj, "value");
                                }
                            }
                            if (json_object_has_member(area, "country")) {
                                JsonArray* countryArr = json_object_get_array_member(area, "country");
                                if (json_array_get_length(countryArr) > 0) {
                                    JsonObject* countryObj = json_array_get_object_element(countryArr, 0);
                                    country = safeGetString(countryObj, "value");
                                }
                            }
                            
                            data.location = city;
                            data.country = country;
                        }
                    }
                    
                    // Get forecast (wttr.in provides 3 days)
                    if (json_object_has_member(obj, "weather")) {
                        JsonArray* weather = json_object_get_array_member(obj, "weather");
                        guint numDays = json_array_get_length(weather);
                        
                        const char* dayNames[] = {"Today", "Tomorrow"};
                        
                        for (guint i = 0; i < numDays && i < 3; i++) {
                            JsonObject* day = json_array_get_object_element(weather, i);
                            WeatherData::Forecast f;
                            
                            // Get date and convert to day name
                            if (i < 2) {
                                f.day = dayNames[i];
                            } else {
                                std::string dateStr = safeGetString(day, "date");
                                f.day = "Day 3";
                                
                                if (!dateStr.empty()) {
                                    int year, month, dayNum;
                                    if (sscanf(dateStr.c_str(), "%d-%d-%d", &year, &month, &dayNum) == 3) {
                                        if (month < 3) { month += 12; year--; }
                                        int dow = (dayNum + 13*(month+1)/5 + year + year/4 - year/100 + year/400) % 7;
                                        const char* weekDays[] = {"Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri"};
                                        f.day = weekDays[dow];
                                    }
                                }
                            }
                            
                            std::string maxC = safeGetString(day, "maxtempC");
                            std::string minC = safeGetString(day, "mintempC");
                            f.high = formatTemp(maxC, unit);
                            f.low = formatTemp(minC, unit);
                            
                            // Get condition for the day (from hourly, use midday)
                            if (json_object_has_member(day, "hourly")) {
                                JsonArray* hourly = json_object_get_array_member(day, "hourly");
                                guint idx = (json_array_get_length(hourly) > 4) ? 4 : 0;
                                JsonObject* hour = json_array_get_object_element(hourly, idx);
                                
                                f.conditionCode = safeGetString(hour, "weatherCode");
                                
                                if (json_object_has_member(hour, "weatherDesc")) {
                                    JsonArray* descArr = json_object_get_array_member(hour, "weatherDesc");
                                    if (json_array_get_length(descArr) > 0) {
                                        JsonObject* descObj = json_array_get_object_element(descArr, 0);
                                        f.condition = safeGetString(descObj, "value");
                                    }
                                }
                            }
                            
                            data.forecast.push_back(f);
                        }
                    }
                    
                    // Get weather alerts if available
                    if (json_object_has_member(obj, "alerts")) {
                        JsonNode* alertsNode = json_object_get_member(obj, "alerts");
                        if (JSON_NODE_HOLDS_OBJECT(alertsNode)) {
                            JsonObject* alertsObj = json_node_get_object(alertsNode);
                            if (json_object_has_member(alertsObj, "alert")) {
                                JsonArray* alertArr = json_object_get_array_member(alertsObj, "alert");
                                guint numAlerts = json_array_get_length(alertArr);
                                for (guint i = 0; i < numAlerts && i < 5; i++) {
                                    JsonObject* alert = json_array_get_object_element(alertArr, i);
                                    WeatherData::Alert a;
                                    a.headline = safeGetString(alert, "headline");
                                    a.severity = safeGetString(alert, "severity");
                                    a.description = safeGetString(alert, "desc");
                                    a.expires = safeGetString(alert, "expires");
                                    if (!a.headline.empty()) {
                                        data.alerts.push_back(a);
                                    }
                                }
                            }
                        } else if (JSON_NODE_HOLDS_ARRAY(alertsNode)) {
                            JsonArray* alertArr = json_node_get_array(alertsNode);
                            guint numAlerts = json_array_get_length(alertArr);
                            for (guint i = 0; i < numAlerts && i < 5; i++) {
                                JsonObject* alert = json_array_get_object_element(alertArr, i);
                                WeatherData::Alert a;
                                a.headline = safeGetString(alert, "headline");
                                a.severity = safeGetString(alert, "severity");
                                a.description = safeGetString(alert, "desc");
                                a.expires = safeGetString(alert, "expires");
                                if (!a.headline.empty()) {
                                    data.alerts.push_back(a);
                                }
                            }
                        }
                    }
                }
            }
            
            if (error) g_error_free(error);
            g_object_unref(parser);
        }
        
        // Fallback if JSON parsing failed
        if (data.temperature.empty()) {
            std::string simpleUrl = "https://wttr.in/" + encodedLocation + "?format=%l|%t|%C|%h|%w";
            auto simpleResp = client.get(simpleUrl);
            
            if (simpleResp.success && !simpleResp.body.empty()) {
                std::vector<std::string> parts;
                std::string part;
                for (char c : simpleResp.body) {
                    if (c == '|') { parts.push_back(part); part.clear(); }
                    else if (c != '\n') part += c;
                }
                if (!part.empty()) parts.push_back(part);
                
                if (parts.size() >= 5) {
                    data.location = sanitizeUtf8(parts[0].c_str());
                    data.temperature = sanitizeUtf8(parts[1].c_str());
                    data.condition = sanitizeUtf8(parts[2].c_str());
                    data.humidity = sanitizeUtf8(parts[3].c_str());
                    data.wind = sanitizeUtf8(parts[4].c_str());
                }
            }
        }
        
        callback(data);
    }).detach();
}

void WeatherService::fetchAllLocations(std::function<void(std::vector<WeatherData>)> callback) {
    auto locations = Config::getInstance().getWeatherLocations();
    if (locations.empty()) {
        locations.push_back("auto");
    }
    
    auto results = std::make_shared<std::vector<WeatherData>>();
    auto mutex = std::make_shared<std::mutex>();
    auto remaining = std::make_shared<std::atomic<size_t>>(locations.size());
    
    for (const auto& location : locations) {
        fetchWeather(location, [results, mutex, remaining, callback](WeatherData data) {
            {
                std::lock_guard<std::mutex> lock(*mutex);
                results->push_back(data);
            }
            
            size_t left = --(*remaining);
            if (left == 0) {
                callback(*results);
            }
        });
    }
}

}
