// 包含 Wi-Fi 板级通用头文件
#include "boards/common/wifi_board.h"
// 包含 BluFi 辅助函数头文件
#include "blufi_helper.h"
// 包含 ESP 蓝牙栈头文件
#include "esp_bt.h"
// #include "blufi_example.h" // To be created from example
// 包含 NVS Flash (非易失性存储) 头文件
#include "nvs_flash.h"
// 包含 ESP 网络接口头文件
#include "esp_netif.h"
// 包含 ESP 事件循环头文件
#include "esp_event.h"
// 包含 FreeRTOS 核心头文件
#include "freertos/FreeRTOS.h"
// 包含 FreeRTOS 任务管理头文件
#include "freertos/task.h"
// 包含 FreeRTOS 事件组头文件
#include "freertos/event_groups.h"
// 包含 ESP Wi-Fi 库头文件
#include "esp_wifi.h"
// 包含 ESP 日志库头文件，确保 ESP_LOGI, ESP_LOGE 可用
#include "esp_log.h"
// 包含音频编解码器 ES8311 头文件
#include "audio_codecs/es8311_audio_codec.h"
// 包含 LCD 显示屏头文件
#include "display/lcd_display.h"
// 包含应用程序头文件
#include "application.h"
// 包含按钮头文件
#include "button.h"
// 包含配置头文件
#include "config.h"
// 包含 IoT 物件管理器头文件
#include "iot/thing_manager.h"
// 包含单颗 LED 控制头文件
#include "led/single_led.h"

// 包含 ESP 日志库头文件
#include <esp_log.h>
// 包含 ESP eFuse 表头文件
#include <esp_efuse_table.h>
// 包含 I2C 主机驱动头文件
#include <driver/i2c_master.h>

// 包含 ESP LCD 面板 IO 头文件
#include <esp_lcd_panel_io.h>
// 包含 ESP LCD 面板操作头文件
#include <esp_lcd_panel_ops.h>
// 包含 ESP LCD GC9A01 驱动头文件
#include <esp_lcd_gc9a01.h>

// 包含 GPIO 驱动头文件
#include "driver/gpio.h"
// 包含 SPI 主机驱动头文件
#include "driver/spi_master.h"

// 包含 ESP BluFi API 头文件
#include "esp_blufi_api.h"
// 包含 ESP MAC 地址头文件
#include "esp_mac.h"
// 包含 ESP BluFi 模块头文件
#include "esp_blufi.h"

// 定义日志标签
#define TAG "MovecallMojiESP32S3BLUFI"

// LVGL 字体声明
// 声明普惠体20号字体的LVGL字体对象
LV_FONT_DECLARE(font_puhui_20_4);
// 声明Font Awesome 20号字体的LVGL字体对象
LV_FONT_DECLARE(font_awesome_20_4);

// 定义辅助方法使用的常量
// 定义Wi-Fi连接最大重试次数
#define EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY 5
// 定义无效RSSI值
#define EXAMPLE_INVALID_RSSI -128
// 定义无效原因码
#define EXAMPLE_INVALID_REASON 255
// const int CONNECTED_BIT = BIT0; // 如果使用事件组位

// 根据配置定义 Wi-Fi 扫描认证模式阈值
#if CONFIG_ESP_WIFI_AUTH_OPEN
// 如果配置为开放认证模式，则定义阈值为开放
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
// 如果配置为WEP认证模式，则定义阈值为WEP
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
// 如果配置为WPA PSK认证模式，则定义阈值为WPA PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
// 如果配置为WPA2 PSK认证模式，则定义阈值为WPA2 PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
// 如果配置为WPA/WPA2 PSK认证模式，则定义阈值为WPA/WPA2 PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
// 如果配置为WPA3 PSK认证模式，则定义阈值为WPA3 PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
// 如果配置为WPA2/WPA3 PSK认证模式，则定义阈值为WPA2/WPA3 PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
// 如果配置为WAPI PSK认证模式，则定义阈值为WAPI PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/**
 * @brief 自定义 LCD 显示类，继承自 SpiLcdDisplay。
 *        主要用于处理圆形屏幕的布局调整，例如状态栏的内边距。
 */
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    /**
     * @brief 构造函数，初始化自定义 LCD 显示。
     * @param io_handle 屏幕 IO 句柄。
     * @param panel_handle 屏幕面板句柄。
     * @param width 屏幕宽度。
     * @param height 屏幕高度。
     * @param offset_x X 轴偏移量。
     * @param offset_y Y 轴偏移量。
     * @param mirror_x 是否沿 X 轴镜像。
     * @param mirror_y 是否沿 Y 轴镜像。
     * @param swap_xy 是否交换 X 和 Y 轴。
     */
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle, 
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy) 
        : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    {
                        .text_font = &font_puhui_20_4, // 设置文本字体
                        .icon_font = &font_awesome_20_4, // 设置图标字体
                        .emoji_font = font_emoji_64_init(), // 初始化并设置 emoji 字体
                    }) {

        DisplayLockGuard lock(this); // 锁定显示以进行安全操作
        // 由于屏幕是圆的，所以状态栏需要增加左右内边距
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.33, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.33, 0);
    }
};

