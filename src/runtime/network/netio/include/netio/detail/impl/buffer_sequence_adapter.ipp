#ifndef NETIO_DETAIL_IMPL_BUFFER_SEQUENCE_ADAPTER_IPP
#define NETIO_DETAIL_IMPL_BUFFER_SEQUENCE_ADAPTER_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS_RUNTIME)

#include <robuffer.h>
#include <windows.storage.streams.h>
#include <wrl/implements.h>
#include "netio/detail/buffer_sequence_adapter.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class winrt_buffer_impl :
  public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<
      Microsoft::WRL::RuntimeClassType::WinRtClassicComMix>,
    ABI::Windows::Storage::Streams::IBuffer,
    Windows::Storage::Streams::IBufferByteAccess>
{
public:
  explicit winrt_buffer_impl(const netio::const_buffer& b)
  {
    bytes_ = const_cast<byte*>(static_cast<const byte*>(b.data()));
    length_ = b.size();
    capacity_ = b.size();
  }

  explicit winrt_buffer_impl(const netio::mutable_buffer& b)
  {
    bytes_ = static_cast<byte*>(b.data());
    length_ = 0;
    capacity_ = b.size();
  }

  ~winrt_buffer_impl()
  {
  }

  STDMETHODIMP Buffer(byte** value)
  {
    *value = bytes_;
    return S_OK;
  }

  STDMETHODIMP get_Capacity(UINT32* value)
  {
    *value = capacity_;
    return S_OK;
  }

  STDMETHODIMP get_Length(UINT32 *value)
  {
    *value = length_;
    return S_OK;
  }

  STDMETHODIMP put_Length(UINT32 value)
  {
    if (value > capacity_)
      return E_INVALIDARG;
    length_ = value;
    return S_OK;
  }

private:
  byte* bytes_;
  UINT32 length_;
  UINT32 capacity_;
};

void buffer_sequence_adapter_base::init_native_buffer(
    buffer_sequence_adapter_base::native_buffer_type& buf,
    const netio::mutable_buffer& buffer)
{
  std::memset(&buf, 0, sizeof(native_buffer_type));
  Microsoft::WRL::ComPtr<IInspectable> insp
    = Microsoft::WRL::Make<winrt_buffer_impl>(buffer);
  buf = reinterpret_cast<Windows::Storage::Streams::IBuffer^>(insp.Get());
}

void buffer_sequence_adapter_base::init_native_buffer(
    buffer_sequence_adapter_base::native_buffer_type& buf,
    const netio::const_buffer& buffer)
{
  std::memset(&buf, 0, sizeof(native_buffer_type));
  Microsoft::WRL::ComPtr<IInspectable> insp
    = Microsoft::WRL::Make<winrt_buffer_impl>(buffer);
  Platform::Object^ buf_obj = reinterpret_cast<Platform::Object^>(insp.Get());
  buf = reinterpret_cast<Windows::Storage::Streams::IBuffer^>(insp.Get());
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_WINDOWS_RUNTIME)

#endif // NETIO_DETAIL_IMPL_BUFFER_SEQUENCE_ADAPTER_IPP
