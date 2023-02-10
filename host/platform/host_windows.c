#include "clem_host_platform.h"

#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <Dbt.h>
#include <ShlObj_core.h>
#include <Xinput.h>
#include <combaseapi.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <dinputd.h>

#ifdef _MSC_VER
#pragma comment(lib, "dxguid")
#pragma comment(lib, "dinput8")
#pragma comment(lib, "xinput")
#endif

unsigned clem_host_get_processor_number() { return (unsigned)GetCurrentProcessorNumber(); }

void clem_host_uuid_gen(ClemensHostUUID *uuid) {
    GUID guid;
    ZeroMemory(&guid, sizeof(guid));
    CoCreateGuid(&guid);
    uuid->data[0] = (char)((guid.Data1 >> 24) & 0xff);
    uuid->data[1] = (char)((guid.Data1 >> 16) & 0xff);
    uuid->data[2] = (char)((guid.Data1 >> 8) & 0xff);
    uuid->data[3] = (char)(guid.Data1 & 0xff);
    uuid->data[4] = (char)((guid.Data2 >> 8) & 0xff);
    uuid->data[5] = (char)(guid.Data2 & 0xff);
    uuid->data[6] = (char)((guid.Data3 >> 8) & 0xff);
    uuid->data[7] = (char)(guid.Data3 & 0xff);
    uuid->data[8] = guid.Data4[0];
    uuid->data[9] = guid.Data4[1];
    uuid->data[10] = guid.Data4[2];
    uuid->data[11] = guid.Data4[3];
    uuid->data[12] = guid.Data4[4];
    uuid->data[13] = guid.Data4[5];
    uuid->data[14] = guid.Data4[6];
    uuid->data[15] = guid.Data4[7];
}

char *get_process_executable_path(char *outpath, size_t* outpath_size) {
    DWORD path_size = (DWORD)(*outpath_size) - 1;
    DWORD actual_path_size = GetModuleFileName(NULL, outpath, path_size);
    if (actual_path_size == 0) {
        DWORD last_error = GetLastError();
        printf("GetModuleFileName failed with error %u\n", last_error);
        return NULL;
    }
    outpath[actual_path_size] = '\0';
    if (actual_path_size == path_size) {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            printf("GetModuleFileName truncated path %s\n", outpath);
            return NULL;
        }
    }
    return outpath;
}

char *get_local_user_data_directory(char *outpath, size_t outpath_size, const char *company_name,
                                    const char *app_name) {
    char *current_path = outpath;
    char *tail_path = outpath + outpath_size - 1;
    int state = 0;
    PWSTR directoryString;
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &directoryString);
    if (SUCCEEDED(hr)) {
        int cnt = WideCharToMultiByte(CP_UTF8, 0, directoryString, wcslen(directoryString), outpath,
                                      outpath_size, NULL, NULL);
        if (cnt <= 0) {
            return NULL;
        }
        current_path += cnt;
    }
    //  according to docs for SHGetKnownFolderPath, there will be no trailing '\'
    while (current_path < tail_path && state < 4) {
        if (!(state & 1)) {
            *current_path = '\\';
            ++current_path;
        } else if (state == 1) {
            strncpy(current_path, company_name, tail_path - current_path);
            current_path += strnlen(current_path, tail_path - current_path);
        } else if (state == 3) {
            strncpy(current_path, app_name, tail_path - current_path);
            current_path += strnlen(current_path, tail_path - current_path);
        }
        ++state;
    }
    CoTaskMemFree(directoryString);
    *current_path = '\0';

    //  did we finish our path construction
    if (state != 4)
        return NULL;

    return outpath;
}

////////////////////////////////////////////////////////////////////////////////
struct ClemensHostJoystickInfo {
    IDirectInputDevice8 *device;
    LONG axisDeadZoneX, axisDeadZoneY;
};
enum ClemensHostJoystickProvider {
    kClemensHostJoystick_None,
    kClemensHostJoystick_XInput,
    kClemensHostJoystick_DInput
};
static struct ClemensHostJoystickInfo s_DInputDevices[CLEM_HOST_JOYSTICK_LIMIT];
static unsigned s_DInputDeviceCount = 0;
static IDirectInput8 *s_DInput = NULL;
static enum ClemensHostJoystickProvider s_Provider = kClemensHostJoystick_None;
static DIJOYCONFIG s_DInputJoyConfig;
static bool s_hasPreferredJoyCfg = false;
static HHOOK s_win32Hook = NULL;
static BOOL s_flushDevices = FALSE;

