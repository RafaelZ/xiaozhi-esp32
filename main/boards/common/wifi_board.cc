// 包含 wifi_board.h 头文件，用于获取 WifiBoard 类的定义。
#include "wifi_board.h"

// 包含其他所需的头文件，这些头文件定义了应用中使用的各种模块和工具。
#include "display.h"
#include "application.h"
#include "system_info.h"
#include "font_awesome_symbols.h"
#include "settings.h"
#include "assets/lang_config.h"

// 包含 FreeRTOS 相关的头文件，用于多任务管理。
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
// 包含 ESP-IDF 的网络和传输层相关头文件。
#include <esp_http.h>
#include <esp_mqtt.h>
#include <esp_udp.h>
#include <tcp_transport.h>
#include <tls_transport.h>
#include <web_socket.h>
#include <esp_log.h>

// 包含自定义的 Wi-Fi 模块头文件。
#include <wifi_station.h>
#include <wifi_configuration_ap.h>
#include <ssid_manager.h>

// 定义日志标签，用于 ESP_LOGI 等日志输出函数。
static const char *TAG = "WifiBoard";

// WifiBoard 类的构造函数。
WifiBoard::WifiBoard() {
    // 创建一个 Settings 实例，用于读取和写入 Wi-Fi 配置。
    // "wifi" 是命名空间，true 表示持久化设置。
    Settings settings("wifi", true);
    // 从设置中获取 "force_ap" 的值，如果为 1，则进入 Wi-Fi 配置模式。
    wifi_config_mode_ = settings.GetInt("force_ap") == 1;
    // 如果 force_ap 被设置为 1（强制进入 AP 模式）。
    if (wifi_config_mode_) {
        // 打印日志信息。
        ESP_LOGI(TAG, "force_ap is set to 1, reset to 0");
        // 将 force_ap 重置为 0，避免下次启动时继续强制进入 AP 模式。
        settings.SetInt("force_ap", 0);
    }
}

// 获取板子类型字符串的函数，返回 "wifi"。
std::string WifiBoard::GetBoardType() {
    return "wifi";
}

