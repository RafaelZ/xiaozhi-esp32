// #ifndef WIFI_BOARD_H 和 #define WIFI_BOARD_H 是头文件保护宏，用于防止头文件被重复包含。
#ifndef WIFI_BOARD_H
#define WIFI_BOARD_H

// 包含 "board.h" 头文件，这表明 WifiBoard 类继承自 Board 类。
#include "board.h"

// 定义 WifiBoard 类，它公开继承自 Board 类。
class WifiBoard : public Board {
protected:
    // 声明一个布尔成员变量 wifi_config_mode_，用于指示是否处于 Wi-Fi 配置模式，并初始化为 false。
    bool wifi_config_mode_ = false;
    // 声明一个受保护的成员函数 EnterWifiConfigMode()，用于进入 Wi-Fi 配置模式。
    void EnterWifiConfigMode();
    // 声明一个虚函数 GetBoardJson()，它重写了基类 Board 的同名函数，返回板子的 JSON 字符串。
    virtual std::string GetBoardJson() override;

public:
    // 声明 WifiBoard 类的构造函数。
    WifiBoard();
    // 声明一个虚函数 GetBoardType()，它重写了基类 Board 的同名函数，返回板子的类型字符串。
    virtual std::string GetBoardType() override;
    // 声明一个虚函数 StartNetwork()，它重写了基类 Board 的同名函数，用于启动网络连接。
    virtual void StartNetwork() override;
    // 声明一个虚函数 CreateHttp()，它重写了基类 Board 的同名函数，用于创建 Http 实例。
    virtual Http* CreateHttp() override;
    // 声明一个虚函数 CreateWebSocket()，它重写了基类 Board 的同名函数，用于创建 WebSocket 实例。
    virtual WebSocket* CreateWebSocket() override;
    // 声明一个虚函数 CreateMqtt()，它重写了基类 Board 的同名函数，用于创建 Mqtt 实例。
    virtual Mqtt* CreateMqtt() override;
    // 声明一个虚函数 CreateUdp()，它重写了基类 Board 的同名函数，用于创建 Udp 实例。
    virtual Udp* CreateUdp() override;
    // 声明一个虚函数 GetNetworkStateIcon()，它重写了基类 Board 的同名函数，返回网络状态图标的字符串。
    virtual const char* GetNetworkStateIcon() override;
    // 声明一个虚函数 SetPowerSaveMode()，它重写了基类 Board 的同名函数，用于设置电源保存模式。
    virtual void SetPowerSaveMode(bool enabled) override;
    // 声明一个虚函数 ResetWifiConfiguration()，用于重置 Wi-Fi 配置。
    virtual void ResetWifiConfiguration();
    // 声明一个虚函数 GetAudioCodec()，它重写了基类 Board 的同名函数，用于获取音频编解码器实例，这里默认返回 nullptr。
    virtual AudioCodec* GetAudioCodec() override { return nullptr; }
    // 声明一个虚函数 GetDeviceStatusJson()，它重写了基类 Board 的同名函数，返回设备状态的 JSON 字符串。
    virtual std::string GetDeviceStatusJson() override;
};

// #endif 是头文件保护宏的结束。
#endif // WIFI_BOARD_H
