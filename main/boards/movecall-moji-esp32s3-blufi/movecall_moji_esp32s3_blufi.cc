#include "board.h" // Assuming 'Board' base class is defined here
#include "esp_blufi_api.h"
#include "esp_bt.h"
#include "blufi_example.h" // To be created from example
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h" // Ensure ESP_LOGI, ESP_LOGE are available
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"

#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_gc9a01.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"

#define TAG "MovecallMojiESP32S3BLUFI"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

// Define constants used by helper methods if not globally available
#define EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY 5
#define EXAMPLE_INVALID_RSSI -128
#define EXAMPLE_INVALID_REASON 255
// const int CONNECTED_BIT = BIT0; // If using event group bits

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

class CustomLcdDisplay : public SpiLcdDisplay {
public:
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
                        .text_font = &font_puhui_20_4,
                        .icon_font = &font_awesome_20_4,
                        .emoji_font = font_emoji_64_init(),
                    }) {

        DisplayLockGuard lock(this);
        // 由于屏幕是圆的，所以状态栏需要增加左右内边距
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.33, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.33, 0);
    }
};

class MovecallMojiESP32S3BLUFI : public Board {
private:
    // BluFi related members
    EventGroupHandle_t wifi_event_group_; // For signaling Wi-Fi events if needed by other logic
    bool ble_is_connected_;
    uint8_t blufi_sta_bssid_[6];
    uint8_t blufi_sta_ssid_[32];
    int blufi_sta_ssid_len_;
    bool blufi_sta_got_ip_;
    bool blufi_sta_is_connecting_;
    esp_blufi_extra_info_t blufi_sta_conn_info_;
    uint8_t example_wifi_retry_count;

    static wifi_config_t sta_config_;
    static MovecallMojiESP32S3BLUFI* s_blufi_instance; // For static BluFi callback

    // Methods
    void InitializeBluFi();
    void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);
    void wifi_event_handler(esp_event_base_t event_base, int32_t event_id, void* event_data);
    void ip_event_handler(int32_t event_id, void* event_data);
    void blufi_record_wifi_conn_info(int rssi, uint8_t reason);
    void blufi_wifi_connect();
    bool blufi_wifi_reconnect();

    // Static Callbacks
    static void blufi_event_callback_static(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);
    static void wifi_event_handler_static(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void ip_event_handler_static(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    Display* display_;

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    // SPI初始化
    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SPI bus");
        spi_bus_config_t buscfg = GC9A01_PANEL_BUS_SPI_CONFIG(DISPLAY_SPI_SCLK_PIN, DISPLAY_SPI_MOSI_PIN, 
                                    DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    // GC9A01初始化
    void InitializeGc9a01Display() {
        ESP_LOGI(TAG, "Init GC9A01 display");

        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(DISPLAY_SPI_CS_PIN, DISPLAY_SPI_DC_PIN, NULL, NULL);
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle));
    
        ESP_LOGI(TAG, "Install GC9A01 panel driver");
        esp_lcd_panel_handle_t panel_handle = NULL;
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_RESET_PIN;    // Set to -1 if not use
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;           //LCD_RGB_ENDIAN_RGB;
        panel_config.bits_per_pixel = 16;                       // Implemented by LCD command `3Ah` (16/18)

        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true)); 

        display_ = new SpiLcdDisplay(io_handle, panel_handle,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4,
                                        .icon_font = &font_awesome_20_4,
                                        .emoji_font = font_emoji_64_init(),
                                    });
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            Application::GetInstance().ToggleChatState();
            // Removed Wi-Fi reset logic, BluFi handles provisioning.
            // If manual re-provisioning is needed, it would be more involved.
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker")); 
        thing_manager.AddThing(iot::CreateThing("Screen"));   
    }

public:
    MovecallMojiESP32S3BLUFI() : boot_button_(BOOT_BUTTON_GPIO) {  
        InitializeCodecI2c();
        InitializeSpi();
        InitializeGc9a01Display();
        InitializeButtons();
        InitializeIot();

        s_blufi_instance = this; // Set static instance pointer
        ble_is_connected_ = false;
        blufi_sta_got_ip_ = false;
        blufi_sta_is_connecting_ = false;
        example_wifi_retry_count = 0;
        memset(blufi_sta_bssid_, 0, sizeof(blufi_sta_bssid_));
        memset(blufi_sta_ssid_, 0, sizeof(blufi_sta_ssid_));
        blufi_sta_ssid_len_ = 0;
        memset(&blufi_sta_conn_info_, 0, sizeof(esp_blufi_extra_info_t));

        InitializeBluFi();
        GetBacklight()->RestoreBrightness();
    }

    virtual Led* GetLed() override {
        static SingleLed led_strip(BUILTIN_LED_GPIO);
        return &led_strip;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }
};