static BOOL CALLBACK _clem_joystick_dinput_enum_cb(LPCDIDEVICEINSTANCE instance, LPVOID userData) {
    IDirectInputDevice8 *device = NULL;
    HRESULT hr;

    if (s_hasPreferredJoyCfg &&
        !IsEqualGUID(&instance->guidInstance, &s_DInputJoyConfig.guidInstance)) {
        return DIENUM_CONTINUE;
    }
    hr = IDirectInput8_CreateDevice(s_DInput, &instance->guidInstance, &device, NULL);
    if (hr == DI_OK) {
        IDirectInputDevice8_SetCooperativeLevel(device, GetActiveWindow(),
                                                DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
        // DISCL_FOREGROUND | DISCL_EXCLUSIVE);
        IDirectInputDevice8_SetDataFormat(device, &c_dfDIJoystick);
        IDirectInputDevice8_Acquire(device);
    }
    s_DInputDevices[s_DInputDeviceCount].device = device;
    s_DInputDeviceCount++;
    if (s_DInputDeviceCount >= CLEM_HOST_JOYSTICK_LIMIT)
        return DIENUM_STOP;
    return DIENUM_CONTINUE;
}

static BOOL CALLBACK _clem_joystick_dinput_object_cb(const DIDEVICEOBJECTINSTANCE *instance,
                                                     LPVOID userData) {
    struct ClemensHostJoystickInfo *joystick = (struct ClemensHostJoystickInfo *)userData;
    DIPROPRANGE propRange;
    DIPROPDWORD propDword;
    HRESULT hr;
    if (instance->dwType & DIDFT_AXIS) {
        propRange.diph.dwSize = sizeof(propRange);
        propRange.diph.dwHeaderSize = sizeof(propRange.diph);
        propRange.diph.dwHow = DIPH_BYID;
        propRange.diph.dwObj = instance->dwType; // Specify the enumerated axis
        propRange.lMin = -CLEM_HOST_JOYSTICK_AXIS_DELTA;
        propRange.lMax = CLEM_HOST_JOYSTICK_AXIS_DELTA;
        hr = IDirectInputDevice8_SetProperty(joystick->device, DIPROP_RANGE, &propRange.diph);
        if (FAILED(hr)) {
            printf("IDirectInputDevice8 enumerate object error: SetProperty range (%08x)\n", hr);
        }

        propDword.diph.dwSize = sizeof(propDword);
        propDword.diph.dwHeaderSize = sizeof(propDword.diph);
        propDword.diph.dwHow = DIPH_BYID;
        propDword.diph.dwObj = instance->dwType;
        hr = IDirectInputDevice8_GetProperty(joystick->device, DIPROP_DEADZONE, &propDword.diph);
        if (SUCCEEDED(hr)) {
            if (IsEqualGUID(&instance->guidType, &GUID_XAxis)) {
                joystick->axisDeadZoneX = (LONG)propDword.dwData;
            } else if (IsEqualGUID(&instance->guidType, &GUID_YAxis)) {
                joystick->axisDeadZoneY = (LONG)propDword.dwData;
            }
        } else {
            printf("IDirectInputDevice8 enumerate object error: GetProperty deadzone (%08x)\n", hr);
        }
    }
    return DIENUM_CONTINUE;
}

static LONG _clem_joystick_estimated_deadzone() {
    return (CLEM_HOST_JOYSTICK_AXIS_DELTA + CLEM_HOST_JOYSTICK_AXIS_DELTA) * 5 / 100;
}

static LRESULT _clem_win32_hook(int code, WPARAM wParam, LPARAM lParam) {
    CWPSTRUCT *msg;
    if (code >= 0) {
        msg = (CWPSTRUCT *)lParam;
        if (msg->message == WM_DEVICECHANGE) {
            switch (msg->wParam) {
            case DBT_DEVNODES_CHANGED:
                s_flushDevices = TRUE;
                break;
            }
        }
    }
    return CallNextHookEx(NULL, code, wParam, lParam);
}

static void _clem_joystick_dinput_devices_release() {
    if (s_DInputDeviceCount > 0) {
        while (s_DInputDeviceCount) {
            --s_DInputDeviceCount;
            if (s_DInputDevices[s_DInputDeviceCount].device) {
                IDirectInputDevice8_Release(s_DInputDevices[s_DInputDeviceCount].device);
                s_DInputDevices[s_DInputDeviceCount].device = NULL;
            }
        }
    }
}

static void _clem_joystick_dinput_enum() {
    IDirectInputJoyConfig8 *joyConfigI;
    HRESULT hr;
    int i;

    _clem_joystick_dinput_devices_release();

    hr = IDirectInput8_QueryInterface(s_DInput, &IID_IDirectInputJoyConfig8, (void **)&joyConfigI);
    if (FAILED(hr)) {
        printf("Failed to initialize DirectInput provider: Query IDirectInputJoyConfig8 (%08x)\n",
               hr);
        return;
    }

    s_DInputJoyConfig.dwSize = sizeof(s_DInputJoyConfig);
    hr = IDirectInputJoyConfig8_GetConfig(joyConfigI, 0, &s_DInputJoyConfig, DIJC_GUIDINSTANCE);
    if (SUCCEEDED(hr)) {
        s_hasPreferredJoyCfg = true;
    } else {
        s_hasPreferredJoyCfg = false;
    }
    IDirectInputJoyConfig8_Release(joyConfigI);

    s_DInputDeviceCount = 0;
    IDirectInput8_EnumDevices(s_DInput, DI8DEVCLASS_GAMECTRL, _clem_joystick_dinput_enum_cb, NULL,
                              DIEDFL_ALLDEVICES);
    for (i = 0; i < s_DInputDeviceCount; ++i) {
        IDirectInputDevice8_EnumObjects(s_DInputDevices[i].device, _clem_joystick_dinput_object_cb,
                                        (LPVOID)(&s_DInputDevices[i]), DIDFT_AXIS);
        if (s_DInputDevices[i].axisDeadZoneX == 0) {
            s_DInputDevices[i].axisDeadZoneX = _clem_joystick_estimated_deadzone();
        }
        if (s_DInputDevices[i].axisDeadZoneY == 0) {
            s_DInputDevices[i].axisDeadZoneY = _clem_joystick_estimated_deadzone();
        }
    }
}

static void _clem_joystick_dinput_start() {
    //  to detect device attach and detach and work with the application's windows
    //  event handler, inject a hook callback to
    HANDLE win32AppInstance = GetModuleHandle(NULL);
    DWORD win32ThreadID = GetCurrentThreadId();
    HRESULT hr;

    s_flushDevices = FALSE;
    s_win32Hook = SetWindowsHookExA(WH_CALLWNDPROC, (HOOKPROC)&_clem_win32_hook, win32AppInstance,
                                    win32ThreadID);

    hr = DirectInput8Create(win32AppInstance, DIRECTINPUT_VERSION, &IID_IDirectInput8,
                            (LPVOID)&s_DInput, NULL);
    if (hr != DI_OK) {
        printf("Failed to initialize DirectInput provider: DirectInput8Create (%08x)\n", hr);
        return;
    }

    _clem_joystick_dinput_enum();
}

static int16_t _clem_joystick_dinput_clamp_axis(LONG value, LONG deadzone) {
    int axisThreshLeft = -(deadzone / 2);
    int axisThreshRight = (deadzone / 2);
    if (value >= axisThreshRight) {
        value = ((value - axisThreshRight) * CLEM_HOST_JOYSTICK_AXIS_DELTA) /
                (CLEM_HOST_JOYSTICK_AXIS_DELTA - axisThreshRight);
    } else if (value <= axisThreshLeft) {
        value = -((axisThreshLeft - value) * CLEM_HOST_JOYSTICK_AXIS_DELTA) /
                (CLEM_HOST_JOYSTICK_AXIS_DELTA + axisThreshLeft);
    } else {
        value = 0;
    }
    return value;
}

static unsigned _clem_joystick_dinput(ClemensHostJoystick *joysticks) {
    struct ClemensHostJoystickInfo *joyinfo;
    unsigned count = 0;
    int i, btn;
    HRESULT hr;
    DIJOYSTATE state;

    if (s_flushDevices) {
        _clem_joystick_dinput_enum();
        s_flushDevices = FALSE;
    }

    for (i = 0; i < s_DInputDeviceCount; ++i, ++count) {
        joyinfo = &s_DInputDevices[i];
        if (!joyinfo->device) {
            joysticks[i].isConnected = false;
            continue;
        }
        hr = IDirectInputDevice8_GetDeviceState(joyinfo->device, sizeof(state), &state);
        if (hr == DI_OK) {
            joysticks[i].x[0] = _clem_joystick_dinput_clamp_axis(state.lX, joyinfo->axisDeadZoneX);
            joysticks[i].y[0] = _clem_joystick_dinput_clamp_axis(state.lY, joyinfo->axisDeadZoneY);
            joysticks[i].x[1] = 0;
            joysticks[i].y[1] = 0;
            joysticks[i].buttons = 0;
            for (btn = 0; btn < 32; ++btn) {
                if (state.rgbButtons[btn] & 0x80) {
                    joysticks[i].buttons |= (1 << btn);
                }
            }
            joysticks[i].isConnected = true;
        } else {
            if (hr == DIERR_INPUTLOST) {
                if (GetForegroundWindow() == GetActiveWindow()) {
                    IDirectInputDevice8_Acquire(s_DInputDevices[i].device);
                }
            }
            joysticks[i].isConnected = false;
        }
    }
    return count;
}

static unsigned _clem_joystick_xinput(ClemensHostJoystick *joysticks) {
    unsigned count = 0;
    for (unsigned i = 0; i < 4; ++i, ++count) {
        if (i >= CLEM_HOST_JOYSTICK_LIMIT)
            break;
        XINPUT_STATE state;
        DWORD result = XInputGetState(i, &state);
        joysticks[i].isConnected = (result == ERROR_SUCCESS && state.dwPacketNumber);
        if (!joysticks[i].isConnected)
            continue;
        memset(&joysticks[i], 0, sizeof(ClemensHostJoystick));
        if (state.Gamepad.wButtons & XINPUT_GAMEPAD_A) {
            joysticks[i].buttons |= CLEM_HOST_JOYSTICK_BUTTON_A;
        }
        if (state.Gamepad.wButtons & XINPUT_GAMEPAD_B) {
            joysticks[i].buttons |= CLEM_HOST_JOYSTICK_BUTTON_B;
        }
        if (state.Gamepad.wButtons & XINPUT_GAMEPAD_X) {
            joysticks[i].buttons |= CLEM_HOST_JOYSTICK_BUTTON_X;
        }
        if (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) {
            joysticks[i].buttons |= CLEM_HOST_JOYSTICK_BUTTON_Y;
        }
        joysticks[i].x[0] = state.Gamepad.sThumbLX;
        joysticks[i].y[0] = state.Gamepad.sThumbLY;
        joysticks[i].x[1] = state.Gamepad.sThumbRX;
        joysticks[i].y[1] = state.Gamepad.sThumbRY;
    }
    return count;
}

void clem_joystick_open_devices(const char *provider) {
    if (s_Provider != kClemensHostJoystick_None) {
        return;
    }
    if (strncmp(provider, CLEM_HOST_JOYSTICK_PROVIDER_XINPUT, 32) == 0) {
        XInputEnable(TRUE);
        s_Provider = kClemensHostJoystick_XInput;
    } else if (strncmp(provider, CLEM_HOST_JOYSTICK_PROVIDER_DINPUT, 32) == 0) {
        _clem_joystick_dinput_start();
        s_Provider = kClemensHostJoystick_DInput;
    }
}

unsigned clem_joystick_poll(ClemensHostJoystick *joysticks) {
    switch (s_Provider) {
    case kClemensHostJoystick_XInput:
        return _clem_joystick_xinput(joysticks);
    case kClemensHostJoystick_DInput:
        return _clem_joystick_dinput(joysticks);
    }
    return 0;
}

void clem_joystick_close_devices() {
    int i;

    switch (s_Provider) {
    case kClemensHostJoystick_XInput:
        XInputEnable(FALSE);
        break;
    case kClemensHostJoystick_DInput:
        _clem_joystick_dinput_devices_release();
        if (s_DInput) {
            IDirectInput8_Release(s_DInput);
            s_DInput = NULL;
        }
        if (s_win32Hook) {
            UnhookWindowsHookEx(s_win32Hook);
            s_win32Hook = NULL;
        }
        break;
    }
    s_Provider = kClemensHostJoystick_None;
}
