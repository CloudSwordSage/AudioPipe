/*
 * @Time    : 2026/4/14 21:17:12
 * @Author  : 墨烟行(GitHub UserName: CloudSwordSage)
 * @File    : wasapi_input.cpp
 * @License : GPL-3.0
 * @Desc    : WASAPI 音频输入流实现
 */

#include <initguid.h>

#include "wasapi_input.hpp"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ks.h>
#include <ksmedia.h>
#include <objbase.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <chrono>
#include <stdexcept>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace {

    class ComScope {
        public:
            explicit ComScope(DWORD mode) {
                HRESULT hr = CoInitializeEx(nullptr, mode);
                if (hr == S_OK || hr == S_FALSE) {
                    initialized_ = true;
                    return;
                }
                if (hr == RPC_E_CHANGED_MODE) {
                    initialized_ = false;
                    return;
                }
                throw std::runtime_error("CoInitializeEx failed");
            }

            ~ComScope() {
                if (initialized_) {
                    CoUninitialize();
                }
            }

        private:
            bool initialized_{false};
    };

    std::string utf16_to_utf8(const std::wstring & value) {
        if (value.empty()) {
            return {};
        }
        int required = WideCharToMultiByte(
            CP_UTF8,
            0,
            value.c_str(),
            static_cast<int>(value.size()),
            nullptr,
            0,
            nullptr,
            nullptr
        );
        if (required <= 0) {
            throw std::runtime_error("WideCharToMultiByte failed");
        }
        std::string out(static_cast<size_t>(required), '\0');
        int converted = WideCharToMultiByte(
            CP_UTF8,
            0,
            value.c_str(),
            static_cast<int>(value.size()),
            out.data(),
            required,
            nullptr,
            nullptr
        );
        if (converted != required) {
            throw std::runtime_error("WideCharToMultiByte failed");
        }
        return out;
    }

    std::wstring utf8_to_utf16(const std::string & value) {
        if (value.empty()) {
            return {};
        }
        int required = MultiByteToWideChar(
            CP_UTF8,
            0,
            value.c_str(),
            static_cast<int>(value.size()),
            nullptr,
            0
        );
        if (required <= 0) {
            throw std::runtime_error("MultiByteToWideChar failed");
        }
        std::wstring out(static_cast<size_t>(required), L'\0');
        int converted = MultiByteToWideChar(
            CP_UTF8,
            0,
            value.c_str(),
            static_cast<int>(value.size()),
            out.data(),
            required
        );
        if (converted != required) {
            throw std::runtime_error("MultiByteToWideChar failed");
        }
        return out;
    }

    template <typename T> void check_hr(HRESULT hr, T && message) {
        if (FAILED(hr)) {
            throw std::runtime_error(std::forward<T>(message));
        }
    }

    std::string get_device_friendly_name(IMMDevice * device) {
        ComPtr<IPropertyStore> property_store;
        check_hr(
            device->OpenPropertyStore(STGM_READ, property_store.GetAddressOf()),
            "OpenPropertyStore failed"
        );

        PROPVARIANT value;
        PropVariantInit(&value);
        HRESULT hr = property_store->GetValue(PKEY_Device_FriendlyName, &value);
        if (FAILED(hr)) {
            PropVariantClear(&value);
            throw std::runtime_error(
                "GetValue(PKEY_Device_FriendlyName) failed"
            );
        }

        std::wstring name = value.pwszVal != nullptr ? value.pwszVal : L"";
        PropVariantClear(&value);
        return utf16_to_utf8(name);
    }

} // namespace