/**
 * @brief Movecall Moji ESP32S3 BluFi 板级支持包类，继承自 WifiBoard。
 *        负责处理 BluFi Wi-Fi 配置、音频、显示、按钮和物联网初始化。
 */
class MovecallMojiESP32S3BLUFI : public WifiBoard {
private:
    // BluFi 相关成员
    bool ble_is_connected_; // 蓝牙是否已连接
    uint8_t blufi_sta_bssid_[6]; // BluFi 站点的 BSSID
    uint8_t blufi_sta_ssid_[32]; // BluFi 站点的 SSID
    int blufi_sta_ssid_len_; // BluFi 站点的 SSID 长度
    bool blufi_sta_got_ip_; // BluFi 站点是否已获取 IP
    bool blufi_sta_is_connecting_; // BluFi 站点是否正在连接
    esp_blufi_extra_info_t blufi_sta_conn_info_; // BluFi 站点的连接信息
    uint8_t example_wifi_retry_count; // Wi-Fi 连接重试次数

    static wifi_config_t sta_config_; // 静态 Wi-Fi 配置
    static MovecallMojiESP32S3BLUFI* s_blufi_instance; // 用于静态 BluFi 回调的实例

    /**
     * @brief BluFi 事件回调函数。
     * @param event BluFi 事件类型。
     * @param param BluFi 事件参数。
     */
    void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);
    /**
     * @brief Wi-Fi 事件处理函数。
     * @param event_base 事件基础。
     * @param event_id 事件 ID。
     * @param event_data 事件数据。
     */
    void wifi_event_handler(esp_event_base_t event_base, int32_t event_id, void* event_data);
    /**
     * @brief IP 事件处理函数。
     * @param event_id 事件 ID。
     * @param event_data 事件数据。
     */
    void ip_event_handler(int32_t event_id, void* event_data);
    /**
     * @brief 记录 Wi-Fi 连接信息。
     * @param rssi RSSI 值。
     * @param reason 断开连接原因。
     */
    void blufi_record_wifi_conn_info(int rssi, uint8_t reason);
    /**
     * @brief BluFi Wi-Fi 连接。
     */
    void blufi_wifi_connect();
    /**
     * @brief BluFi Wi-Fi 重新连接。
     * @return 如果成功重新连接返回 true，否则返回 false。
     */
    bool blufi_wifi_reconnect();

    // 静态回调函数
    /**
     * @brief 静态 BluFi 事件回调函数。
     * @param event BluFi 事件类型。
     * @param param BluFi 事件参数。
     */
    static void blufi_event_callback_static(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);
    /**
     * @brief 静态 Wi-Fi 事件处理函数。
     * @param arg 用户定义数据。
     * @param event_base 事件基础。
     * @param event_id 事件 ID。
     * @param event_data 事件数据。
     */
    static void wifi_event_handler_static(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    /**
     * @brief 静态 IP 事件处理函数。
     * @param arg 用户定义数据。
     * @param event_base 事件基础。
     * @param event_id 事件 ID。
     * @param event_data 事件数据。
     */
    static void ip_event_handler_static(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    i2c_master_bus_handle_t codec_i2c_bus_; // 音频编解码器 I2C 总线句柄
    Button boot_button_; // 启动按钮实例
    Display* display_; // 显示器实例

    /**
     * @brief 初始化编解码器 I2C 总线。
     */
    void InitializeCodecI2c() {
        // 初始化 I2C 外设
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0, // I2C 端口 0
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN, // SDA 引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN, // SCL 引脚
            .clk_source = I2C_CLK_SRC_DEFAULT, // 时钟源
            .glitch_ignore_cnt = 7, // 毛刺过滤计数
            .intr_priority = 0, // 中断优先级
            .trans_queue_depth = 0, // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1, // 启用内部上拉电阻
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_)); // 创建新的 I2C 主总线
    }

    /**
     * @brief 初始化 SPI 总线。
     */
    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SPI bus"); // 记录 SPI 总线初始化信息
        spi_bus_config_t buscfg = GC9A01_PANEL_BUS_SPI_CONFIG(DISPLAY_SPI_SCLK_PIN, DISPLAY_SPI_MOSI_PIN, 
                                    DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t)); // 配置 SPI 总线
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO)); // 初始化 SPI 总线
    }

    /**
     * @brief 初始化 GC9A01 显示面板。
     */
    void InitializeGc9a01Display() {
        ESP_LOGI(TAG, "Init GC9A01 display"); // 记录 GC9A01 显示初始化信息

        ESP_LOGI(TAG, "Install panel IO"); // 记录安装面板 IO 信息
        esp_lcd_panel_io_handle_t io_handle = NULL; // LCD 面板 IO 句柄
        esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(DISPLAY_SPI_CS_PIN, DISPLAY_SPI_DC_PIN, NULL, NULL); // 配置面板 IO SPI
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ; // 设置像素时钟频率
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle)); // 创建新的 LCD 面板 IO SPI
    
        ESP_LOGI(TAG, "Install GC9A01 panel driver"); // 记录安装 GC9A01 面板驱动信息
        esp_lcd_panel_handle_t panel_handle = NULL; // LCD 面板句柄
        esp_lcd_panel_dev_config_t panel_config = {}; // 面板设备配置
        panel_config.reset_gpio_num = DISPLAY_SPI_RESET_PIN;    // 设置复位 GPIO 引脚，如果未使用则设置为 -1
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;           // RGB 字节序
        panel_config.bits_per_pixel = 16;                       // 每像素位数

        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle)); // 创建新的 GC9A01 面板
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle)); // 复位面板
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle)); // 初始化面板
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true)); // 反转颜色
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false)); // 镜像面板
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true)); // 打开/关闭显示

        // 创建 CustomLcdDisplay 实例
        display_ = new CustomLcdDisplay(io_handle, panel_handle, // 使用 CustomLcdDisplay
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    /**
     * @brief 初始化按钮。
     */
    void InitializeButtons() {
        // 注册启动按钮点击事件回调
        boot_button_.OnClick([this]() {
            ESP_LOGI(TAG, "Boot button pressed, requesting Wi-Fi configuration reset (BluFi)."); // 记录按钮按下信息
            this->ResetWifiConfiguration(); // 重置 Wi-Fi 配置
        });
    }

    /**
     * @brief 物联网初始化，添加对 AI 可见设备。
     */
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance(); // 获取 ThingManager 实例
        thing_manager.AddThing(iot::CreateThing("Speaker")); // 添加"Speaker"设备
        thing_manager.AddThing(iot::CreateThing("Screen"));   // 添加"Screen"设备
    }