void MovecallMojiESP32S3BLUFI::InitializeBluFi() {
    esp_err_t ret;
    ESP_LOGI(TAG, "Initializing BluFi");

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group_ = xEventGroupCreate(); // Initialize event group
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &MovecallMojiESP32S3BLUFI::wifi_event_handler_static, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &MovecallMojiESP32S3BLUFI::ip_event_handler_static, this));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Check for stored Wi-Fi configuration
    wifi_config_t saved_wifi_config;
    if (esp_wifi_get_config(WIFI_IF_STA, &saved_wifi_config) == ESP_OK) {
        // Check if SSID is not empty
        if (strlen(reinterpret_cast<const char*>(saved_wifi_config.sta.ssid)) > 0) {
            ESP_LOGI(TAG, "Found stored Wi-Fi configuration for SSID: %s", saved_wifi_config.sta.ssid);
            // Further action based on this will be handled by WIFI_EVENT_STA_START event
        } else {
            ESP_LOGI(TAG, "Stored Wi-Fi configuration has empty SSID. Will proceed as if no configuration is stored.");
        }
    } else {
        ESP_LOGE(TAG, "Failed to get stored Wi-Fi configuration. Proceeding as if no configuration is stored.");
        // This case might indicate an NVS issue or first boot.
    }

    static esp_blufi_callbacks_t blufi_callbacks = {
        .event_cb = MovecallMojiESP32S3BLUFI::blufi_event_callback_static,
        .negotiate_data_handler = blufi_dh_negotiate_data_handler,
        .encrypt_func = blufi_aes_encrypt,
        .decrypt_func = blufi_aes_decrypt,
        .checksum_func = blufi_crc_checksum,
    };

    // Assuming NimBLE is used (CONFIG_BT_NIMBLE_ENABLED=y in sdkconfig)
    // If not, esp_blufi_controller_init() for classic BT might be needed here.
    // The blufi_init.c typically handles esp_nimble_init() etc. via esp_blufi_host_init().

        // Initialize NimBLE stack specifically for BluFi using our helper
        ret = app_blufi_nimble_stack_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "app_blufi_nimble_stack_init failed: %s", esp_err_to_name(ret));
            // Consider how to handle this error - board might not function for Wi-Fi
            return;
        }

        // Now register BluFi callbacks with the initialized stack
        ret = esp_blufi_register_callbacks(&blufi_callbacks); // Use esp_blufi_register_callbacks
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_blufi_register_callbacks failed: %s", esp_err_to_name(ret));
            // app_blufi_nimble_stack_deinit(); // Attempt cleanup
            return;
        }
        ESP_LOGI(TAG, "BluFi callbacks registered.");

    blufi_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, EXAMPLE_INVALID_REASON);
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "BLUFI Initialized, VERSION %04x", esp_blufi_get_version());
}

void MovecallMojiESP32S3BLUFI::blufi_event_callback_static(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param) {
    if (s_blufi_instance) {
        s_blufi_instance->blufi_event_callback(event, param);
    } else {
        ESP_LOGE(TAG, "s_blufi_instance is null in blufi_event_callback_static");
    }
}

