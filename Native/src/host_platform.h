#pragma once
// The public input ABI uses the original desktop key/button numbers on every
// platform. Android receives these values from Unity; it never polls Win32.
#if defined(_WIN32)
#include <windows.h>
#include <Xinput.h>
#else
#include <cstdint>
using BYTE=std::uint8_t;
using WORD=std::uint16_t;
using SHORT=std::int16_t;
using HWND=void*;
using COLORREF=std::uint32_t;
constexpr COLORREF RGB(unsigned r,unsigned g,unsigned b){return (r&255)|((g&255)<<8)|((b&255)<<16);}
constexpr unsigned GetRValue(COLORREF c){return c&255;}
constexpr unsigned GetGValue(COLORREF c){return (c>>8)&255;}
constexpr unsigned GetBValue(COLORREF c){return (c>>16)&255;}
struct XINPUT_GAMEPAD {
    WORD wButtons{};BYTE bLeftTrigger{},bRightTrigger{};
    SHORT sThumbLX{},sThumbLY{},sThumbRX{},sThumbRY{};
};
struct XINPUT_STATE{std::uint32_t dwPacketNumber{};XINPUT_GAMEPAD Gamepad;};
constexpr int VK_BACK=0x08,VK_RETURN=0x0d,VK_ESCAPE=0x1b,VK_SPACE=0x20;
constexpr int VK_LEFT=0x25,VK_UP=0x26,VK_RIGHT=0x27,VK_DOWN=0x28;
constexpr int VK_F1=0x70,VK_F2=0x71,VK_F3=0x72,VK_F5=0x74;
constexpr WORD XINPUT_GAMEPAD_DPAD_UP=0x0001,XINPUT_GAMEPAD_DPAD_DOWN=0x0002,
    XINPUT_GAMEPAD_DPAD_LEFT=0x0004,XINPUT_GAMEPAD_DPAD_RIGHT=0x0008,
    XINPUT_GAMEPAD_START=0x0010,XINPUT_GAMEPAD_A=0x1000,
    XINPUT_GAMEPAD_B=0x2000,XINPUT_GAMEPAD_X=0x4000,XINPUT_GAMEPAD_Y=0x8000;
#endif
