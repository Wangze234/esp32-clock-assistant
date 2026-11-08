// secrets.example.h — 复制为 secrets.h 后填入真实值
// secrets.h 不要分享、不要上传到任何公开位置。

#pragma once

#define WIFI_SSID       "你的WiFi名称"
#define WIFI_PASSWORD   "你的WiFi密码"

// 和风天气 https://dev.qweather.com/
#define QWEATHER_KEY    "你的API_KEY"
// 自动定位失败时的兜底城市 ID（北京 101010100）；平时用公网 IP 自动定位
#define QWEATHER_CITY   "101010100"
// 可选：1 = 自动定位（默认），0 = 只用上面的 QWEATHER_CITY
// #define QWEATHER_AUTO_LOCATION 0
// 2024 之后新版和风天气要求用"专属 API Host"（控制台首页能看到），
// 形如 xxxxxxxx.re.qweatherapi.com，不能再用 devapi.qweather.com
#define QWEATHER_HOST   "你的专属ApiHost.re.qweatherapi.com"
