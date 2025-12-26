# InfoDash v0.07

A modern GTK4 dashboard application for Linux that displays RSS feeds, weather, and stock data.

## Features

### RSS Feed Reader
- Feedly-style card layout with thumbnail images
- Category organization with sidebar navigation
- Read/unread tracking with visual indicators
- Save articles for later reading
- Feed management (add/remove feeds, assign categories)
- Right-click context menu for article actions
- Mark all as read functionality

### Weather Panel
- Multiple location support (add cities by name or ZIP code)
- Automatic country detection (e.g., "London, United Kingdom")
- Current conditions with weather icons (sunny, cloudy, rain, snow, storm)
- "Feels like" temperature display
- 3-day forecast with condition icons
- Temperature unit toggle (Celsius/Fahrenheit)
- Weather alerts display (tornado warnings, etc.)
- Loading indicator while fetching data
- Humidity and wind information

### Stock Ticker
- Real-time stock quotes
- Add/remove stock symbols
- Price change indicators (green/red)
- Percentage change display

## Dependencies (Arch/EndeavourOS)

```bash
sudo pacman -S gtk4 libcurl libxml2 json-glib cmake base-devel
```

## Build

```bash
cd InfoDash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Run

```bash
./InfoDash
```

## Configuration

Config stored at `~/.config/infodash/config.json`

## Changelog

### v0.07
- Added multiple weather location support
- Weather now shows city name with country (e.g., "London, United Kingdom")
- Added weather condition icons (sun, clouds, rain, snow, thunderstorm)
- Added 3-day forecast with icons
- Added "feels like" temperature display
- Added loading spinner while fetching weather data
- Added weather alerts support (tornado warnings, etc.)
- Temperature unit toggle (°F/°C)
- Fixed Pango UTF-8 warnings in RSS and Weather services
- Fixed GTK CSS border property warnings
- Improved weather data parsing from wttr.in JSON API

### v0.06
- RSS feed improvements
- Article read tracking
- Category management

### v0.05
- Initial public release
- RSS feeds, weather, stocks dashboard
