#include "services/WeatherService.hpp"
#include "utils/HttpClient.hpp"
#include "utils/HtmlParser.hpp"
#include "utils/Config.hpp"
#include <thread>
#include <regex>

namespace InfoDash {

WeatherService::WeatherService() {}

void WeatherService::fetchWeather(const std::string& zipCode, std::function<void(WeatherData)> callback) {
    std::thread([zipCode, callback]() {
        WeatherData data;
        data.zipCode = zipCode;
        HttpClient client;
        
        // Scrape weather from wttr.in (no API key needed)
        std::string url = "https://wttr.in/" + zipCode + "?format=%l|%t|%C|%h|%w";
        auto response = client.get(url);
        
        if (response.success && !response.body.empty()) {
            // Parse: Location|Temp|Condition|Humidity|Wind
            std::vector<std::string> parts;
            std::string part;
            for (char c : response.body) {
                if (c == '|') { parts.push_back(part); part.clear(); }
                else if (c != '\n') part += c;
            }
            if (!part.empty()) parts.push_back(part);
            
            if (parts.size() >= 5) {
                data.location = parts[0];
                data.temperature = parts[1];
                data.condition = parts[2];
                data.humidity = parts[3];
                data.wind = parts[4];
            }
        }
        
        // Get forecast
        std::string forecastUrl = "https://wttr.in/" + zipCode + "?format=%t|%C\\n&n1";
        auto forecastResp = client.get("https://wttr.in/" + zipCode + "?format=j1");
        if (forecastResp.success) {
            // Simple parsing for demo - in production parse JSON properly
            data.forecast = {
                {"Today", data.temperature, "--", data.condition},
                {"Tomorrow", "--", "--", "..."},
                {"Day 3", "--", "--", "..."}
            };
        }
        
        callback(data);
    }).detach();
}

void WeatherService::fetchAllLocations(std::function<void(std::vector<WeatherData>)> callback) {
    // Use single location from config
    std::string location = Config::getInstance().getWeatherLocation();
    if (location.empty()) location = "auto";
    
    auto results = std::make_shared<std::vector<WeatherData>>();
    
    fetchWeather(location, [results, callback](WeatherData data) {
        results->push_back(data);
        callback(*results);
    });
}

}
