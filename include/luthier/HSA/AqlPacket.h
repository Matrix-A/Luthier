//===-- AqlPacket.h - HSA AQL Packet POD Wrapper ----------------*- C++ -*-===//
// AqlPacket.h HSA AQL 数据包 POD 包装器头文件
// Copyright 2022-2025 @ Northeastern University Computer Architecture Lab
//
// Licensed under the Apache License, Version 2.0 (the "License");
// 您可以在遵守许可证的情况下使用此文件
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===----------------------------------------------------------------------===//
///
/// \file
/// This file describes the \c luthier::hsa::AqlPacket struct,
/// a plain-old-data struct that provides an abstraction over the
/// HSA AQL packet.
/// 此文件描述 \c luthier::hsa::AqlPacket 结构体，一个提供 HSA AQL 数据包抽象的纯数据结构
//===----------------------------------------------------------------------===//
#ifndef LUTHIER_HSA_AQL_PACKET_H
#define LUTHIER_HSA_AQL_PACKET_H
#include <hsa/hsa.h>

namespace luthier::hsa {

/// \brief AMD Vendor Packet struct
/// AMD 供应商数据包结构体
typedef struct {
  uint16_t Header;
  uint8_t Format;
  uint8_t Rest[61];
} amd_vendor_packet_t;

/// \brief POD struct to provide an abstraction over HSA AQL packets as well
/// as some convenience methods to convert them to their specific type
/// POD 结构体，提供 HSA AQL 数据包的抽象以及一些将其转换为特定类型的便捷方法
struct AqlPacket {
  /// Generic packet struct
  struct {
    uint16_t Header;
    struct {
      uint8_t UserData[62];
    } Body;
  } Packet;

  /// \return the type of the packet
  /// \sa hsa_packet_type_t
  /// 返回数据包的类型
  [[nodiscard]] hsa_packet_type_t getPacketType() const {
    return static_cast<hsa_packet_type_t>(
        (Packet.Header >> HSA_PACKET_HEADER_TYPE) &
        ((1 << HSA_PACKET_HEADER_WIDTH_TYPE) - 1));
  }

  /// \return if the type of the packet is \c HSA_PACKET_TYPE_VENDOR_SPECIFIC
  /// returns a non-const \c amd_vendor_packet_t pointer to the contents of
  /// the packet; Otherwise, \c nullptr
  /// \sa amd_vendor_packet_t
  [[nodiscard]] amd_vendor_packet_t *asAMDVendor() {
    if (getPacketType() == HSA_PACKET_TYPE_VENDOR_SPECIFIC)
      return reinterpret_cast<amd_vendor_packet_t *>(&Packet);
    else
      return nullptr;
  }

  /// \return if the type of the packet is \c HSA_PACKET_TYPE_VENDOR_SPECIFIC
  /// returns a const \c amd_vendor_packet_t pointer to the contents of
  /// the packet; Otherwise, \c nullptr
  /// \sa amd_vendor_packet_t
  [[nodiscard]] const amd_vendor_packet_t *asAMDVendor() const {
    if (getPacketType() == HSA_PACKET_TYPE_VENDOR_SPECIFIC)
      return reinterpret_cast<const amd_vendor_packet_t *>(&Packet);
    else
      return nullptr;
  }

  /// \return if the type of the packet is \c HSA_PACKET_TYPE_KERNEL_DISPATCH
  /// returns a non-const \c hsa_kernel_dispatch_packet_t pointer to the
  /// contents of the packet; Otherwise, \c nullptr
  /// \sa hsa_kernel_dispatch_packet_t, HSA_PACKET_TYPE_KERNEL_DISPATCH
  [[nodiscard]] hsa_kernel_dispatch_packet_t *asKernelDispatch() {
    if (getPacketType() == HSA_PACKET_TYPE_KERNEL_DISPATCH)
      return reinterpret_cast<hsa_kernel_dispatch_packet_t *>(&Packet);
    else
      return nullptr;
  }

  /// \return if the type of the packet is \c HSA_PACKET_TYPE_KERNEL_DISPATCH
  /// returns a const \c hsa_kernel_dispatch_packet_t pointer to the
  /// contents of the packet; Otherwise, \c nullptr
  /// \sa hsa_kernel_dispatch_packet_t, HSA_PACKET_TYPE_KERNEL_DISPATCH
  [[nodiscard]] const hsa_kernel_dispatch_packet_t *asKernelDispatch() const {
    if (getPacketType() == HSA_PACKET_TYPE_KERNEL_DISPATCH)
      return reinterpret_cast<const hsa_kernel_dispatch_packet_t *>(&Packet);
    else
      return nullptr;
  }