public:
    /**
     * @brief 进入 Wi-Fi 配置模式（BluFi）。
     */
    virtual void EnterWifiConfigMode() override;

    /**
     * @brief 构造函数，初始化 MovecallMojiESP32S3BLUFI 类。
     */
    MovecallMojiESP32S3BLUFI() : boot_button_(BOOT_BUTTON_GPIO) {  
        InitializeCodecI2c(); // 初始化编解码器 I2C
        InitializeSpi(); // 初始化 SPI
        InitializeGc9a01Display(); // 初始化 GC9A01 显示
        InitializeButtons(); // 初始化按钮
        InitializeIot(); // 初始化物联网
        // BluFi 初始化现在推迟到 EnterWifiConfigMode
    }

    /**
     * @brief 获取 LED 实例。
     * @return LED 实例的指针。
     */
    virtual Led* GetLed() override {
        static SingleLed led_strip(BUILTIN_LED_GPIO); // 创建单个 LED 实例
        return &led_strip; // 返回 LED 实例
    }

    /**
     * @brief 获取显示器实例。
     * @return 显示器实例的指针。
     */
    virtual Display* GetDisplay() override {
        return display_; // 返回显示器实例
    }
    
    /**
     * @brief 获取背光实例。
     * @return 背光实例的指针。
     */
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT); // 创建 PWM 背光实例
        return &backlight; // 返回背光实例
    }

    /**
     * @brief 获取音频编解码器实例。
     * @return 音频编解码器实例的指针。
     */
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR); // 创建 ES8311 音频编解码器实例
        return &audio_codec; // 返回音频编解码器实例
    }
};

