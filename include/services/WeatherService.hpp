#pragma once
#include <string>
#include <vector>
#include <functional>

namespace InfoDash {

struct WeatherData {
    std::string location;
    std::string zipCode;
    std::string temperature;
    std::string condition;
    std::string humidity;
    std::string wind;
    struct Forecast { std::string day; std::string high; std::string low; std::string condition; };
    std::vector<Forecast> forecast;
};

class WeatherService {
public:
    WeatherService();
    void fetchWeather(const std::string& zipCode, std::function<void(WeatherData)> callback);
    void fetchAllLocations(std::function<void(std::vector<WeatherData>)> callback);
};

}