  /// \return if the type of the packet is \c HSA_PACKET_TYPE_BARRIER_AND
  /// returns a non-const \c hsa_barrier_and_packet_t pointer to the
  /// contents of the packet; Otherwise, \c nullptr
  /// \sa hsa_barrier_and_packet_t, HSA_PACKET_TYPE_BARRIER_AND
  [[nodiscard]] hsa_barrier_and_packet_t *asBarrierAnd() {
    if (getPacketType() == HSA_PACKET_TYPE_BARRIER_AND)
      return reinterpret_cast<hsa_barrier_and_packet_t *>(&Packet);
    else
      return nullptr;
  }

  /// \return if the type of the packet is \c HSA_PACKET_TYPE_BARRIER_AND
  /// returns a const \c hsa_barrier_and_packet_t pointer to the
  /// contents of the packet; Otherwise, \c nullptr
  /// \sa hsa_barrier_and_packet_t, HSA_PACKET_TYPE_BARRIER_AND
  [[nodiscard]] const hsa_barrier_and_packet_t *asBarrierAnd() const {
    if (getPacketType() == HSA_PACKET_TYPE_BARRIER_AND)
      return reinterpret_cast<const hsa_barrier_and_packet_t *>(&Packet);
    else
      return nullptr;
  }

  /// \return if the type of the packet is \c HSA_PACKET_TYPE_BARRIER_OR
  /// returns a non-const \c hsa_barrier_or_packet_t pointer to the
  /// contents of the packet; Otherwise, \c nullptr
  /// \sa hsa_barrier_or_packet_t, HSA_PACKET_TYPE_BARRIER_OR
  [[nodiscard]] hsa_barrier_or_packet_t *asBarrierOr() {
    if (getPacketType() == HSA_PACKET_TYPE_BARRIER_OR)
      return reinterpret_cast<hsa_barrier_or_packet_t *>(&Packet);
    else
      return nullptr;
  }

  /// \return if the type of the packet is \c HSA_PACKET_TYPE_BARRIER_OR
  /// returns a const \c hsa_barrier_or_packet_t pointer to the
  /// contents of the packet; Otherwise, \c nullptr
  /// \sa hsa_barrier_or_packet_t, HSA_PACKET_TYPE_BARRIER_OR
  [[nodiscard]] const hsa_barrier_or_packet_t *asBarrierOr() const {
    if (getPacketType() == HSA_PACKET_TYPE_BARRIER_OR)
      return reinterpret_cast<const hsa_barrier_or_packet_t *>(&Packet);
    else
      return nullptr;
  }

  /// \return if the type of the packet is \c HSA_PACKET_TYPE_AGENT_DISPATCH
  /// returns a non-const \c hsa_agent_dispatch_packet_t pointer to the
  /// contents of the packet; Otherwise, \c nullptr
  /// \sa hsa_agent_dispatch_packet_t, HSA_PACKET_TYPE_AGENT_DISPATCH
  [[nodiscard]] hsa_agent_dispatch_packet_t *asAgentDispatch() {
    if (getPacketType() == HSA_PACKET_TYPE_AGENT_DISPATCH)
      return reinterpret_cast<hsa_agent_dispatch_packet_t *>(&Packet);
    else
      return nullptr;
  }

  /// \return if the type of the packet is \c HSA_PACKET_TYPE_AGENT_DISPATCH
  /// returns a const \c hsa_agent_dispatch_packet_t pointer to the
  /// contents of the packet; Otherwise, \c nullptr
  /// \sa hsa_agent_dispatch_packet_t, HSA_PACKET_TYPE_AGENT_DISPATCH
  [[nodiscard]] const hsa_agent_dispatch_packet_t *asAgentDispatch() const {
    if (getPacketType() == HSA_PACKET_TYPE_AGENT_DISPATCH)
      return reinterpret_cast<const hsa_agent_dispatch_packet_t *>(&Packet);
    else
      return nullptr;
  }
};

} // namespace luthier::hsa

#endif