/**
 * @brief 进入 Wi-Fi 配置模式（BluFi）。
 *        此方法负责初始化 BluFi 相关的事件组、网络接口、事件处理程序和 Wi-Fi 栈。
 *        它还会检查是否存在存储的 Wi-Fi 配置并尝试连接。
 */
void MovecallMojiESP32S3BLUFI::EnterWifiConfigMode() {
    ESP_LOGI(TAG, "Entering BluFi Wi-Fi Configuration Mode..."); // 记录进入 BluFi 配置模式信息
    Application::GetInstance().SetDeviceState(kDeviceStateWifiConfiguring); // 设置设备状态为 Wi-Fi 配置中

    s_blufi_instance = this; // 设置静态实例指针
    ble_is_connected_ = false; // 蓝牙未连接
    blufi_sta_got_ip_ = false; // 未获取 IP
    blufi_sta_is_connecting_ = false; // 未连接
    example_wifi_retry_count = 0; // 重置重试计数
    memset(blufi_sta_bssid_, 0, sizeof(blufi_sta_bssid_)); // 清空 BSSID
    memset(blufi_sta_ssid_, 0, sizeof(blufi_sta_ssid_)); // 清空 SSID
    blufi_sta_ssid_len_ = 0; // SSID 长度为 0
    memset(&blufi_sta_conn_info_, 0, sizeof(esp_blufi_extra_info_t)); // 清空连接信息

    esp_err_t ret;
    // ESP_LOGI(TAG, "Initializing BluFi"); // 已经记录了 "Entering BluFi..."

    // 初始化 NVS Flash
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); // 擦除 Flash
        ret = nvs_flash_init(); // 重新初始化
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init()); // 初始化网络接口
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta(); // 创建默认 Wi-Fi 站点网络接口
    assert(sta_netif); // 断言网络接口不为空

    // 注册 Wi-Fi 事件处理程序
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &MovecallMojiESP32S3BLUFI::wifi_event_handler_static, this));
    // 注册 IP 事件处理程序
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &MovecallMojiESP32S3BLUFI::ip_event_handler_static, this));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // 获取默认 Wi-Fi 初始化配置
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); // 初始化 Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // 设置 Wi-Fi 模式为站点模式

    // 检查是否有存储的 Wi-Fi 配置
    wifi_config_t saved_wifi_config;
    if (esp_wifi_get_config(WIFI_IF_STA, &saved_wifi_config) == ESP_OK) {
        // 检查 SSID 是否不为空
        if (strlen(reinterpret_cast<const char*>(saved_wifi_config.sta.ssid)) > 0) {
            ESP_LOGI(TAG, "Found stored Wi-Fi configuration for SSID: %s", saved_wifi_config.sta.ssid); // 记录找到存储的 Wi-Fi 配置
            // 基于此的后续操作将由 WIFI_EVENT_STA_START 事件处理
        } else {
            ESP_LOGI(TAG, "Stored Wi-Fi configuration has empty SSID. Will proceed as if no configuration is stored."); // 记录存储的 Wi-Fi 配置 SSID 为空
        }
    } else {
        ESP_LOGE(TAG, "Failed to get stored Wi-Fi configuration. Proceeding as if no configuration is stored."); // 记录获取存储的 Wi-Fi 配置失败
        // 这种情况可能表示 NVS 问题或首次启动。
    }

    static esp_blufi_callbacks_t blufi_callbacks = {
        .event_cb = MovecallMojiESP32S3BLUFI::blufi_event_callback_static, // BluFi 事件回调
        .negotiate_data_handler = blufi_dh_negotiate_data_handler, // 数据协商处理
        .encrypt_func = blufi_aes_encrypt, // 加密函数
        .decrypt_func = blufi_aes_decrypt, // 解密函数
        .checksum_func = blufi_crc_checksum, // 校验和函数
    };

    // 初始化 NimBLE 堆栈，专门用于 BluFi，使用我们的辅助函数
    ret = app_blufi_nimble_stack_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "app_blufi_nimble_stack_init failed: %s", esp_err_to_name(ret)); // 记录 BluFi NimBLE 堆栈初始化失败
        // 考虑如何处理此错误 - 板可能无法用于 Wi-Fi
        // 可能会将设备状态设置为错误或恢复？
        return;
    }

    // 现在向已初始化的堆栈注册 BluFi 回调
    ret = esp_blufi_register_callbacks(&blufi_callbacks);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_blufi_register_callbacks failed: %s", esp_err_to_name(ret)); // 记录 BluFi 回调注册失败
        // app_blufi_nimble_stack_deinit(); // 如果可能，尝试清理
        return;
    }
    ESP_LOGI(TAG, "BluFi callbacks registered."); // 记录 BluFi 回调已注册

    blufi_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, EXAMPLE_INVALID_REASON); // 记录 Wi-Fi 连接信息
    ESP_ERROR_CHECK(esp_wifi_start()); // 这对于触发 WIFI_EVENT_STA_START 至关重要

    ESP_LOGI(TAG, "BLUFI Initialized and Wi-Fi Started, VERSION %04x", esp_blufi_get_version()); // 记录 BluFi 初始化和 Wi-Fi 启动信息
    GetBacklight()->RestoreBrightness(); // 恢复背光亮度
}

