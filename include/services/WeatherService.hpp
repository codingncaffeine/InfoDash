#pragma once
#include <string>
#include <vector>
#include <functional>

namespace InfoDash {

struct WeatherData {
    std::string zipCode;
    std::string location;      // City name
    std::string country;       // Country name
    std::string temperature;
    std::string condition;
    std::string conditionCode; // For icon mapping
    std::string humidity;
    std::string wind;
    std::string feelsLike;
    
    struct Forecast {
        std::string day;
        std::string high;
        std::string low;
        std::string condition;
        std::string conditionCode;
    };
    std::vector<Forecast> forecast;
    
    struct Alert {
        std::string headline;
        std::string severity;
        std::string description;
        std::string expires;
    };
    std::vector<Alert> alerts;
};

class WeatherService {
public:
    WeatherService();
    void fetchWeather(const std::string& location, std::function<void(WeatherData)> callback);
    void fetchAllLocations(std::function<void(std::vector<WeatherData>)> callback);
    
    // Map condition code to icon name
    static std::string getWeatherIcon(const std::string& conditionCode);
};

}