// 进入 Wi-Fi 配置模式的函数。
void WifiBoard::EnterWifiConfigMode() {
    // 获取 Application 的单例实例。
    auto& application = Application::GetInstance();
    // 设置设备状态为 Wi-Fi 配置中。
    application.SetDeviceState(kDeviceStateWifiConfiguring);

    // 获取 WifiConfigurationAp 的单例实例。
    auto& wifi_ap = WifiConfigurationAp::GetInstance();
    // 设置 Wi-Fi AP 配置页面的语言。
    wifi_ap.SetLanguage(Lang::CODE);
    // 设置 AP 模式下 SSID 的前缀。
    wifi_ap.SetSsidPrefix("Xiaozhi");
    // 启动 Wi-Fi AP 模式。
    wifi_ap.Start();

    // 显示 WiFi 配置 AP 的 SSID 和 Web 服务器 URL 的提示信息。
    std::string hint = Lang::Strings::CONNECT_TO_HOTSPOT; // "请连接到热点: "
    hint += wifi_ap.GetSsid();                            // 添加 AP 的 SSID
    hint += Lang::Strings::ACCESS_VIA_BROWSER;           // "并访问网页: "
    hint += wifi_ap.GetWebServerUrl();                   // 添加 Web 服务器 URL
    hint += "\n\n";                                    // 添加换行符
    
    // 播报配置 WiFi 的提示，包括文本提示和提示音。
    application.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "", Lang::Sounds::P3_WIFICONFIG);
    
    // 循环等待，直到设备在配置完成后被重置。这个循环会一直运行。
    while (true) {
        // 获取内部 SRAM 的空闲大小。
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        // 获取内部 SRAM 的最小空闲大小（历史最低值）。
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        // 打印内存使用情况的日志。
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);
        // 延迟 10 秒，避免忙等待。
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// 启动网络连接的函数。
void WifiBoard::StartNetwork() {
    // 用户可以在启动时按下 BOOT 按钮进入 WiFi 配置模式。
    if (wifi_config_mode_) {
        // 如果处于强制 AP 模式，则进入 Wi-Fi 配置模式。
        EnterWifiConfigMode();
        return;
    }

    // 如果没有配置 WiFi SSID，则进入 WiFi 配置模式。
    auto& ssid_manager = SsidManager::GetInstance();
    // 获取已保存的 SSID 列表。
    auto ssid_list = ssid_manager.GetSsidList();
    // 如果 SSID 列表为空，表示没有配置 Wi-Fi。
    if (ssid_list.empty()) {
        // 设置为 Wi-Fi 配置模式。
        wifi_config_mode_ = true;
        // 进入 Wi-Fi 配置模式。
        EnterWifiConfigMode();
        return;
    }

    // 获取 WifiStation 的单例实例。
    auto& wifi_station = WifiStation::GetInstance();
    // 设置扫描开始时的回调函数：显示 "正在扫描 Wi-Fi" 通知。
    wifi_station.OnScanBegin([this]() {
        auto display = Board::GetInstance().GetDisplay();
        display->ShowNotification(Lang::Strings::SCANNING_WIFI, 30000);
    });
    // 设置连接尝试时的回调函数：显示 "正在连接到 [SSID]..." 通知。
    wifi_station.OnConnect([this](const std::string& ssid) {
        auto display = Board::GetInstance().GetDisplay();
        std::string notification = Lang::Strings::CONNECT_TO;
        notification += ssid;
        notification += "...";
        display->ShowNotification(notification.c_str(), 30000);
    });
    // 设置连接成功时的回调函数：显示 "已连接到 [SSID]" 通知。
    wifi_station.OnConnected([this](const std::string& ssid) {
        auto display = Board::GetInstance().GetDisplay();
        std::string notification = Lang::Strings::CONNECTED_TO;
        notification += ssid;
        display->ShowNotification(notification.c_str(), 30000);
    });
    // 启动 Wi-Fi 站模式（尝试连接到已保存的 AP）。
    wifi_station.Start();

    // 尝试连接到 WiFi，如果连接失败，则启动 WiFi 配置 AP 模式。
    if (!wifi_station.WaitForConnected(60 * 1000)) { // 等待 60 秒连接成功
        // 如果连接失败，停止 Wi-Fi 站模式。
        wifi_station.Stop();
        // 设置为 Wi-Fi 配置模式。
        wifi_config_mode_ = true;
        // 进入 Wi-Fi 配置模式。
        EnterWifiConfigMode();
        return;
    }
}

// 创建并返回一个 EspHttp 实例的函数。
Http* WifiBoard::CreateHttp() {
    return new EspHttp();
}

// 创建并返回一个 WebSocket 实例的函数。
WebSocket* WifiBoard::CreateWebSocket() {
    // 创建一个 Settings 实例，用于读取 WebSocket 配置。
    Settings settings("websocket", false);
    // 获取 WebSocket URL。
    std::string url = settings.GetString("url");
    // 根据 URL 判断是使用 TLS 传输还是 TCP 传输。
    if (url.find("wss://") == 0) {
        // 如果 URL 以 "wss://" 开头，则使用 TLS 传输。
        return new WebSocket(new TlsTransport());
    } else {
        // 否则，使用 TCP 传输。
        return new WebSocket(new TcpTransport());
    }
    // 理论上不会执行到这里，因为上面两个分支都会返回。
    return nullptr;
}

// 创建并返回一个 EspMqtt 实例的函数。
Mqtt* WifiBoard::CreateMqtt() {
    return new EspMqtt();
}

// 创建并返回一个 EspUdp 实例的函数。
Udp* WifiBoard::CreateUdp() {
    return new EspUdp();
}