/**
 * @brief 静态 BluFi 事件回调函数，将调用实例的非静态方法。
 * @param event BluFi 事件类型。
 * @param param BluFi 事件参数。
 */
void MovecallMojiESP32S3BLUFI::blufi_event_callback_static(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param) {
    if (s_blufi_instance) {
        s_blufi_instance->blufi_event_callback(event, param); // 调用实例的非静态方法
    } else {
        ESP_LOGE(TAG, "s_blufi_instance is null in blufi_event_callback_static"); // 记录实例为空错误
    }
}

/**
 * @brief BluFi 事件回调函数，处理各种 BluFi 事件。
 * @param event BluFi 事件类型。
 * @param param BluFi 事件参数。
 */
void MovecallMojiESP32S3BLUFI::blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param) {
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        ESP_LOGI(TAG, "ESP_BLUFI_EVENT_INIT_FINISH: BluFi initialization complete."); // 记录初始化完成信息

        // 在开始广播之前检查当前 Wi-Fi 连接状态。
        if (blufi_sta_got_ip_) {
            ESP_LOGI(TAG, "Wi-Fi already connected (IP obtained). BluFi advertising will not start automatically."); // 记录 Wi-Fi 已连接信息
            // 可选：如果存在已连接的 BLE 客户端，则发送状态更新，
            // 但通常在 INIT_FINISH 时，没有客户端连接以进行配置。
        } else if (blufi_sta_is_connecting_) {
            ESP_LOGI(TAG, "Wi-Fi is currently attempting to connect (e.g., to a stored network). BluFi advertising will not start automatically."); // 记录 Wi-Fi 正在连接信息
            // 如果此连接尝试失败，将触发 WIFI_EVENT_STA_DISCONNECTED。
            // 我们可能需要在此处添加逻辑，以在适当的情况下启动 BluFi 广播。
        }
        // 此处移除了对 esp_blufi_adv_start() 的显式调用。
        // BluFi 堆栈应在初始化后根据需要自动处理启动广播。
        break;
    case ESP_BLUFI_EVENT_DEINIT_FINISH:
        ESP_LOGI(TAG, "BLUFI deinit finish"); // 记录去初始化完成信息
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
        ESP_LOGI(TAG, "BLUFI ble connect"); // 记录 BLE 连接信息
        ble_is_connected_ = true; // 设置蓝牙已连接标志
        // blufi_security_init(); // 确保此函数存在或已实现
        esp_blufi_adv_stop(); // 停止 BluFi 广播
        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        ESP_LOGI(TAG, "BLUFI ble disconnect"); // 记录 BLE 断开连接信息
        ble_is_connected_ = false; // 设置蓝牙未连接标志
        // blufi_security_deinit(); // 确保此函数存在或已实现
        esp_blufi_adv_start(); // 启动 BluFi 广播
        break;
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        ESP_LOGI(TAG, "BLUFI Set WIFI opmode %d", param->wifi_mode.op_mode); // 记录设置 Wi-Fi 操作模式信息
        ESP_ERROR_CHECK(esp_wifi_set_mode(param->wifi_mode.op_mode)); // 设置 Wi-Fi 模式
        break;
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        ESP_LOGI(TAG, "BLUFI requset wifi connect from mobile"); // 记录请求连接 Wi-Fi 信息
        esp_wifi_disconnect(); // 断开 Wi-Fi 连接
        blufi_wifi_connect(); // 连接 Wi-Fi
        break;
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        ESP_LOGI(TAG, "BLUFI requset wifi disconnect from mobile"); // 记录请求断开 Wi-Fi 连接信息
        esp_wifi_disconnect(); // 断开 Wi-Fi 连接
        break;
    case ESP_BLUFI_EVENT_REPORT_ERROR:
        ESP_LOGE(TAG, "BLUFI report error, error code %d", param->report_error.state); // 记录 BluFi 错误信息
        esp_blufi_send_error_info(param->report_error.state); // 发送错误信息
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
        wifi_mode_t mode;
        esp_blufi_extra_info_t info;

        esp_wifi_get_mode(&mode); // 获取 Wi-Fi 模式
        memset(&info, 0, sizeof(esp_blufi_extra_info_t)); // 清空额外信息

        if (blufi_sta_got_ip_) {
            memcpy(info.sta_bssid, blufi_sta_bssid_, 6); // 拷贝 BSSID
            info.sta_bssid_set = true; // 设置 BSSID 已设置标志
            info.sta_ssid = blufi_sta_ssid_; // 设置 SSID
            info.sta_ssid_len = blufi_sta_ssid_len_; // 设置 SSID 长度
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info); // 发送 Wi-Fi 连接成功报告
        } else if (blufi_sta_is_connecting_) {
             esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, 0, &blufi_sta_conn_info_); // 发送 Wi-Fi 连接中报告
        }
        else {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, 0, &blufi_sta_conn_info_); // 发送 Wi-Fi 连接失败报告
        }
        ESP_LOGI(TAG, "BLUFI get wifi status from mobile"); // 记录获取 Wi-Fi 状态信息
        break;
    }
    case ESP_BLUFI_EVENT_RECV_STA_SSID:
        memcpy(sta_config_.sta.ssid, param->sta_ssid.ssid, param->sta_ssid.ssid_len); // 拷贝 SSID
        sta_config_.sta.ssid[param->sta_ssid.ssid_len] = '\0'; // 添加字符串结束符
        ESP_LOGI(TAG, "BLUFI recv STA SSID %s", sta_config_.sta.ssid); // 记录接收到的 SSID
        break;
    case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        memcpy(sta_config_.sta.password, param->sta_passwd.passwd, param->sta_passwd.passwd_len); // 拷贝密码
        sta_config_.sta.password[param->sta_passwd.passwd_len] = '\0'; // 添加字符串结束符
        // 设置通用认证模式 - 如果需要其他模式，请调整
        sta_config_.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        ESP_LOGI(TAG, "BLUFI recv STA PASSWORD %s", sta_config_.sta.password); // 记录接收到的密码
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config_)); // 设置 Wi-Fi 配置
        break;
    // 根据 blufi_example.c 添加其他情况
    default:
        ESP_LOGW(TAG, "Unknown BLUFI event: %d", event); // 记录未知 BluFi 事件
        break;
    }
}