std::vector<audio_pipe::InputDeviceInfo> enumerate_input_devices() {
    ComScope com_scope(COINIT_MULTITHREADED);

    ComPtr<IMMDeviceEnumerator> enumerator;
    check_hr(
        CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            IID_PPV_ARGS(enumerator.GetAddressOf())
        ),
        "CoCreateInstance(MMDeviceEnumerator) failed"
    );

    ComPtr<IMMDeviceCollection> collection;
    check_hr(
        enumerator->EnumAudioEndpoints(
            eCapture,
            DEVICE_STATE_ACTIVE,
            collection.GetAddressOf()
        ),
        "EnumAudioEndpoints failed"
    );

    UINT count = 0;
    check_hr(collection->GetCount(&count), "GetCount failed");

    std::vector<audio_pipe::InputDeviceInfo> devices;
    devices.reserve(count);
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        check_hr(
            collection->Item(i, device.GetAddressOf()),
            "DeviceCollection Item failed"
        );

        LPWSTR device_id = nullptr;
        check_hr(device->GetId(&device_id), "IMMDevice::GetId failed");
        std::wstring id_w = device_id != nullptr ? device_id : L"";
        CoTaskMemFree(device_id);

        ComPtr<IPropertyStore> property_store;
        check_hr(
            device->OpenPropertyStore(STGM_READ, property_store.GetAddressOf()),
            "OpenPropertyStore failed"
        );

        PROPVARIANT format_value;
        PropVariantInit(&format_value);
        HRESULT format_hr = property_store->GetValue(
            PKEY_AudioEngine_DeviceFormat,
            &format_value
        );
        if (FAILED(format_hr)) {
            PropVariantClear(&format_value);
            throw std::runtime_error(
                "GetValue(PKEY_AudioEngine_DeviceFormat) failed"
            );
        }
        if (format_value.vt != (VT_BLOB | VT_VECTOR) &&
            format_value.vt != VT_BLOB) {
            PropVariantClear(&format_value);
            throw std::runtime_error(
                "PKEY_AudioEngine_DeviceFormat has invalid type"
            );
        }
        if (format_value.blob.cbSize < sizeof(WAVEFORMATEX) ||
            format_value.blob.pBlobData == nullptr) {
            PropVariantClear(&format_value);
            throw std::runtime_error(
                "PKEY_AudioEngine_DeviceFormat has invalid payload"
            );
        }
        auto * device_format =
            reinterpret_cast<WAVEFORMATEX *>(format_value.blob.pBlobData);

        audio_pipe::InputDeviceInfo info{};
        info.device_id = utf16_to_utf8(id_w);
        info.display_name = get_device_friendly_name(device.Get());
        info.sample_rate = device_format->nSamplesPerSec;
        info.channels = device_format->nChannels;
        info.bit_depth = device_format->wBitsPerSample;
        if (device_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
            device_format->cbSize >= 22 &&
            format_value.blob.cbSize >= sizeof(WAVEFORMATEXTENSIBLE)) {
            auto * extensible =
                reinterpret_cast<WAVEFORMATEXTENSIBLE *>(device_format);
            info.bit_depth = extensible->Samples.wValidBitsPerSample;
        }
        PropVariantClear(&format_value);
        devices.push_back(std::move(info));
    }

    return devices;
}

WasapiInputStream::WasapiInputStream(
    std::string device_id,
    uint32_t sample_rate,
    uint16_t bit_depth,
    uint16_t channels,
    uint32_t frame_size
)
    : device_id_(std::move(device_id)), sample_rate_(sample_rate),
      bit_depth_(bit_depth), frame_size_(frame_size), channels_(channels) {
    if (device_id_.empty()) {
        throw std::invalid_argument("device_id must not be empty");
    }
    if (sample_rate_ == 0) {
        throw std::invalid_argument("sample_rate must be greater than 0");
    }
    if (frame_size_ == 0) {
        throw std::invalid_argument("frame_size must be greater than 0");
    }
    if (bit_depth_ != 16 && bit_depth_ != 24 && bit_depth_ != 32) {
        throw std::invalid_argument("bit_depth must be one of: 16, 24, 32");
    }
    if (channels_ == 0) {
        throw std::invalid_argument("channels must be greater than 0");
    }
}

WasapiInputStream::~WasapiInputStream() {
    stop();
}

void WasapiInputStream::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        throw std::runtime_error("stream already started");
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
        pending_.clear();
        last_error_.clear();
        start_completed_ = false;
        start_succeeded_ = false;
    }

    capture_thread_ = std::thread([this]() {
        try {
            capture_loop();
        } catch (const std::exception & e) {
            std::lock_guard<std::mutex> lock(mutex_);
            last_error_ = e.what();
            if (!start_completed_) {
                start_completed_ = true;
                start_succeeded_ = false;
            }
            running_.store(false);
            cv_.notify_all();
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            last_error_ = "unknown capture error";
            if (!start_completed_) {
                start_completed_ = true;
                start_succeeded_ = false;
            }
            running_.store(false);
            cv_.notify_all();
        }
    });

    std::unique_lock<std::mutex> lock(mutex_);
    bool ready = cv_.wait_for(lock, std::chrono::seconds(3), [this]() {
        return start_completed_;
    });

    if (!ready) {
        last_error_ = "start timeout";
        running_.store(false);
        lock.unlock();
        if (capture_thread_.joinable()) {
            capture_thread_.join();
        }
        throw std::runtime_error("start timeout");
    }

    if (!start_succeeded_) {
        std::string error =
            last_error_.empty() ? "failed to start stream" : last_error_;
        lock.unlock();
        if (capture_thread_.joinable()) {
            capture_thread_.join();
        }
        throw std::runtime_error(error);
    }
}

std::vector<uint8_t> WasapiInputStream::get_frame(int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    const bool has_data =
        cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() {
            return !queue_.empty() || !running_.load();
        });

    if (!has_data || queue_.empty()) {
        if (!last_error_.empty()) {
            throw std::runtime_error(last_error_);
        }
        if (!running_.load()) {
            throw std::runtime_error("stream stopped");
        }
        throw std::runtime_error("timeout waiting for frame");
    }

    std::vector<uint8_t> frame = std::move(queue_.front());
    queue_.pop_front();
    return frame;
}

void WasapiInputStream::stop() {
    bool was_running = running_.exchange(false);
    cv_.notify_all();
    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }
    if (was_running) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
        pending_.clear();
    }
}