void MovecallMojiESP32S3BLUFI::blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param) {
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        ESP_LOGI(TAG, "ESP_BLUFI_EVENT_INIT_FINISH: BluFi initialization complete.");

        // Check current Wi-Fi connection state before starting advertising.
        if (blufi_sta_got_ip_) {
            ESP_LOGI(TAG, "Wi-Fi already connected (IP obtained). BluFi advertising will not start automatically.");
            // Optionally, could send a status update to a connected BLE client if one exists,
            // but typically at INIT_FINISH, there's no client connected yet for provisioning.
        } else if (blufi_sta_is_connecting_) {
            ESP_LOGI(TAG, "Wi-Fi is currently attempting to connect (e.g., to a stored network). BluFi advertising will not start automatically.");
            // If this connection attempt fails, WIFI_EVENT_STA_DISCONNECTED will be triggered.
            // We might need logic there to then start BluFi advertising if appropriate.
        } else {
            // No current connection and no active attempt to connect to a stored network.
            // This means either no stored config, or auto-connect attempt already failed and concluded.
            ESP_LOGI(TAG, "Wi-Fi not connected and not attempting connection. Starting BluFi advertising for provisioning.");
            esp_blufi_adv_start();
        }
        break;
    case ESP_BLUFI_EVENT_DEINIT_FINISH:
        ESP_LOGI(TAG, "BLUFI deinit finish");
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
        ESP_LOGI(TAG, "BLUFI ble connect");
        ble_is_connected_ = true;
        // blufi_security_init(); // Ensure this function exists or is implemented
        esp_blufi_adv_stop();
        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        ESP_LOGI(TAG, "BLUFI ble disconnect");
        ble_is_connected_ = false;
        // blufi_security_deinit(); // Ensure this function exists or is implemented
        esp_blufi_adv_start(); // Ensure this function exists or is implemented
        break;
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        ESP_LOGI(TAG, "BLUFI Set WIFI opmode %d", param->wifi_mode.op_mode);
        ESP_ERROR_CHECK(esp_wifi_set_mode(param->wifi_mode.op_mode));
        break;
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        ESP_LOGI(TAG, "BLUFI requset wifi connect from mobile");
        esp_wifi_disconnect();
        blufi_wifi_connect();
        break;
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        ESP_LOGI(TAG, "BLUFI requset wifi disconnect from mobile");
        esp_wifi_disconnect();
        break;
    case ESP_BLUFI_EVENT_REPORT_ERROR:
        ESP_LOGE(TAG, "BLUFI report error, error code %d", param->report_error.state);
        esp_blufi_send_error_info(param->report_error.state);
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
        wifi_mode_t mode;
        esp_blufi_extra_info_t info;

        esp_wifi_get_mode(&mode);
        memset(&info, 0, sizeof(esp_blufi_extra_info_t));

        if (blufi_sta_got_ip_) {
            memcpy(info.sta_bssid, blufi_sta_bssid_, 6);
            info.sta_bssid_set = true;
            info.sta_ssid = blufi_sta_ssid_;
            info.sta_ssid_len = blufi_sta_ssid_len_;
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
        } else if (blufi_sta_is_connecting_) {
             esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, 0, &blufi_sta_conn_info_);
        }
        else {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, 0, &blufi_sta_conn_info_);
        }
        ESP_LOGI(TAG, "BLUFI get wifi status from mobile");
        break;
    }
    case ESP_BLUFI_EVENT_RECV_STA_SSID:
        memcpy(sta_config_.sta.ssid, param->sta_ssid.ssid, param->sta_ssid.ssid_len);
        sta_config_.sta.ssid[param->sta_ssid.ssid_len] = '\0';
        ESP_LOGI(TAG, "BLUFI recv STA SSID %s", sta_config_.sta.ssid);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        memcpy(sta_config_.sta.password, param->sta_passwd.passwd, param->sta_passwd.passwd_len);
        sta_config_.sta.password[param->sta_passwd.passwd_len] = '\0';
        // Set a common auth mode - adjust if other modes are needed
        sta_config_.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        ESP_LOGI(TAG, "BLUFI recv STA PASSWORD %s", sta_config_.sta.password);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config_));
        break;
    // Add other cases as needed from blufi_example.c
    default:
        ESP_LOGW(TAG, "Unknown BLUFI event: %d", event);
        break;
    }
}