/**
 * @brief Wi-Fi 事件处理函数，处理各种 Wi-Fi 事件。
 * @param event_base 事件基础。
 * @param event_id 事件 ID。
 * @param event_data 事件数据。
 */
void MovecallMojiESP32S3BLUFI::wifi_event_handler(esp_event_base_t event_base, int32_t event_id, void* event_data) {
    wifi_event_sta_connected_t *event_conn;
    wifi_event_sta_disconnected_t *event_disc;

    switch (event_id) {
    case WIFI_EVENT_STA_START: {
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START received."); // 记录接收到 STA START 事件
        // 尝试获取当前配置的 Wi-Fi 凭据
        wifi_config_t current_wifi_config;
        esp_err_t get_config_ret = esp_wifi_get_config(WIFI_IF_STA, &current_wifi_config);

        if (get_config_ret == ESP_OK && strlen(reinterpret_cast<const char*>(current_wifi_config.sta.ssid)) > 0) {
            ESP_LOGI(TAG, "STA_START: Stored configuration found for SSID: %s. Attempting to connect.", current_wifi_config.sta.ssid); // 记录找到存储的配置并尝试连接

            // 重要的是 sta_config_（BluFi 使用的类成员）要一致
            // 或者 esp_wifi_connect() 使用 Wi-Fi 驱动程序中已设置的配置。
            // ESP-IDF 的 esp_wifi_connect() 使用先前由 esp_wifi_set_config() 设置的配置。
            // 因此，如果 NVS 有配置，它应该通过 Wi-Fi 初始化过程加载到驱动程序中。

            // 设置我们的内部状态标志
            blufi_sta_is_connecting_ = true; // 设置正在连接标志
            example_wifi_retry_count = 0; // 重置重试计数
            blufi_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, EXAMPLE_INVALID_REASON); // 记录连接尝试开始

            esp_err_t connect_ret = esp_wifi_connect(); // 连接 Wi-Fi
            if (connect_ret == ESP_OK) {
                ESP_LOGI(TAG, "STA_START: esp_wifi_connect() initiated for stored configuration."); // 记录连接已启动
            } else {
                ESP_LOGE(TAG, "STA_START: esp_wifi_connect() failed for stored configuration with error: %s", esp_err_to_name(connect_ret)); // 记录连接失败
                blufi_sta_is_connecting_ = false; // 连接甚至没有开始
                // 可选：向 BluFi 报告此失败（如果已连接），尽管在 STA_START 时不太可能。
            }
        } else {
            ESP_LOGI(TAG, "STA_START: No valid stored Wi-Fi configuration found, or SSID is empty. Waiting for BluFi provisioning."); // 记录未找到有效配置，等待 BluFi 配置
            // 此处不执行任何操作；如果需要，将根据 blufi_event_callback 逻辑启动 BluFi 进程。
            // 确保如果我们不尝试连接，则 blufi_sta_is_connecting_ 为 false。
            blufi_sta_is_connecting_ = false;
        }
        break;
    }
    case WIFI_EVENT_STA_CONNECTED:
        event_conn = (wifi_event_sta_connected_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi STA Connected to %s (BSSID: " MACSTR ")", event_conn->ssid, MAC2STR(event_conn->bssid)); // 记录 Wi-Fi 连接成功信息
        memcpy(blufi_sta_bssid_, event_conn->bssid, 6); // 拷贝 BSSID
        memcpy(blufi_sta_ssid_, event_conn->ssid, event_conn->ssid_len); // 拷贝 SSID
        blufi_sta_ssid_len_ = event_conn->ssid_len; // 设置 SSID 长度
        blufi_sta_is_connecting_ = false; // 连接成功
        // xEventGroupSetBits(wifi_event_group_, CONNECTED_BIT); // 如果使用事件组获取 IP
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        event_disc = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi STA Disconnected, reason: %d", event_disc->reason); // 记录 Wi-Fi 断开连接信息
        blufi_sta_got_ip_ = false; // IP 未获取
        // xEventGroupClearBits(wifi_event_group_, CONNECTED_BIT); // 清除连接位

        if (ble_is_connected_) {
            esp_blufi_send_wifi_conn_report(WIFI_MODE_STA, ESP_BLUFI_STA_CONN_FAIL, event_disc->reason, &blufi_sta_conn_info_); // 发送 Wi-Fi 连接失败报告
        }

        if (blufi_sta_is_connecting_) { // 仅当我们正在连接时才尝试重新连接
            blufi_wifi_reconnect(); // 重新连接 Wi-Fi
        }
        // 此处移除了一个条件块，该块之前调用 esp_blufi_adv_start()。
        // BluFi 堆栈应根据 BLE 连接状态以及是否已成功配置来管理广播状态。
        // 在 STA_DISCONNECTED（特别是当自动连接失败时）手动重新启动广播通常是不需要的，
        // 因为配置广播的主要触发器通常是初始状态或显式用户操作（如启动按钮）。
        break;
    default:
        ESP_LOGI(TAG, "Unhandled Wi-Fi event: %ld", event_id); // 记录未处理的 Wi-Fi 事件
        break;
    }
}