// 获取网络状态图标的函数。
const char* WifiBoard::GetNetworkStateIcon() {
    // 如果处于 Wi-Fi 配置模式，返回 Wi-Fi 图标。
    if (wifi_config_mode_) {
        return FONT_AWESOME_WIFI;
    }
    // 获取 WifiStation 的单例实例。
    auto& wifi_station = WifiStation::GetInstance();
    // 如果 Wi-Fi 未连接，返回 Wi-Fi 关闭图标。
    if (!wifi_station.IsConnected()) {
        return FONT_AWESOME_WIFI_OFF;
    }
    // 获取 RSSI 值（信号强度）。
    int8_t rssi = wifi_station.GetRssi();
    // 根据 RSSI 值返回不同的 Wi-Fi 信号强度图标。
    if (rssi >= -60) {
        return FONT_AWESOME_WIFI;       // 信号强
    } else if (rssi >= -70) {
        return FONT_AWESOME_WIFI_FAIR;  // 信号一般
    } else {
        return FONT_AWESOME_WIFI_WEAK;  // 信号弱
    }
}

// 获取板子 JSON 字符串的函数。
std::string WifiBoard::GetBoardJson() {
    // 设置 OTA 的板子类型。
    auto& wifi_station = WifiStation::GetInstance();
    // 构建 JSON 字符串的开始部分，包含 type 和 name。
    std::string board_json = std::string("{\"type\":\"" BOARD_TYPE "\",");
    board_json += "\"name\":\"" BOARD_NAME "\",";
    // 如果不处于 Wi-Fi 配置模式，则添加 Wi-Fi 连接信息。
    if (!wifi_config_mode_) {
        board_json += "\"ssid\":\"" + wifi_station.GetSsid() + "\",";             // 添加 SSID
        board_json += "\"rssi\":" + std::to_string(wifi_station.GetRssi()) + ","; // 添加 RSSI
        board_json += "\"channel\":" + std::to_string(wifi_station.GetChannel()) + ","; // 添加频道
        board_json += "\"ip\":\"" + wifi_station.GetIpAddress() + "\",";         // 添加 IP 地址
    }
    // 添加 MAC 地址并结束 JSON 字符串。
    board_json += "\"mac\":\"" + SystemInfo::GetMacAddress() + "\"}";
    return board_json;
}

// 设置电源保存模式的函数。
void WifiBoard::SetPowerSaveMode(bool enabled) {
    // 获取 WifiStation 的单例实例。
    auto& wifi_station = WifiStation::GetInstance();
    // 设置 Wi-Fi 站的电源保存模式。
    wifi_station.SetPowerSaveMode(enabled);
}

// 重置 Wi-Fi 配置的函数。
void WifiBoard::ResetWifiConfiguration() {
    // 设置一个标志并重启设备，以进入网络配置模式。
    {
        // 创建一个 Settings 实例，用于设置 Wi-Fi 配置。
        Settings settings("wifi", true);
        // 将 "force_ap" 设置为 1，强制下次启动进入 AP 模式。
        settings.SetInt("force_ap", 1);
    }
    // 显示 "正在进入 Wi-Fi 配置模式" 通知。
    GetDisplay()->ShowNotification(Lang::Strings::ENTERING_WIFI_CONFIG_MODE);
    // 延迟 1 秒。
    vTaskDelay(pdMS_TO_TICKS(1000));
    // 重启设备。
    esp_restart();
}

