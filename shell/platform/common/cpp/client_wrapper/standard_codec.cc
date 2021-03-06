// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// FLUTTER_NOLINT

// This file contains what would normally be standard_codec_serializer.cc,
// standard_message_codec.cc, and standard_method_codec.cc. They are grouped
// together to simplify use of the client wrapper, since the common case is
// that any client that needs one of these files needs all three.

#include <assert.h>

#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "include/flutter/standard_message_codec.h"
#include "include/flutter/standard_method_codec.h"
#include "standard_codec_serializer.h"

namespace flutter {

// ===== standard_codec_serializer.h =====

namespace {

// The order/values here must match the constants in message_codecs.dart.
enum class EncodedType {
  kNull = 0,
  kTrue,
  kFalse,
  kInt32,
  kInt64,
  kLargeInt,  // No longer used. If encountered, treat as kString.
  kFloat64,
  kString,
  kUInt8List,
  kInt32List,
  kInt64List,
  kFloat64List,
  kList,
  kMap,
};

// Returns the encoded type that should be written when serializing |value|.
EncodedType EncodedTypeForValue(const EncodableValue& value) {
#ifdef USE_LEGACY_ENCODABLE_VALUE
  switch (value.type()) {
    case EncodableValue::Type::kNull:
      return EncodedType::kNull;
    case EncodableValue::Type::kBool:
      return value.BoolValue() ? EncodedType::kTrue : EncodedType::kFalse;
    case EncodableValue::Type::kInt:
      return EncodedType::kInt32;
    case EncodableValue::Type::kLong:
      return EncodedType::kInt64;
    case EncodableValue::Type::kDouble:
      return EncodedType::kFloat64;
    case EncodableValue::Type::kString:
      return EncodedType::kString;
    case EncodableValue::Type::kByteList:
      return EncodedType::kUInt8List;
    case EncodableValue::Type::kIntList:
      return EncodedType::kInt32List;
    case EncodableValue::Type::kLongList:
      return EncodedType::kInt64List;
    case EncodableValue::Type::kDoubleList:
      return EncodedType::kFloat64List;
    case EncodableValue::Type::kList:
      return EncodedType::kList;
    case EncodableValue::Type::kMap:
      return EncodedType::kMap;
  }
#else
  switch (value.index()) {
    case 0:
      return EncodedType::kNull;
    case 1:
      return std::get<bool>(value) ? EncodedType::kTrue : EncodedType::kFalse;
    case 2:
      return EncodedType::kInt32;
    case 3:
      return EncodedType::kInt64;
    case 4:
      return EncodedType::kFloat64;
    case 5:
      return EncodedType::kString;
    case 6:
      return EncodedType::kUInt8List;
    case 7:
      return EncodedType::kInt32List;
    case 8:
      return EncodedType::kInt64List;
    case 9:
      return EncodedType::kFloat64List;
    case 10:
      return EncodedType::kList;
    case 11:
      return EncodedType::kMap;
  }
#endif
  assert(false);
  return EncodedType::kNull;
}

}  // namespace

StandardCodecSerializer::StandardCodecSerializer() = default;

StandardCodecSerializer::~StandardCodecSerializer() = default;

EncodableValue StandardCodecSerializer::ReadValue(
    ByteBufferStreamReader* stream) const {
  EncodedType type = static_cast<EncodedType>(stream->ReadByte());
  switch (type) {
    case EncodedType::kNull:
      return EncodableValue();
    case EncodedType::kTrue:
      return EncodableValue(true);
    case EncodedType::kFalse:
      return EncodableValue(false);
    case EncodedType::kInt32: {
      int32_t int_value = 0;
      stream->ReadBytes(reinterpret_cast<uint8_t*>(&int_value), 4);
      return EncodableValue(int_value);
    }
    case EncodedType::kInt64: {
      int64_t long_value = 0;
      stream->ReadBytes(reinterpret_cast<uint8_t*>(&long_value), 8);
      return EncodableValue(long_value);
    }
    case EncodedType::kFloat64: {
      double double_value = 0;
      stream->ReadAlignment(8);
      stream->ReadBytes(reinterpret_cast<uint8_t*>(&double_value), 8);
      return EncodableValue(double_value);
    }
    case EncodedType::kLargeInt:
    case EncodedType::kString: {
      size_t size = ReadSize(stream);
      std::string string_value;
      string_value.resize(size);
      stream->ReadBytes(reinterpret_cast<uint8_t*>(&string_value[0]), size);
      return EncodableValue(string_value);
    }
    case EncodedType::kUInt8List:
      return ReadVector<uint8_t>(stream);
    case EncodedType::kInt32List:
      return ReadVector<int32_t>(stream);
    case EncodedType::kInt64List:
      return ReadVector<int64_t>(stream);
    case EncodedType::kFloat64List:
      return ReadVector<double>(stream);
    case EncodedType::kList: {
      size_t length = ReadSize(stream);
      EncodableList list_value;
      list_value.reserve(length);
      for (size_t i = 0; i < length; ++i) {
        list_value.push_back(ReadValue(stream));
      }
      return EncodableValue(list_value);
    }
    case EncodedType::kMap: {
      size_t length = ReadSize(stream);
      EncodableMap map_value;
      for (size_t i = 0; i < length; ++i) {
        EncodableValue key = ReadValue(stream);
        EncodableValue value = ReadValue(stream);
        map_value.emplace(std::move(key), std::move(value));
      }
      return EncodableValue(map_value);
    }
  }
  std::cerr << "Unknown type in StandardCodecSerializer::ReadValue: "
            << static_cast<int>(type) << std::endl;
  return EncodableValue();
}

void StandardCodecSerializer::WriteValue(const EncodableValue& value,
                                         ByteBufferStreamWriter* stream) const {
  stream->WriteByte(static_cast<uint8_t>(EncodedTypeForValue(value)));
#ifdef USE_LEGACY_ENCODABLE_VALUE
  switch (value.type()) {
    case EncodableValue::Type::kNull:
    case EncodableValue::Type::kBool:
      // Null and bool are encoded directly in the type.
      break;
    case EncodableValue::Type::kInt: {
      int32_t int_value = value.IntValue();
      stream->WriteBytes(reinterpret_cast<const uint8_t*>(&int_value), 4);
      break;
    }
    case EncodableValue::Type::kLong: {
      int64_t long_value = value.LongValue();
      stream->WriteBytes(reinterpret_cast<const uint8_t*>(&long_value), 8);
      break;
    }
    case EncodableValue::Type::kDouble: {
      stream->WriteAlignment(8);
      double double_value = value.DoubleValue();
      stream->WriteBytes(reinterpret_cast<const uint8_t*>(&double_value), 8);
      break;
    }
    case EncodableValue::Type::kString: {
      const auto& string_value = value.StringValue();
      size_t size = string_value.size();
      WriteSize(size, stream);
      if (size > 0) {
        stream->WriteBytes(
            reinterpret_cast<const uint8_t*>(string_value.data()), size);
      }
      break;
    }
    case EncodableValue::Type::kByteList:
      WriteVector(value.ByteListValue(), stream);
      break;
    case EncodableValue::Type::kIntList:
      WriteVector(value.IntListValue(), stream);
      break;
    case EncodableValue::Type::kLongList:
      WriteVector(value.LongListValue(), stream);
      break;
    case EncodableValue::Type::kDoubleList:
      WriteVector(value.DoubleListValue(), stream);
      break;
    case EncodableValue::Type::kList:
      WriteSize(value.ListValue().size(), stream);
      for (const auto& item : value.ListValue()) {
        WriteValue(item, stream);
      }
      break;
    case EncodableValue::Type::kMap:
      WriteSize(value.MapValue().size(), stream);
      for (const auto& pair : value.MapValue()) {
        WriteValue(pair.first, stream);
        WriteValue(pair.second, stream);
      }
      break;
  }
#else
  // TODO: Consider replacing this this with a std::visitor.
  switch (value.index()) {
    case 0:
    case 1:
      // Null and bool are encoded directly in the type.
      break;
    case 2: {
      int32_t int_value = std::get<int32_t>(value);
      stream->WriteBytes(reinterpret_cast<const uint8_t*>(&int_value), 4);
      break;
    }
    case 3: {
      int64_t long_value = std::get<int64_t>(value);
      stream->WriteBytes(reinterpret_cast<const uint8_t*>(&long_value), 8);
      break;
    }
    case 4: {
      stream->WriteAlignment(8);
      double double_value = std::get<double>(value);
      stream->WriteBytes(reinterpret_cast<const uint8_t*>(&double_value), 8);
      break;
    }
    case 5: {
      const auto& string_value = std::get<std::string>(value);
      size_t size = string_value.size();
      WriteSize(size, stream);
      if (size > 0) {
        stream->WriteBytes(
            reinterpret_cast<const uint8_t*>(string_value.data()), size);
      }
      break;
    }
    case 6:
      WriteVector(std::get<std::vector<uint8_t>>(value), stream);
      break;
    case 7:
      WriteVector(std::get<std::vector<int32_t>>(value), stream);
      break;
    case 8:
      WriteVector(std::get<std::vector<int64_t>>(value), stream);
      break;
    case 9:
      WriteVector(std::get<std::vector<double>>(value), stream);
      break;
    case 10: {
      const auto& list = std::get<EncodableList>(value);
      WriteSize(list.size(), stream);
      for (const auto& item : list) {
        WriteValue(item, stream);
      }
      break;
    }
    case 11: {
      const auto& map = std::get<EncodableMap>(value);
      WriteSize(map.size(), stream);
      for (const auto& pair : map) {
        WriteValue(pair.first, stream);
        WriteValue(pair.second, stream);
      }
      break;
    }
  }
#endif
}

size_t StandardCodecSerializer::ReadSize(ByteBufferStreamReader* stream) const {
  uint8_t byte = stream->ReadByte();
  if (byte < 254) {
    return byte;
  } else if (byte == 254) {
    uint16_t value;
    stream->ReadBytes(reinterpret_cast<uint8_t*>(&value), 2);
    return value;
  } else {
    uint32_t value;
    stream->ReadBytes(reinterpret_cast<uint8_t*>(&value), 4);
    return value;
  }
}

void StandardCodecSerializer::WriteSize(size_t size,
                                        ByteBufferStreamWriter* stream) const {
  if (size < 254) {
    stream->WriteByte(static_cast<uint8_t>(size));
  } else if (size <= 0xffff) {
    stream->WriteByte(254);
    uint16_t value = static_cast<uint16_t>(size);
    stream->WriteBytes(reinterpret_cast<uint8_t*>(&value), 2);
  } else {
    stream->WriteByte(255);
    uint32_t value = static_cast<uint32_t>(size);
    stream->WriteBytes(reinterpret_cast<uint8_t*>(&value), 4);
  }
}

template <typename T>
EncodableValue StandardCodecSerializer::ReadVector(
    ByteBufferStreamReader* stream) const {
  size_t count = ReadSize(stream);
  std::vector<T> vector;
  vector.resize(count);
  uint8_t type_size = static_cast<uint8_t>(sizeof(T));
  if (type_size > 1) {
    stream->ReadAlignment(type_size);
  }
  stream->ReadBytes(reinterpret_cast<uint8_t*>(vector.data()),
                    count * type_size);
  return EncodableValue(vector);
}

template <typename T>
void StandardCodecSerializer::WriteVector(
    const std::vector<T> vector,
    ByteBufferStreamWriter* stream) const {
  size_t count = vector.size();
  WriteSize(count, stream);
  if (count == 0) {
    return;
  }
  uint8_t type_size = static_cast<uint8_t>(sizeof(T));
  if (type_size > 1) {
    stream->WriteAlignment(type_size);
  }
  stream->WriteBytes(reinterpret_cast<const uint8_t*>(vector.data()),
                     count * type_size);
}

// ===== standard_message_codec.h =====

// static
const StandardMessageCodec& StandardMessageCodec::GetInstance() {
  static StandardMessageCodec sInstance;
  return sInstance;
}

StandardMessageCodec::StandardMessageCodec() = default;

StandardMessageCodec::~StandardMessageCodec() = default;

std::unique_ptr<EncodableValue> StandardMessageCodec::DecodeMessageInternal(
    const uint8_t* binary_message,
    size_t message_size) const {
  StandardCodecSerializer serializer;
  ByteBufferStreamReader stream(binary_message, message_size);
  return std::make_unique<EncodableValue>(serializer.ReadValue(&stream));
}

std::unique_ptr<std::vector<uint8_t>>
StandardMessageCodec::EncodeMessageInternal(
    const EncodableValue& message) const {
  StandardCodecSerializer serializer;
  auto encoded = std::make_unique<std::vector<uint8_t>>();
  ByteBufferStreamWriter stream(encoded.get());
  serializer.WriteValue(message, &stream);
  return encoded;
}

// ===== standard_method_codec.h =====

// static
const StandardMethodCodec& StandardMethodCodec::GetInstance() {
  static StandardMethodCodec sInstance;
  return sInstance;
}

std::unique_ptr<MethodCall<EncodableValue>>
StandardMethodCodec::DecodeMethodCallInternal(const uint8_t* message,
                                              size_t message_size) const {
  StandardCodecSerializer serializer;
  ByteBufferStreamReader stream(message, message_size);
#ifdef USE_LEGACY_ENCODABLE_VALUE
  EncodableValue method_name = serializer.ReadValue(&stream);
  if (!method_name.IsString()) {
    std::cerr << "Invalid method call; method name is not a string."
              << std::endl;
    return nullptr;
  }
  auto arguments =
      std::make_unique<EncodableValue>(serializer.ReadValue(&stream));
  return std::make_unique<MethodCall<EncodableValue>>(method_name.StringValue(),
                                                      std::move(arguments));
#else
  EncodableValue method_name_value = serializer.ReadValue(&stream);
  const auto* method_name = std::get_if<std::string>(&method_name_value);
  if (!method_name) {
    std::cerr << "Invalid method call; method name is not a string."
              << std::endl;
    return nullptr;
  }
  auto arguments =
      std::make_unique<EncodableValue>(serializer.ReadValue(&stream));
  return std::make_unique<MethodCall<EncodableValue>>(*method_name,
                                                      std::move(arguments));
#endif
}

std::unique_ptr<std::vector<uint8_t>>
StandardMethodCodec::EncodeMethodCallInternal(
    const MethodCall<EncodableValue>& method_call) const {
  StandardCodecSerializer serializer;
  auto encoded = std::make_unique<std::vector<uint8_t>>();
  ByteBufferStreamWriter stream(encoded.get());
  serializer.WriteValue(EncodableValue(method_call.method_name()), &stream);
  if (method_call.arguments()) {
    serializer.WriteValue(*method_call.arguments(), &stream);
  } else {
    serializer.WriteValue(EncodableValue(), &stream);
  }
  return encoded;
}

std::unique_ptr<std::vector<uint8_t>>
StandardMethodCodec::EncodeSuccessEnvelopeInternal(
    const EncodableValue* result) const {
  StandardCodecSerializer serializer;
  auto encoded = std::make_unique<std::vector<uint8_t>>();
  ByteBufferStreamWriter stream(encoded.get());
  stream.WriteByte(0);
  if (result) {
    serializer.WriteValue(*result, &stream);
  } else {
    serializer.WriteValue(EncodableValue(), &stream);
  }
  return encoded;
}

std::unique_ptr<std::vector<uint8_t>>
StandardMethodCodec::EncodeErrorEnvelopeInternal(
    const std::string& error_code,
    const std::string& error_message,
    const EncodableValue* error_details) const {
  StandardCodecSerializer serializer;
  auto encoded = std::make_unique<std::vector<uint8_t>>();
  ByteBufferStreamWriter stream(encoded.get());
  stream.WriteByte(1);
  serializer.WriteValue(EncodableValue(error_code), &stream);
  if (error_message.empty()) {
    serializer.WriteValue(EncodableValue(), &stream);
  } else {
    serializer.WriteValue(EncodableValue(error_message), &stream);
  }
  if (error_details) {
    serializer.WriteValue(*error_details, &stream);
  } else {
    serializer.WriteValue(EncodableValue(), &stream);
  }
  return encoded;
}

bool StandardMethodCodec::DecodeAndProcessResponseEnvelopeInternal(
    const uint8_t* response,
    size_t response_size,
    MethodResult<EncodableValue>* result) const {
  StandardCodecSerializer serializer;
  ByteBufferStreamReader stream(response, response_size);
  uint8_t flag = stream.ReadByte();
  switch (flag) {
    case 0: {
      EncodableValue value = serializer.ReadValue(&stream);
      result->Success(value.IsNull() ? nullptr : &value);
      return true;
    }
    case 1: {
      EncodableValue code = serializer.ReadValue(&stream);
      EncodableValue message = serializer.ReadValue(&stream);
      EncodableValue details = serializer.ReadValue(&stream);
#ifdef USE_LEGACY_ENCODABLE_VALUE
      result->Error(code.StringValue(),
                    message.IsNull() ? "" : message.StringValue(),
                    details.IsNull() ? nullptr : &details);
#else
      result->Error(std::get<std::string>(code),
                    message.IsNull() ? "" : std::get<std::string>(message),
                    details.IsNull() ? nullptr : &details);
#endif
      return true;
    }
    default:
      return false;
  }
}

}  // namespace flutter