/**
 * @brief IP 事件处理函数，处理 IP 地址获取事件。
 * @param event_id 事件 ID。
 * @param event_data 事件数据。
 */
void MovecallMojiESP32S3BLUFI::ip_event_handler(int32_t event_id, void* event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Wi-Fi STA Got IP: " IPSTR, IP2STR(&event->ip_info.ip)); // 记录获取到 IP 地址
        blufi_sta_got_ip_ = true; // 设置获取到 IP 标志
        blufi_sta_is_connecting_ = false; // IP 已获取，连接过程完成
        example_wifi_retry_count = 0; // 成功连接后重置重试计数

        if (ble_is_connected_) {
            esp_blufi_send_wifi_conn_report(WIFI_MODE_STA, ESP_BLUFI_STA_CONN_SUCCESS, 0, NULL); // 发送 Wi-Fi 连接成功报告
        }
        // xEventGroupSetBits(wifi_event_group_, CONNECTED_BIT); // 发出成功获取 IP 的信号
    }
}

/**
 * @brief 记录 Wi-Fi 连接信息，包括 RSSI 和断开连接原因。
 * @param rssi RSSI 值。
 * @param reason 断开连接原因。
 */
void MovecallMojiESP32S3BLUFI::blufi_record_wifi_conn_info(int rssi, uint8_t reason) {
    memset(&blufi_sta_conn_info_, 0, sizeof(esp_blufi_extra_info_t)); // 清空连接信息
    if (rssi != EXAMPLE_INVALID_RSSI) {
        blufi_sta_conn_info_.sta_max_conn_retry_set = true; // 设置最大连接重试次数已设置标志
        blufi_sta_conn_info_.sta_max_conn_retry = EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY; // 示例值
    }
    if (reason != EXAMPLE_INVALID_REASON) {
         blufi_sta_conn_info_.sta_conn_end_reason_set = true; // 设置连接结束原因已设置标志
         blufi_sta_conn_info_.sta_conn_end_reason = reason; // 设置连接结束原因
    }
}