void MovecallMojiESP32S3BLUFI::wifi_event_handler(esp_event_base_t event_base, int32_t event_id, void* event_data) {
    wifi_event_sta_connected_t *event_conn;
    wifi_event_sta_disconnected_t *event_disc;

    switch (event_id) {
    case WIFI_EVENT_STA_START: {
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START received.");
        // Attempt to get the currently configured Wi-Fi credentials
        wifi_config_t current_wifi_config;
        esp_err_t get_config_ret = esp_wifi_get_config(WIFI_IF_STA, &current_wifi_config);

        if (get_config_ret == ESP_OK && strlen(reinterpret_cast<const char*>(current_wifi_config.sta.ssid)) > 0) {
            ESP_LOGI(TAG, "STA_START: Stored configuration found for SSID: %s. Attempting to connect.", current_wifi_config.sta.ssid);

            // It's important that sta_config_ (the class member used by BluFi) is consistent
            // or that esp_wifi_connect() uses the configuration already set in the Wi-Fi driver.
            // ESP-IDF's esp_wifi_connect() uses the configuration previously set by esp_wifi_set_config().
            // So, if NVS had a config, it should be loaded into the driver by Wi-Fi init process.

            // Set our internal state flag
            blufi_sta_is_connecting_ = true;
            example_wifi_retry_count = 0; // Reset retry count for this new attempt sequence
            blufi_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, EXAMPLE_INVALID_REASON); // Record that connection attempt is starting

            esp_err_t connect_ret = esp_wifi_connect();
            if (connect_ret == ESP_OK) {
                ESP_LOGI(TAG, "STA_START: esp_wifi_connect() initiated for stored configuration.");
            } else {
                ESP_LOGE(TAG, "STA_START: esp_wifi_connect() failed for stored configuration with error: %s", esp_err_to_name(connect_ret));
                blufi_sta_is_connecting_ = false; // Connection didn't even start
                // Optionally, report this failure to BluFi if connected, though unlikely at STA_START.
            }
        } else {
            ESP_LOGI(TAG, "STA_START: No valid stored Wi-Fi configuration found, or SSID is empty. Waiting for BluFi provisioning.");
            // Do nothing here; BluFi process will be initiated if needed based on blufi_event_callback logic.
            // Ensure blufi_sta_is_connecting_ is false if we are not attempting connection.
            blufi_sta_is_connecting_ = false;
        }
        break;
    }
    case WIFI_EVENT_STA_CONNECTED:
        event_conn = (wifi_event_sta_connected_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi STA Connected to %s (BSSID: " MACSTR ")", event_conn->ssid, MAC2STR(event_conn->bssid));
        memcpy(blufi_sta_bssid_, event_conn->bssid, 6);
        memcpy(blufi_sta_ssid_, event_conn->ssid, event_conn->ssid_len);
        blufi_sta_ssid_len_ = event_conn->ssid_len;
        blufi_sta_is_connecting_ = false; // Successfully connected
        // xEventGroupSetBits(wifi_event_group_, CONNECTED_BIT); // If using event group for IP
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        event_disc = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi STA Disconnected, reason: %d", event_disc->reason);
        blufi_sta_got_ip_ = false;
        // xEventGroupClearBits(wifi_event_group_, CONNECTED_BIT); // Clear connected bit

        if (ble_is_connected_) {
            esp_blufi_send_wifi_conn_report(WIFI_MODE_STA, ESP_BLUFI_STA_CONN_FAIL, event_disc->reason, &blufi_sta_conn_info_);
        }

        if (blufi_sta_is_connecting_) { // Only try to reconnect if we were in the process of connecting
            blufi_wifi_reconnect();
        }
            // If all retries for an auto-connection have failed and BluFi is initialized but not advertising
            // gl_sta_connected is the example's flag, use blufi_sta_got_ip_
            if (!blufi_sta_got_ip_ && !blufi_sta_is_connecting_ && !ble_is_connected_) {
                // Check if BluFi system is initialized (s_blufi_instance exists, or a similar flag)
                // This check is to ensure we only try to start adv if BluFi itself is ready.
                // Assuming BluFi is ready if we are in wifi_event_handler of this board.

                // Check if this disconnect was the end of an auto-connect attempt (not a BluFi initiated one)
                // This can be tricky. A simple way: if no BLE client is connected,
                // and we just exhausted retries for a connection that wasn't initiated by a BluFi command.
                // For now, if disconnected, not trying to connect anymore, and no BLE client, start ADV.
                // This might conflict if user disconnects from AP via BluFi command, then BLE disconnects.
                // A more robust flag like `is_auto_connecting_` might be needed.

                // Current logic for retries: blufi_wifi_reconnect() sets blufi_sta_is_connecting_ to false after max retries.
                // So, if blufi_sta_is_connecting_ is false here, and we are not connected, it means connection failed.
                ESP_LOGI(TAG, "STA_DISCONNECTED: Connection attempt failed or lost, and no active BLE client. Considering starting BluFi advertising.");

                // Start BluFi advertising if it's not already started and no BLE client is connected.
                // The ESP_BLUFI_EVENT_INIT_FINISH might have already decided not to start it.
                // This ensures that if auto-connection fails, BluFi becomes available.
                // We need to be careful not to start it if a BLE client *was* connected and caused the disconnect.

                // Simpler: If not connected, and not currently trying to connect via esp_wifi_connect,
                // and BluFi is initialized, let ESP_BLUFI_EVENT_INIT_FINISH decide or start here.
                // The ESP_BLUFI_EVENT_INIT_FINISH check is primary. This is a fallback.
                // If ESP_BLUFI_EVENT_INIT_FINISH occurred when blufi_sta_is_connecting_ was true, it wouldn't have advertised.
                // Now, if blufi_sta_is_connecting_ becomes false due to failure, we should start it.

                // Check if BluFi is initialized (a simple proxy for this is if esp_blufi_get_version() > 0 or similar)
                // For now, assume if these handlers are active, BluFi has been initialized.
                if (!ble_is_connected_) { // Only if no phone app is currently interacting via BLE
                    ESP_LOGI(TAG, "STA_DISCONNECTED: Auto-connection failed or disconnected. Starting BluFi advertising.");
                    esp_blufi_adv_start();
                }
            }
        break;
    default:
        ESP_LOGI(TAG, "Unhandled Wi-Fi event: %ld", event_id);
        break;
    }
}