// 获取设备状态 JSON 字符串的函数。
std::string WifiBoard::GetDeviceStatusJson() {
    /*
     * 返回设备状态JSON
     * 
     * 返回的JSON结构如下：
     * {
     *     "audio_speaker": {         // 音频扬声器状态
     *         "volume": 70          // 音量
     *     },
     *     "screen": {                // 屏幕状态
     *         "brightness": 100     // 亮度
     *         "theme": "light"      // 主题
     *     },
     *     "battery": {               // 电池状态
     *         "level": 50,          // 电量水平
     *         "charging": true      // 是否正在充电
     *     },
     *     "network": {               // 网络状态
     *         "type": "wifi",       // 网络类型
     *         "ssid": "Xiaozhi",    // 连接的 SSID
     *         "rssi": -60           // 信号强度
     *     },
     *     "chip": {                  // 芯片状态
     *         "temperature": 25     // 芯片温度
     *     }
     * }
     */
    // 获取 Board 的单例实例。
    auto& board = Board::GetInstance();
    // 创建一个 JSON 根对象。
    auto root = cJSON_CreateObject();

    // 处理音频扬声器状态。
    auto audio_speaker = cJSON_CreateObject();
    // 获取音频编解码器实例。
    auto audio_codec = board.GetAudioCodec();
    // 如果音频编解码器存在，则添加音量信息。
    if (audio_codec) {
        cJSON_AddNumberToObject(audio_speaker, "volume", audio_codec->output_volume());
    }
    // 将音频扬声器对象添加到根 JSON 对象。
    cJSON_AddItemToObject(root, "audio_speaker", audio_speaker);

    // 处理屏幕亮度状态。
    auto backlight = board.GetBacklight();
    // 创建一个 JSON 屏幕对象。
    auto screen = cJSON_CreateObject();
    // 如果背光对象存在，则添加亮度信息。
    if (backlight) {
        cJSON_AddNumberToObject(screen, "brightness", backlight->brightness());
    }
    // 获取显示器实例。
    auto display = board.GetDisplay();
    // 如果显示器存在且高度大于 64（通常表示 LCD 显示器），则添加主题信息。
    if (display && display->height() > 64) { // 仅适用于 LCD 显示器
        cJSON_AddStringToObject(screen, "theme", display->GetTheme().c_str());
    }
    // 将屏幕对象添加到根 JSON 对象。
    cJSON_AddItemToObject(root, "screen", screen);

    // 处理电池状态。
    int battery_level = 0; // 电池电量水平
    bool charging = false; // 是否正在充电
    bool discharging = false; // 是否正在放电
    // 尝试获取电池电量、充电和放电状态。
    if (board.GetBatteryLevel(battery_level, charging, discharging)) {
        // 创建一个 JSON 电池对象。
        cJSON* battery = cJSON_CreateObject();
        // 添加电池电量信息。
        cJSON_AddNumberToObject(battery, "level", battery_level);
        // 添加充电状态信息。
        cJSON_AddBoolToObject(battery, "charging", charging);
        // 将电池对象添加到根 JSON 对象。
        cJSON_AddItemToObject(root, "battery", battery);
    }

    // 处理网络状态。
    auto network = cJSON_CreateObject();
    // 获取 WifiStation 的单例实例。
    auto& wifi_station = WifiStation::GetInstance();
    // 添加网络类型信息为 "wifi"。
    cJSON_AddStringToObject(network, "type", "wifi");
    // 添加连接的 SSID 信息。
    cJSON_AddStringToObject(network, "ssid", wifi_station.GetSsid().c_str());
    // 获取 RSSI 值（信号强度）。
    int rssi = wifi_station.GetRssi();
    // 根据 RSSI 值添加信号强度描述。
    if (rssi >= -60) {
        cJSON_AddStringToObject(network, "signal", "strong"); // 信号强
    } else if (rssi >= -70) {
        cJSON_AddStringToObject(network, "signal", "medium"); // 信号中等
    } else {
        cJSON_AddStringToObject(network, "signal", "weak");   // 信号弱
    }
    // 将网络对象添加到根 JSON 对象。
    cJSON_AddItemToObject(root, "network", network);

    // 处理芯片状态。
    float esp32temp = 0.0f;
    // 尝试获取芯片温度。
    if (board.GetTemperature(esp32temp)) {
        // 创建一个 JSON 芯片对象。
        auto chip = cJSON_CreateObject();
        // 添加芯片温度信息。
        cJSON_AddNumberToObject(chip, "temperature", esp32temp);
        // 将芯片对象添加到根 JSON 对象。
        cJSON_AddItemToObject(root, "chip", chip);
    }

    // 将 cJSON 对象转换为未格式化的 JSON 字符串。
    auto json_str = cJSON_PrintUnformatted(root);
    // 将 C 风格字符串转换为 C++ 字符串。
    std::string json(json_str);
    // 释放 cJSON_PrintUnformatted 分配的内存。
    cJSON_free(json_str);
    // 删除 cJSON 根对象，释放所有相关内存。
    cJSON_Delete(root);
    // 返回生成的 JSON 字符串。
    return json;
}