/**
 * @brief 尝试连接到 AP。
 */
void MovecallMojiESP32S3BLUFI::blufi_wifi_connect() {
    ESP_LOGI(TAG, "Attempting to connect to AP with SSID: %s", sta_config_.sta.ssid); // 记录尝试连接 AP 信息
    blufi_sta_is_connecting_ = true; // 设置正在连接标志
    blufi_sta_got_ip_ = false; // 重置 IP 状态
    example_wifi_retry_count = 0; // 重置重试计数
    blufi_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, EXAMPLE_INVALID_REASON); // 清除之前的错误信息
    ESP_ERROR_CHECK(esp_wifi_connect()); // 连接 Wi-Fi
}

/**
 * @brief 尝试重新连接 Wi-Fi。
 * @return 如果成功重新连接返回 true，否则返回 false。
 */
bool MovecallMojiESP32S3BLUFI::blufi_wifi_reconnect() {
    if (example_wifi_retry_count < EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY) {
        example_wifi_retry_count++; // 增加重试计数
        ESP_LOGI(TAG, "Retrying Wi-Fi connection (%d/%d)...", example_wifi_retry_count, EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY); // 记录重试信息
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延迟后重试
        esp_wifi_connect(); // 连接 Wi-Fi
        return true; // 返回 true 表示正在重试
    } else {
        ESP_LOGE(TAG, "Failed to connect to AP after %d retries.", EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY); // 记录连接失败信息
        blufi_sta_is_connecting_ = false; // 停止尝试连接
        // 如果已连接 BluFi，则报告持续失败
        if (ble_is_connected_) {
             blufi_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, WIFI_REASON_NO_AP_FOUND); // 示例原因
             esp_blufi_send_wifi_conn_report(WIFI_MODE_STA, ESP_BLUFI_STA_CONN_FAIL, 0, &blufi_sta_conn_info_); // 发送 Wi-Fi 连接失败报告
        }
        return false; // 返回 false 表示重试失败
    }
}

/**
 * @brief 静态 Wi-Fi 事件处理函数，将调用实例的非静态方法。
 * @param arg 用户定义数据。
 * @param event_base 事件基础。
 * @param event_id 事件 ID。
 * @param event_data 事件数据。
 */
void MovecallMojiESP32S3BLUFI::wifi_event_handler_static(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    MovecallMojiESP32S3BLUFI* instance = static_cast<MovecallMojiESP32S3BLUFI*>(arg);
    if (instance) {
        instance->wifi_event_handler(event_base, event_id, event_data); // 调用实例的非静态方法
    }
}

/**
 * @brief 静态 IP 事件处理函数，将调用实例的非静态方法。
 * @param arg 用户定义数据。
 * @param event_base 事件基础。
 * @param event_id 事件 ID。
 * @param event_data 事件数据。
 */
void MovecallMojiESP32S3BLUFI::ip_event_handler_static(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    MovecallMojiESP32S3BLUFI* instance = static_cast<MovecallMojiESP32S3BLUFI*>(arg);
    if (instance) {
        instance->ip_event_handler(event_id, event_data); // 调用实例的非静态方法
    }
}

wifi_config_t MovecallMojiESP32S3BLUFI::sta_config_; // 静态 Wi-Fi 配置实例
MovecallMojiESP32S3BLUFI* MovecallMojiESP32S3BLUFI::s_blufi_instance = nullptr; // 静态 BluFi 实例指针
DECLARE_BOARD(MovecallMojiESP32S3BLUFI); // 声明板