void MovecallMojiESP32S3BLUFI::ip_event_handler(int32_t event_id, void* event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Wi-Fi STA Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        blufi_sta_got_ip_ = true;
        blufi_sta_is_connecting_ = false; // IP obtained, connection process complete
        example_wifi_retry_count = 0; // Reset retry counter on successful connection

        if (ble_is_connected_) {
            esp_blufi_send_wifi_conn_report(WIFI_MODE_STA, ESP_BLUFI_STA_CONN_SUCCESS, 0, NULL);
        }
        // xEventGroupSetBits(wifi_event_group_, CONNECTED_BIT); // Signal successful IP acquisition
    }
}

void MovecallMojiESP32S3BLUFI::blufi_record_wifi_conn_info(int rssi, uint8_t reason) {
    memset(&blufi_sta_conn_info_, 0, sizeof(esp_blufi_extra_info_t));
    if (rssi != EXAMPLE_INVALID_RSSI) {
        blufi_sta_conn_info_.sta_max_conn_retry_set = true;
        blufi_sta_conn_info_.sta_max_conn_retry = EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY; // Example value
    }
    if (reason != EXAMPLE_INVALID_REASON) {
         blufi_sta_conn_info_.sta_conn_fail_reason_set = true;
         blufi_sta_conn_info_.sta_conn_fail_reason = reason;
    }
}

void MovecallMojiESP32S3BLUFI::blufi_wifi_connect() {
    ESP_LOGI(TAG, "Attempting to connect to AP with SSID: %s", sta_config_.sta.ssid);
    blufi_sta_is_connecting_ = true;
    blufi_sta_got_ip_ = false; // Reset IP status on new connection attempt
    example_wifi_retry_count = 0; // Reset retry count for new explicit connection
    blufi_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, EXAMPLE_INVALID_REASON); // Clear previous error info
    ESP_ERROR_CHECK(esp_wifi_connect());
}

bool MovecallMojiESP32S3BLUFI::blufi_wifi_reconnect() {
    if (example_wifi_retry_count < EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY) {
        example_wifi_retry_count++;
        ESP_LOGI(TAG, "Retrying Wi-Fi connection (%d/%d)...", example_wifi_retry_count, EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay before retrying
        esp_wifi_connect();
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to connect to AP after %d retries.", EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY);
        blufi_sta_is_connecting_ = false; // Stop attempting to connect
        // Report persistent failure to BluFi if connected
        if (ble_is_connected_) {
             blufi_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, ESP_BLUFI_STA_CONN_FAIL_REASON_NO_AP_FOUND); // Example reason
             esp_blufi_send_wifi_conn_report(WIFI_MODE_STA, ESP_BLUFI_STA_CONN_FAIL, 0, &blufi_sta_conn_info_);
        }
        return false;
    }
}

void MovecallMojiESP32S3BLUFI::wifi_event_handler_static(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    MovecallMojiESP32S3BLUFI* instance = static_cast<MovecallMojiESP32S3BLUFI*>(arg);
    if (instance) {
        instance->wifi_event_handler(event_base, event_id, event_data);
    }
}

void MovecallMojiESP32S3BLUFI::ip_event_handler_static(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    MovecallMojiESP32S3BLUFI* instance = static_cast<MovecallMojiESP32S3BLUFI*>(arg);
    if (instance) {
        instance->ip_event_handler(event_id, event_data);
    }
}

wifi_config_t MovecallMojiESP32S3BLUFI::sta_config_;
MovecallMojiESP32S3BLUFI* MovecallMojiESP32S3BLUFI::s_blufi_instance = nullptr;
DECLARE_BOARD(MovecallMojiESP32S3BLUFI);