void WasapiInputStream::push_audio_bytes(const uint8_t * data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.insert(pending_.end(), data, data + size);

    const size_t target_bytes = static_cast<size_t>(frame_size_) * block_align_;
    while (pending_.size() >= target_bytes) {
        std::vector<uint8_t> frame(
            pending_.begin(),
            pending_.begin() + static_cast<std::ptrdiff_t>(target_bytes)
        );
        pending_.erase(
            pending_.begin(),
            pending_.begin() + static_cast<std::ptrdiff_t>(target_bytes)
        );
        queue_.push_back(std::move(frame));
        if (queue_.size() > max_queue_size_) {
            queue_.pop_front();
        }
        cv_.notify_one();
    }
}

void WasapiInputStream::capture_loop() {
    ComScope com_scope(COINIT_MULTITHREADED);

    ComPtr<IMMDeviceEnumerator> enumerator;
    check_hr(
        CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            IID_PPV_ARGS(enumerator.GetAddressOf())
        ),
        "CoCreateInstance(MMDeviceEnumerator) failed"
    );

    const std::wstring device_id_w = utf8_to_utf16(device_id_);
    ComPtr<IMMDevice> device;
    check_hr(
        enumerator->GetDevice(device_id_w.c_str(), device.GetAddressOf()),
        "GetDevice failed"
    );

    ComPtr<IAudioClient> audio_client;
    check_hr(
        device->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void **>(audio_client.GetAddressOf())
        ),
        "Activate IAudioClient failed"
    );

    WAVEFORMATEX * mix_format = nullptr;
    check_hr(audio_client->GetMixFormat(&mix_format), "GetMixFormat failed");

    DWORD channel_mask = 0;
    if (mix_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        mix_format->cbSize >= 22) {
        auto * mix_ext = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(mix_format);
        channel_mask = mix_ext->dwChannelMask;
    }
    CoTaskMemFree(mix_format);

    if (channels_ == 0) {
        throw std::runtime_error("invalid channel count");
    }

    WAVEFORMATEXTENSIBLE request_format{};
    request_format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    request_format.Format.nChannels = channels_;
    request_format.Format.nSamplesPerSec = sample_rate_;
    request_format.Format.wBitsPerSample = bit_depth_;
    request_format.Format.nBlockAlign = static_cast<WORD>(
        (request_format.Format.nChannels *
         request_format.Format.wBitsPerSample) /
        8
    );
    request_format.Format.nAvgBytesPerSec =
        request_format.Format.nSamplesPerSec *
        request_format.Format.nBlockAlign;
    request_format.Format.cbSize = 22;
    request_format.Samples.wValidBitsPerSample = bit_depth_;
    request_format.dwChannelMask = channel_mask;
    request_format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    block_align_ = request_format.Format.nBlockAlign;
    if (block_align_ == 0) {
        throw std::runtime_error("invalid block align");
    }

    HRESULT supported = audio_client->IsFormatSupported(
        AUDCLNT_SHAREMODE_EXCLUSIVE,
        reinterpret_cast<WAVEFORMATEX *>(&request_format),
        nullptr
    );
    check_hr(supported, "exclusive mode does not support requested format");

    REFERENCE_TIME default_period = 0;
    REFERENCE_TIME minimum_period = 0;
    check_hr(
        audio_client->GetDevicePeriod(&default_period, &minimum_period),
        "GetDevicePeriod failed"
    );
    REFERENCE_TIME buffer_duration =
        minimum_period > 0 ? minimum_period : default_period;

    check_hr(
        audio_client->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            0,
            buffer_duration,
            buffer_duration,
            reinterpret_cast<WAVEFORMATEX *>(&request_format),
            nullptr
        ),
        "IAudioClient::Initialize failed"
    );

    ComPtr<IAudioCaptureClient> capture_client;
    check_hr(
        audio_client->GetService(
            __uuidof(IAudioCaptureClient),
            reinterpret_cast<void **>(capture_client.GetAddressOf())
        ),
        "GetService(IAudioCaptureClient) failed"
    );

    check_hr(audio_client->Start(), "IAudioClient::Start failed");
    {
        std::lock_guard<std::mutex> lock(mutex_);
        start_completed_ = true;
        start_succeeded_ = true;
        cv_.notify_all();
    }

    while (running_.load()) {
        UINT32 packet_frames = 0;
        check_hr(
            capture_client->GetNextPacketSize(&packet_frames),
            "GetNextPacketSize failed"
        );

        while (packet_frames > 0) {
            BYTE * data = nullptr;
            UINT32 num_frames = 0;
            DWORD flags = 0;
            check_hr(
                capture_client
                    ->GetBuffer(&data, &num_frames, &flags, nullptr, nullptr),
                "GetBuffer failed"
            );

            const size_t bytes = static_cast<size_t>(num_frames) * block_align_;
            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0) {
                std::vector<uint8_t> silent(bytes, 0);
                push_audio_bytes(silent.data(), silent.size());
            } else {
                push_audio_bytes(data, bytes);
            }

            check_hr(
                capture_client->ReleaseBuffer(num_frames),
                "ReleaseBuffer failed"
            );
            check_hr(
                capture_client->GetNextPacketSize(&packet_frames),
                "GetNextPacketSize failed"
            );
        }
        ::Sleep(2);
    }

    audio_client->Stop();
}
