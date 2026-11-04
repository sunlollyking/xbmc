/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "HighScoreDecoder.h"

#include "utils/log.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <string_view>

#include <tinyxml2.h>

using namespace KODI;
using namespace GAME;

namespace
{
using tinyxml2::XMLElement;

//! A clone's description can name its parent, which can name its own
constexpr int MAX_SAMEAS_DEPTH = 4;
//! High score files are a few hundred bytes; anything this large is not one
constexpr size_t MAX_DATA_SIZE = 1024 * 1024;
//! Far more rows or loop passes than any table holds
constexpr int MAX_COUNT = 10000;

//! Thrown, and caught in Decode(), for a description this can't follow
struct Unsupported
{
  std::string what;
};

std::string_view Attr(const XMLElement* element, const char* name)
{
  const char* value = element->Attribute(name);
  return value != nullptr ? std::string_view(value) : std::string_view();
}

std::string Text(const XMLElement* element)
{
  const char* text = element->GetText();
  return text != nullptr ? text : "";
}

std::optional<long double> ParseNumber(std::string_view text)
{
  while (!text.empty() && text.front() == ' ')
    text.remove_prefix(1);
  while (!text.empty() && text.back() == ' ')
    text.remove_suffix(1);
  if (text.empty())
    return std::nullopt;
  if (text.front() == '+')
    text.remove_prefix(1);

  double value = 0.0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc() || end != text.data() + text.size())
    return std::nullopt;
  return value;
}

std::optional<long long> ParseInteger(std::string_view text)
{
  if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
  {
    long long value = 0;
    const auto [end, error] =
        std::from_chars(text.data() + 2, text.data() + text.size(), value, 16);
    if (error != std::errc() || end != text.data() + text.size())
      return std::nullopt;
    return value;
  }
  const std::optional<long double> number = ParseNumber(text);
  if (!number || *number != std::floor(*number))
    return std::nullopt;
  return static_cast<long long>(*number);
}

std::string NumberText(long double number)
{
  if (std::isfinite(number) && number == std::floor(number) && std::fabs(number) < 1e18L)
    return std::to_string(static_cast<long long>(number));

  // The shortest text that reads back as the same value, as hi2txt shows it
  char buffer[64];
  const auto [end, error] =
      std::to_chars(buffer, buffer + sizeof(buffer), static_cast<double>(number));
  return error == std::errc() ? std::string(buffer, end) : std::string();
}

//! A decoded value: text, and the number it is where it is one
struct Value
{
  std::string text;
  std::optional<long double> number;

  static Value Number(long double n) { return {NumberText(n), n}; }
  static Value Of(std::string text)
  {
    std::optional<long double> number = ParseNumber(text);
    return {std::move(text), number};
  }
  bool IsEmpty() const { return text.empty(); }
};

int Compare(const Value& a, const Value& b)
{
  if (a.number && b.number)
    return *a.number < *b.number ? -1 : (*a.number > *b.number ? 1 : 0);
  return a.text.compare(b.text) < 0 ? -1 : (a.text == b.text ? 0 : 1);
}

bool Holds(const Value& a, std::string_view op, const Value& b)
{
  const int order = Compare(a, b);
  if (op.empty() || op == "==" || op == "=")
    return order == 0;
  if (op == "!=")
    return order != 0;
  if (op == "<")
    return order < 0;
  if (op == "<=")
    return order <= 0;
  if (op == ">")
    return order > 0;
  if (op == ">=")
    return order >= 0;
  throw Unsupported{"operator " + std::string(op)};
}

std::vector<std::string> Split(std::string_view text, char separator)
{
  std::vector<std::string> parts;
  size_t start = 0;
  while (start <= text.size())
  {
    const size_t end = std::min(text.find(separator, start), text.size());
    if (end > start)
      parts.emplace_back(text.substr(start, end - start));
    start = end + 1;
  }
  return parts;
}

/*!
 * \brief A big unsigned number, as the digits of an element, for a change of
 *        base without overflow on wide scores
 */
std::string ToBase(std::vector<int> digits, int fromBase, int toBase)
{
  static constexpr char symbols[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if (toBase < 2 || toBase > 256)
    throw Unsupported{"base " + std::to_string(toBase)};

  std::vector<int> out;
  while (!digits.empty())
  {
    std::vector<int> quotient;
    int remainder = 0;
    for (const int digit : digits)
    {
      const long long accumulated = static_cast<long long>(remainder) * fromBase + digit;
      const int q = static_cast<int>(accumulated / toBase);
      remainder = static_cast<int>(accumulated % toBase);
      if (!quotient.empty() || q != 0)
        quotient.emplace_back(q);
    }
    out.insert(out.begin(), remainder);
    digits = std::move(quotient);
  }
  if (out.empty())
    out.emplace_back(0);

  // Past 36 there are no letters left, so each digit is written as a number
  std::string text;
  for (const int digit : out)
    text += toBase <= 36 ? std::string(1, symbols[digit]) : std::to_string(digit);
  return text;
}

using Column = std::map<int, Value>;
using Values = std::map<std::string, Column, std::less<>>;

//! Where a value is being worked out
struct Row
{
  int value{0}; // where the row's values were read into
  int index{0}; // its place in the table, once sorted
  int unsortedIndex{0}; // its place before sorting
  int loopIndex{0};
};

class CDecoding
{
public:
  CDecoding(const XMLElement& root, HighScoreDetail detail) : m_root(root), m_detail(detail)
  {
    for (const XMLElement* child = root.FirstChildElement(); child != nullptr;
         child = child->NextSiblingElement())
    {
      const std::string_view name = child->Name();
      const std::string id(Attr(child, "id"));
      if (name == "format")
        m_formats[id] = child;
      else if (name == "charset")
        m_charsets[id] = child;
      else if (name == "bitmask")
        m_bitmasks[id] = child;
    }
  }

  bool Decode(const std::string& fileName, std::vector<uint8_t> data, HighScores& scores)
  {
    const XMLElement* structure = PickStructure(fileName, data.size());
    if (structure == nullptr)
      return false;

    if (const std::string_view swap = Attr(structure, "byte-swap"); !swap.empty())
      ByteSwap(data, Integer(swap));

    size_t position = 0;
    ReadChildren(*structure, data, position, nullptr);

    const XMLElement* output = PickOutput(Attr(structure, "output"));
    if (output == nullptr)
      return false;
    Render(*output, scores);
    return true;
  }

private:
  struct Loop
  {
    int start{0};
    int step{1};
    int count{0};
    int iteration{0};
    int lastIndex{0};
    bool hasLast{false};
    int Index() const { return start + iteration * step; }
    int ReverseIndex() const { return start + (count - 1 - iteration) * step; }
  };

  static int Integer(std::string_view text)
  {
    const std::optional<long long> value = ParseInteger(text);
    if (!value || *value < -MAX_COUNT * 1000LL || *value > MAX_COUNT * 1000LL)
      throw Unsupported{"number " + std::string(text)};
    return static_cast<int>(*value);
  }

  bool Shown(const XMLElement& element) const
  {
    const std::string_view display = Attr(&element, "display");
    if (display.empty() || display == "always")
      return true;
    if (display == "extra")
      return m_detail != HighScoreDetail::ALWAYS;
    if (display == "debug")
      return m_detail == HighScoreDetail::DEBUG;
    throw Unsupported{"display " + std::string(display)};
  }

  const XMLElement* PickStructure(const std::string& fileName, size_t size) const
  {
    // Layouts of the same size are told apart by the emulator's hiscore.dat,
    // which isn't to hand, so a file two of them fit is not read
    const XMLElement* picked = nullptr;
    for (const XMLElement* structure = m_root.FirstChildElement("structure"); structure != nullptr;
         structure = structure->NextSiblingElement("structure"))
    {
      const std::string_view file = Attr(structure, "file");
      const bool matches =
          file.empty() ? false
                       : (file.front() == '.' ? fileName.ends_with(file) : fileName == file);
      if (!matches)
        continue;

      // A layout may fit files of more than one size
      const XMLElement* check = structure->FirstChildElement("check");
      const XMLElement* expected = check != nullptr ? check->FirstChildElement("size") : nullptr;
      bool sized = expected == nullptr;
      for (; expected != nullptr; expected = expected->NextSiblingElement("size"))
      {
        if (ParseInteger(Text(expected)) == static_cast<long long>(size))
          sized = true;
      }
      if (!sized)
        continue;
      if (picked != nullptr)
        throw Unsupported{"layouts that only hiscore.dat tells apart"};
      picked = structure;
    }
    return picked;
  }

  const XMLElement* PickOutput(std::string_view id) const
  {
    const XMLElement* first = nullptr;
    for (const XMLElement* output = m_root.FirstChildElement("output"); output != nullptr;
         output = output->NextSiblingElement("output"))
    {
      const std::string_view outputId = Attr(output, "id");
      if (!id.empty() && outputId == id)
        return output;
      if (id.empty() && outputId.empty())
        return output;
      if (first == nullptr)
        first = output;
    }
    return id.empty() ? first : nullptr;
  }

  static void ByteSwap(std::vector<uint8_t>& bytes, int width)
  {
    if (width <= 1)
      return;
    for (size_t i = 0; i + width <= bytes.size(); i += width)
      std::reverse(bytes.begin() + i, bytes.begin() + i + width);
  }

  // --- reading the structure ---------------------------------------------

  static int ElementSize(const XMLElement& elt) { return Integer(Attr(&elt, "size")); }

  void ReadChildren(const XMLElement& parent,
                    const std::vector<uint8_t>& data,
                    size_t& position,
                    Loop* loop)
  {
    for (const XMLElement* child = parent.FirstChildElement(); child != nullptr;
         child = child->NextSiblingElement())
    {
      const std::string_view name = child->Name();
      if (name == "elt")
      {
        const int size = ElementSize(*child);
        if (size < 0)
          throw Unsupported{"an element size"};
        // A file that ends part way through an element still gives what it holds of it
        if (position < data.size())
          ReadElement(*child, data, position,
                      static_cast<int>(std::min<size_t>(size, data.size() - position)), loop);
        position += size;
      }
      else if (name == "loop")
      {
        if (loop != nullptr)
          throw Unsupported{"a loop in a loop"};
        ReadLoop(*child, data, position);
      }
      else if (name != "check")
        throw Unsupported{"structure child " + std::string(name)};
    }
  }

  void ReadLoop(const XMLElement& element, const std::vector<uint8_t>& data, size_t& position)
  {
    Loop loop;
    loop.count = Attr(&element, "count").empty() ? 1 : Integer(Attr(&element, "count"));
    if (const std::string_view start = Attr(&element, "start"); !start.empty())
      loop.start = Integer(start);
    if (const std::string_view step = Attr(&element, "step"); !step.empty())
      loop.step = Integer(step);
    if (loop.count < 0 || loop.count > MAX_COUNT)
      throw Unsupported{"loop count"};

    int passSize = 0;
    for (const XMLElement* elt = element.FirstChildElement(); elt != nullptr;
         elt = elt->NextSiblingElement())
    {
      if (std::string_view(elt->Name()) != "elt")
        throw Unsupported{"loop child " + std::string(elt->Name())};
      passSize += ElementSize(*elt);
    }

    // The first or last few bytes of a loop may be missing from the file, the
    // emulator only having saved part of it: those elements are left unread
    const std::string_view skipFirst = Attr(&element, "skip-first-bytes");
    const std::string_view skipLast = Attr(&element, "skip-last-bytes");
    const long long first = skipFirst.empty() ? 0 : Integer(skipFirst);
    const long long last = skipLast.empty() ? 0 : Integer(skipLast);
    const long long length = static_cast<long long>(passSize) * loop.count;
    if (first < 0 || last < 0 || first + last > length)
      throw Unsupported{"a loop's skipped bytes"};

    long long virtualPosition = 0;
    for (loop.iteration = 0; loop.iteration < loop.count; ++loop.iteration)
    {
      loop.hasLast = false;
      for (const XMLElement* elt = element.FirstChildElement(); elt != nullptr;
           elt = elt->NextSiblingElement())
      {
        const int size = ElementSize(*elt);
        const size_t at = position + static_cast<size_t>(virtualPosition - first);
        if (virtualPosition >= first && virtualPosition + size <= length - last && at < data.size())
          ReadElement(*elt, data, at, static_cast<int>(std::min<size_t>(size, data.size() - at)),
                      &loop);
        virtualPosition += size;
      }
    }
    position += static_cast<size_t>(length - first - last);
  }

  void ReadElement(
      const XMLElement& elt, const std::vector<uint8_t>& data, size_t offset, int size, Loop* loop)
  {
    std::vector<uint8_t> bytes(data.begin() + offset, data.begin() + offset + size);
    Value value = DecodeElement(elt, std::move(bytes));

    Row row;
    row.loopIndex = loop != nullptr ? loop->Index() : 0;
    row.value = row.index = row.unsortedIndex = row.loopIndex;
    if (const std::string_view format = Attr(&elt, "format"); !format.empty())
      value = ApplyFormats(format, std::move(value), row);

    const int index = TableIndex(elt, value, loop);
    const std::string id(Attr(&elt, "id"));
    if (id.empty())
      throw Unsupported{"an element without an id"};
    m_values[id][index] = std::move(value);
  }

  int TableIndex(const XMLElement& elt, const Value& value, Loop* loop)
  {
    const std::string_view how = Attr(&elt, "table-index");
    const int loopIndex = loop != nullptr ? loop->Index() : 0;
    int index = loopIndex;

    auto asIndex = [](const Value& v)
    {
      if (!v.number || *v.number != std::floor(*v.number) || std::fabs(*v.number) > MAX_COUNT)
        throw Unsupported{"an index that is not a number"};
      return static_cast<int>(*v.number);
    };

    if (how.empty() || how == "loop_index")
      index = loopIndex;
    else if (how == "loop_reverse_index")
      index = loop != nullptr ? loop->ReverseIndex() : 0;
    else if (how == "itself")
      index = asIndex(value);
    else if (how == "last")
      index = loop != nullptr && loop->hasLast ? loop->lastIndex : loopIndex;
    else if (const std::optional<long long> fixed = ParseInteger(how); fixed)
      index = static_cast<int>(*fixed);
    else if (const size_t colon = how.find(':'); colon != std::string_view::npos)
    {
      const std::string_view column = how.substr(0, colon);
      const std::string_view mode = how.substr(colon + 1);
      const auto found = m_values.find(column);
      if (found == m_values.end())
        throw Unsupported{"an index from an unknown element"};
      if (mode == "index_from_value")
      {
        const auto at = found->second.find(loopIndex);
        if (at == found->second.end())
          throw Unsupported{"an index from a missing value"};
        index = asIndex(at->second);
      }
      else if (mode == "value_from_index")
      {
        auto at = std::ranges::find_if(found->second, [&](const auto& entry)
                                       { return entry.second.number == loopIndex; });
        if (at == found->second.end())
          throw Unsupported{"an index from a missing value"};
        index = at->first;
      }
      else
        throw Unsupported{"table-index " + std::string(how)};
    }
    else
      throw Unsupported{"table-index " + std::string(how)};

    if (const std::string_view format = Attr(&elt, "table-index-format"); !format.empty())
    {
      Row row;
      row.loopIndex = loopIndex;
      index = asIndex(ApplyFormats(format, Value::Number(index), row));
    }

    if (loop != nullptr)
    {
      loop->lastIndex = index;
      loop->hasLast = true;
    }
    if (index < -MAX_COUNT || index > MAX_COUNT)
      throw Unsupported{"an index out of range"};
    return index;
  }

  // --- one element's bytes to a value -------------------------------------

  struct Decoding
  {
    std::string type;
    std::string endianness;
    std::string byteSkip;
    std::string byteTrim;
    std::string byteTrunc;
    std::string nibbleSkip;
    std::string nibbleTrim;
    std::string bitmask;
    std::string charset;
    int byteSwap{0};
    bool bitSwap{false};
    int base{0};
    int srcUnitSize{8};
    int dstUnitSize{1};
    int asciiOffset{0};
    int asciiStep{1};
  };

  static Decoding Properties(const XMLElement& elt)
  {
    Decoding d;
    d.type = Attr(&elt, "type");
    if (d.type.empty())
      d.type = "raw";

    const std::string_view profile = Attr(&elt, "decoding-profile");
    if (profile == "bcd")
    {
      d.nibbleSkip = "odd";
      d.base = 16;
    }
    else if (profile == "bcd-le")
    {
      d.endianness = "little_endian";
      d.nibbleSkip = "odd";
      d.base = 16;
    }
    else if (profile == "base-40" || profile == "base-32")
    {
      d.srcUnitSize = 16;
      d.base = profile == "base-40" ? 40 : 32;
      d.dstUnitSize = 3;
      d.asciiOffset = 64;
    }
    else if (!profile.empty())
      throw Unsupported{"decoding-profile " + std::string(profile)};

    auto text = [&elt](const char* name, std::string& out)
    {
      if (const std::string_view v = Attr(&elt, name); !v.empty())
        out = v;
    };
    auto number = [&elt](const char* name, int& out)
    {
      if (const std::string_view v = Attr(&elt, name); !v.empty())
        out = Integer(v);
    };
    text("endianness", d.endianness);
    text("byte-skip", d.byteSkip);
    text("byte-trim", d.byteTrim);
    text("byte-trunc", d.byteTrunc);
    text("nibble-skip", d.nibbleSkip);
    text("nibble-trim", d.nibbleTrim);
    text("bitmask", d.bitmask);
    text("charset", d.charset);
    number("byte-swap", d.byteSwap);
    number("base", d.base);
    number("src-unit-size", d.srcUnitSize);
    number("dst-unit-size", d.dstUnitSize);
    number("ascii-offset", d.asciiOffset);
    number("ascii-step", d.asciiStep);
    d.bitSwap = Attr(&elt, "bit-swap") == "yes";

    if (!Attr(&elt, "swap-skip-order").empty())
      throw Unsupported{"swap-skip-order"};
    if (d.srcUnitSize <= 0 || d.srcUnitSize > 32 || d.dstUnitSize <= 0 || d.dstUnitSize > 8 ||
        d.asciiStep == 0)
      throw Unsupported{"unit sizes"};
    return d;
  }

  static int ByteOf(std::string_view hex)
  {
    const std::optional<long long> value = ParseInteger(hex);
    if (!value || *value < 0 || *value > 0xff)
      throw Unsupported{"byte " + std::string(hex)};
    return static_cast<int>(*value);
  }

  Value DecodeElement(const XMLElement& elt, std::vector<uint8_t> bytes)
  {
    const Decoding d = Properties(elt);

    ByteSwap(bytes, d.byteSwap);

    if (d.bitSwap)
    {
      for (uint8_t& byte : bytes)
      {
        uint8_t reversed = 0;
        for (int bit = 0; bit < 8; ++bit)
          reversed |= ((byte >> bit) & 1) << (7 - bit);
        byte = reversed;
      }
    }

    if (!d.byteSkip.empty())
    {
      std::vector<uint8_t> kept;
      if (d.byteSkip == "odd" || d.byteSkip == "even")
      {
        // Counted from one, as the documentation's examples count them
        const size_t first = d.byteSkip == "odd" ? 1 : 0;
        for (size_t i = first; i < bytes.size(); i += 2)
          kept.emplace_back(bytes[i]);
      }
      else if (d.byteSkip.size() == 4 && d.byteSkip.find_first_not_of("01") == std::string::npos)
      {
        for (size_t i = 0; i < bytes.size(); ++i)
        {
          if (d.byteSkip[i % 4] == '1')
            kept.emplace_back(bytes[i]);
        }
      }
      else
      {
        const int skipped = ByteOf(d.byteSkip);
        for (const uint8_t byte : bytes)
        {
          if (byte != skipped)
            kept.emplace_back(byte);
        }
      }
      bytes = std::move(kept);
    }

    if (d.endianness == "little_endian")
      std::reverse(bytes.begin(), bytes.end());
    else if (!d.endianness.empty() && d.endianness != "big_endian")
      throw Unsupported{"endianness " + d.endianness};

    if (!d.byteTrunc.empty())
    {
      const int stop = ByteOf(d.byteTrunc);
      bytes.erase(std::ranges::find(bytes, stop), bytes.end());
    }
    if (!d.byteTrim.empty())
    {
      const int trimmed = ByteOf(d.byteTrim);
      bytes.erase(bytes.begin(),
                  std::ranges::find_if(bytes, [trimmed](uint8_t b) { return b != trimmed; }));
    }

    std::vector<int> nibbles;
    nibbles.reserve(bytes.size() * 2);
    for (const uint8_t byte : bytes)
    {
      nibbles.emplace_back(byte >> 4);
      nibbles.emplace_back(byte & 0x0f);
    }
    if (!d.nibbleSkip.empty())
    {
      if (d.nibbleSkip != "odd" && d.nibbleSkip != "even")
        throw Unsupported{"nibble-skip " + d.nibbleSkip};
      std::vector<int> kept;
      const size_t first = d.nibbleSkip == "odd" ? 1 : 0;
      for (size_t i = first; i < nibbles.size(); i += 2)
        kept.emplace_back(nibbles[i]);
      nibbles = std::move(kept);
    }
    if (!d.nibbleTrim.empty())
    {
      const std::optional<long long> trimmed = ParseInteger(d.nibbleTrim);
      if (!trimmed || *trimmed < 0 || *trimmed > 0xf)
        throw Unsupported{"nibble-trim " + d.nibbleTrim};
      nibbles.erase(nibbles.begin(),
                    std::ranges::find_if(nibbles, [&](int n) { return n != *trimmed; }));
    }

    std::vector<int> bits;
    bits.reserve(nibbles.size() * 4);
    for (const int nibble : nibbles)
    {
      for (int bit = 3; bit >= 0; --bit)
        bits.emplace_back((nibble >> bit) & 1);
    }

    std::vector<std::vector<int>> units;
    if (!d.bitmask.empty())
      units = Masked(d.bitmask, bits);

    if (d.type == "raw")
    {
      static constexpr char hex[] = "0123456789ABCDEF";
      std::string text;
      for (const int nibble : nibbles)
        text += hex[nibble];
      return Value{std::move(text), std::nullopt};
    }

    if (d.type == "int")
    {
      if (units.empty())
        units.emplace_back(bits);
      std::vector<int> all;
      for (const auto& unit : units)
        all.insert(all.end(), unit.begin(), unit.end());
      if (all.empty())
        return Value::Of("0");
      const std::string digits = ToBase(all, 2, d.base > 0 ? d.base : 10);
      return Value::Of(digits);
    }

    if (d.type == "text")
    {
      if (units.empty())
      {
        for (size_t i = 0; i + d.srcUnitSize <= bits.size(); i += d.srcUnitSize)
          units.emplace_back(bits.begin() + i, bits.begin() + i + d.srcUnitSize);
      }

      std::string text;
      for (const auto& unit : units)
      {
        long long code = 0;
        for (const int bit : unit)
          code = code * 2 + bit;

        if (d.base > 0)
        {
          // One unit holds several characters, as digits in the given base
          std::vector<long long> codes(static_cast<size_t>(d.dstUnitSize));
          for (int i = d.dstUnitSize - 1; i >= 0; --i)
          {
            codes[static_cast<size_t>(i)] = code % d.base;
            code /= d.base;
          }
          for (const long long c : codes)
            text += Character(d, c);
        }
        else
          text += Character(d, code);
      }
      // A name shorter than its space is padded out with nothing
      std::erase(text, '\0');
      return Value{std::move(text), std::nullopt};
    }

    throw Unsupported{"type " + d.type};
  }

  std::vector<std::vector<int>> Masked(const std::string& id, const std::vector<int>& bits) const
  {
    const auto found = m_bitmasks.find(id);
    if (found == m_bitmasks.end())
      throw Unsupported{"bitmask " + id};
    const std::string_view completion = Attr(found->second, "byte-completion");
    if (!completion.empty() && completion != "no" && completion != "yes")
      throw Unsupported{"bitmask byte-completion"};

    std::vector<std::vector<int>> units;
    for (const XMLElement* character = found->second->FirstChildElement("character");
         character != nullptr; character = character->NextSiblingElement("character"))
    {
      std::string mask(Attr(character, "mask"));
      std::erase_if(mask, [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; });
      if (mask.size() != bits.size())
        throw Unsupported{"bitmask size"};
      std::vector<int> unit;
      for (size_t i = 0; i < mask.size(); ++i)
      {
        if (mask[i] == '1')
          unit.emplace_back(bits[i]);
      }
      units.emplace_back(std::move(unit));
    }
    return units;
  }

  std::string Character(const Decoding& d, long long code) const
  {
    for (const std::string& name : Split(d.charset, ';'))
    {
      if (name.starts_with("CS_NUMBER"))
      {
        long long offset = 0;
        long long delta = 0;
        if (const size_t open = name.find('['); open != std::string::npos)
        {
          const size_t close = name.find(']', open);
          if (close == std::string::npos)
            throw Unsupported{"charset " + name};
          const std::vector<std::string> args =
              Split(std::string_view(name).substr(open + 1, close - open - 1), ',');
          if (args.empty() || args.size() > 2 || !ParseInteger(args[0]) ||
              (args.size() == 2 && !ParseInteger(args[1])))
            throw Unsupported{"charset " + name};
          offset = *ParseInteger(args[0]);
          if (args.size() == 2)
            delta = *ParseInteger(args[1]);
        }
        for (int digit = 0; digit < 10; ++digit)
        {
          if (code == '0' + offset + digit * (1 + delta))
            return std::string(1, static_cast<char>('0' + digit));
        }
        continue;
      }
      if (name.starts_with("CS_"))
        throw Unsupported{"charset " + name};
      const auto found = m_charsets.find(name);
      if (found == m_charsets.end())
        throw Unsupported{"charset " + name};
      for (const XMLElement* c = found->second->FirstChildElement("char"); c != nullptr;
           c = c->NextSiblingElement("char"))
      {
        const std::optional<long long> src = ParseInteger(Attr(c, "src"));
        if (src && *src == code)
          return std::string(Attr(c, "dst"));
      }
    }

    const long long ascii = code / d.asciiStep + d.asciiOffset;
    if (ascii < 0 || ascii > 0xff)
      throw Unsupported{"a character out of range"};
    if (ascii < 0x80)
      return std::string(1, static_cast<char>(ascii));
    // Latin-1, written as UTF-8
    return {static_cast<char>(0xc0 | (ascii >> 6)), static_cast<char>(0x80 | (ascii & 0x3f))};
  }

  // --- formats ------------------------------------------------------------

  Value ApplyFormats(std::string_view list, Value value, const Row& row)
  {
    for (const std::string& id : Split(list, ';'))
      value = ApplyFormat(id, std::move(value), row);
    return value;
  }

  static long double Number(const Value& value)
  {
    if (value.number)
      return *value.number;
    if (value.text.empty())
      return 0;
    // Raw bytes that read as decimal digits are worked with as the number
    if (const std::optional<long double> number = ParseNumber(value.text); number)
      return *number;
    throw Unsupported{"arithmetic on text"};
  }

  static long double Argument(std::string_view text)
  {
    const std::optional<long double> number = ParseNumber(text);
    if (!number)
      throw Unsupported{"argument " + std::string(text)};
    return *number;
  }

  Value ApplyFormat(const std::string& id, Value value, const Row& row)
  {
    if (const auto found = m_formats.find(id); found != m_formats.end())
      return ApplyNamedFormat(*found->second, std::move(value), row);
    return ApplyImplicit(id, std::move(value), row);
  }

  static Value Pad(Value value, bool left, size_t width, char padding)
  {
    std::string text = value.text;
    if (text.size() < width)
      text.insert(left ? text.begin() : text.end(), width - text.size(), padding);
    return Value::Of(std::move(text));
  }

  static Value Trim(Value value, bool left, bool right, char trimmed)
  {
    std::string text = value.text;
    if (left)
      text.erase(0, std::min(text.find_first_not_of(trimmed), text.size()));
    if (right)
    {
      const size_t last = text.find_last_not_of(trimmed);
      text.erase(last == std::string::npos ? 0 : last + 1);
    }
    return Value::Of(std::move(text));
  }

  Value ApplyImplicit(const std::string& id, Value value, const Row& row)
  {
    if (id.empty())
      return value;
    if (id == "0x" || id == "hexadecimal_string")
    {
      if (value.IsEmpty())
        return value;
      // A number is written in hexadecimal; raw bytes already are
      if (value.number && *value.number == std::floor(*value.number) && *value.number >= 0 &&
          *value.number < 1e18L)
      {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<long long>(*value.number));
        return Value{buffer, std::nullopt};
      }
      return Value{"0x" + value.text, std::nullopt};
    }
    if (id == "LC" || id == "Lowercase" || id == "UC" || id == "Uppercase")
    {
      std::string text = value.text;
      const bool lower = id == "LC" || id == "Lowercase";
      for (char& c : text)
        c = static_cast<char>(lower ? std::tolower(static_cast<unsigned char>(c))
                                    : std::toupper(static_cast<unsigned char>(c)));
      return Value::Of(std::move(text));
    }
    if (id == "Capitalize")
      return Capitalize(std::move(value));
    if (id == "Round" || id == "R")
      return Value::Number(std::round(Number(value)));
    if (id == "Trunc" || id == "T")
      return Value::Number(std::trunc(Number(value)));
    if (id == "LoopIndex")
      return Value::Number(row.loopIndex);
    if (id.starts_with("TrimL") && id.size() == 6)
      return Trim(std::move(value), true, false, id[5]);
    if (id.starts_with("TrimR") && id.size() == 6)
      return Trim(std::move(value), false, true, id[5]);
    if (id.starts_with("Trim") && id.size() == 5)
      return Trim(std::move(value), true, true, id[4]);
    if ((id.starts_with("PadL") || id.starts_with("PadR")) && id.size() >= 6)
    {
      const std::string width = id.substr(4, id.size() - 5);
      return Pad(std::move(value), id[3] == 'L', static_cast<size_t>(Integer(width)), id.back());
    }
    if (id.starts_with("Suffix"))
      return value.IsEmpty() ? value : Value::Of(value.text + id.substr(6));
    if (id.starts_with("Prefix"))
      return value.IsEmpty() ? value : Value::Of(id.substr(6) + value.text);

    const char op = id[0];
    const std::string_view argument = std::string_view(id).substr(1);
    switch (op)
    {
      case '+':
        return Value::Number(Number(value) + Argument(argument));
      case '-':
        return Value::Number(Number(value) - Argument(argument));
      case '*':
        return Value::Number(Number(value) * Argument(argument));
      case '/':
        return Value::Number(Number(value) / Divisor(argument));
      case 'd':
        return Value::Number(std::trunc(Number(value) / Divisor(argument)));
      case 'D':
        return Value::Number(std::round(Number(value) / Divisor(argument)));
      case '%':
        return Value::Number(std::fmod(Number(value), Divisor(argument)));
      case '>':
        return Shift(value, argument);
      default:
        break;
    }
    throw Unsupported{"format " + id};
  }

  static long double Divisor(std::string_view text)
  {
    const long double divisor = Argument(text);
    if (divisor == 0)
      throw Unsupported{"a division by zero"};
    return divisor;
  }

  static Value Shift(const Value& value, std::string_view argument)
  {
    const long long n = static_cast<long long>(Number(value));
    const int by = Integer(argument);
    if (by < -62 || by > 62)
      throw Unsupported{"shift"};
    return Value::Number(static_cast<long double>(by >= 0 ? n >> by : n << -by));
  }

  static Value Capitalize(Value value)
  {
    std::string text = value.text;
    bool start = true;
    for (char& c : text)
    {
      const auto u = static_cast<unsigned char>(c);
      c = static_cast<char>(start ? std::toupper(u) : std::tolower(u));
      start = std::isspace(u) != 0;
    }
    return Value::Of(std::move(text));
  }

  Value ApplyNamedFormat(const XMLElement& format, Value value, const Row& row)
  {
    if (Attr(&format, "apply-to") == "char")
    {
      std::string text;
      for (const char c : value.text)
        text += ApplyOperations(format, Value::Of(std::string(1, c)), row).text;
      value = Value::Of(std::move(text));
    }
    else if (Attr(&format, "apply-to").empty() || Attr(&format, "apply-to") == "value")
      value = ApplyOperations(format, std::move(value), row);
    else
      throw Unsupported{"apply-to"};

    if (const std::string_view formatter = Attr(&format, "formatter"); !formatter.empty())
      value = Formatter(formatter, value);
    return value;
  }

  static Value Formatter(std::string_view pattern, const Value& value)
  {
    // One conversion, as the descriptions use: %d, %s or %.2f with text around it
    const size_t percent = pattern.find('%');
    if (percent == std::string_view::npos)
      return Value::Of(std::string(pattern));
    const size_t conversion = pattern.find_first_of("dsf", percent + 1);
    if (conversion == std::string_view::npos ||
        pattern.find('%', conversion + 1) != std::string_view::npos)
      throw Unsupported{"formatter " + std::string(pattern)};

    const std::string spec(pattern.substr(percent, conversion - percent + 1));
    char buffer[128];
    int written = 0;
    if (pattern[conversion] == 'd')
      written =
          std::snprintf(buffer, sizeof(buffer), (spec.substr(0, spec.size() - 1) + "lld").c_str(),
                        static_cast<long long>(std::llround(Number(value))));
    else if (pattern[conversion] == 'f')
      written = std::snprintf(buffer, sizeof(buffer),
                              (spec.substr(0, spec.size() - 1) + "Lf").c_str(), Number(value));
    else
      written = std::snprintf(buffer, sizeof(buffer), spec.c_str(), value.text.c_str());
    if (written < 0 || static_cast<size_t>(written) >= sizeof(buffer))
      throw Unsupported{"formatter " + std::string(pattern)};

    return Value::Of(std::string(pattern.substr(0, percent)) + buffer +
                     std::string(pattern.substr(conversion + 1)));
  }

  Value ApplyOperations(const XMLElement& format, Value value, const Row& row)
  {
    const bool inputAsSubcolumns = Attr(&format, "input-as-subcolumns-input") == "yes" ||
                                   Attr(&format, "input-as-subcolumns-input") == "true";
    bool casesDone = false;
    for (const XMLElement* op = format.FirstChildElement(); op != nullptr;
         op = op->NextSiblingElement())
    {
      const std::string_view name = op->Name();
      const std::string text = Text(op);
      if (name == "add")
        value = Value::Number(Number(value) + Argument(text));
      else if (name == "substract")
        value = Value::Number(Number(value) - Argument(text));
      else if (name == "multiply")
        value = Value::Number(Number(value) * Argument(text));
      else if (name == "divide")
        value = Value::Number(Number(value) / Divisor(text));
      else if (name == "divide_trunc")
        value = Value::Number(std::trunc(Number(value) / Divisor(text)));
      else if (name == "divide_round")
        value = Value::Number(std::round(Number(value) / Divisor(text)));
      else if (name == "remainder")
        value = Value::Number(std::fmod(Number(value), Divisor(text)));
      else if (name == "shift")
        value = Shift(value, text);
      else if (name == "trunc")
        value = Value::Number(std::trunc(Number(value)));
      else if (name == "round")
        value = Value::Number(std::round(Number(value)));
      else if (name == "lowercase")
        value = ApplyImplicit("LC", std::move(value), row);
      else if (name == "uppercase")
        value = ApplyImplicit("UC", std::move(value), row);
      else if (name == "capitalize")
        value = Capitalize(std::move(value));
      else if (name == "loopindex")
        value = Value::Number(row.loopIndex);
      else if (name == "prefix" || name == "suffix")
      {
        const std::string_view empty = Attr(op, "empty");
        const bool isEmpty = empty.empty() ? value.IsEmpty() : value.text == empty;
        if (isEmpty)
        {
          if (Attr(op, "consume") == "yes")
            value = Value{};
        }
        else
          value = Value::Of(name == "prefix" ? text + value.text : value.text + text);
      }
      else if (name == "pad")
      {
        const std::string_view max = Attr(op, "max");
        if (max.empty() || text.size() > 1)
          throw Unsupported{"pad"};
        value = Pad(std::move(value), Attr(op, "direction") != "right",
                    static_cast<size_t>(Integer(max)), text.empty() ? ' ' : text[0]);
      }
      else if (name == "trim")
      {
        if (text.size() > 1)
          throw Unsupported{"trim"};
        const std::string_view direction = Attr(op, "direction");
        value = Trim(std::move(value), direction != "right", direction != "left",
                     text.empty() ? ' ' : text[0]);
      }
      else if (name == "replace")
      {
        std::string replaced = value.text;
        const std::string src(Attr(op, "src"));
        const std::string dst(Attr(op, "dst"));
        if (!src.empty())
        {
          for (size_t at = replaced.find(src); at != std::string::npos;
               at = replaced.find(src, at + dst.size()))
            replaced.replace(at, src.size(), dst);
        }
        value = Value::Of(std::move(replaced));
      }
      else if (name == "sum" || name == "min" || name == "max" || name == "concat")
        value = Combine(*op, name, value, row, inputAsSubcolumns);
      else if (name == "case")
      {
        if (!casesDone)
        {
          value = Switch(format, std::move(value), row);
          casesDone = true;
        }
      }
      else
        throw Unsupported{"format operation " + std::string(name)};
    }
    return value;
  }

  Value Switch(const XMLElement& format, Value value, const Row& row)
  {
    const XMLElement* fallback = nullptr;
    for (const XMLElement* c = format.FirstChildElement("case"); c != nullptr;
         c = c->NextSiblingElement("case"))
    {
      if (Attr(c, "default") == "yes" || Attr(c, "default") == "true")
        fallback = c;

      Value compared = value;
      if (const std::string_view f = Attr(c, "operator-format"); !f.empty())
        compared = ApplyFormats(f, compared, row);

      // A code written in hexadecimal matches a number, or a character by its code
      Value src = Value::Of(std::string(Attr(c, "src")));
      if (src.text.starts_with("0x") || src.text.starts_with("0X"))
      {
        if (const std::optional<long long> code = ParseInteger(src.text); code)
        {
          src.number = *code;
          if (!compared.number && compared.text.size() == 1)
            compared.number = static_cast<unsigned char>(compared.text[0]);
        }
      }
      if (Holds(compared, Attr(c, "operator"), src))
        return CaseResult(*c, std::move(value), row);
    }
    if (fallback != nullptr)
      return CaseResult(*fallback, std::move(value), row);
    return value;
  }

  Value CaseResult(const XMLElement& c, Value value, const Row& row)
  {
    if (c.Attribute("dst") != nullptr)
      return Value::Of(std::string(Attr(&c, "dst")));
    if (const std::string_view f = Attr(&c, "format"); !f.empty())
      return ApplyFormats(f, std::move(value), row);
    return value;
  }

  Value Combine(const XMLElement& op,
                std::string_view name,
                const Value& input,
                const Row& row,
                bool inputAsSubcolumns)
  {
    std::vector<Value> parts;
    for (const XMLElement* part = op.FirstChildElement(); part != nullptr;
         part = part->NextSiblingElement())
    {
      const std::string_view kind = part->Name();
      if (kind == "txt")
        parts.emplace_back(Value::Of(Text(part)));
      else if (kind == "column" || kind == "field")
        parts.emplace_back(Cell(*part, row, inputAsSubcolumns ? &input : nullptr));
      else
        throw Unsupported{"combined " + std::string(kind)};
    }

    if (name == "concat")
    {
      std::string text;
      for (const Value& part : parts)
        text += part.text;
      return Value::Of(std::move(text));
    }
    if (parts.empty())
      return Value::Number(0);

    long double result = Number(parts.front());
    for (size_t i = 1; i < parts.size(); ++i)
    {
      const long double n = Number(parts[i]);
      if (name == "sum")
        result += n;
      else if (name == "min")
        result = std::min(result, n);
      else
        result = std::max(result, n);
    }
    return Value::Number(result);
  }

  // --- output -------------------------------------------------------------

  /*!
   * \brief A column's or field's value in a row: its element, the row's
   *        index, or the text it holds, then its formats
   */
  Value Cell(const XMLElement& column, const Row& row, const Value* input)
  {
    const std::string_view src = Attr(&column, "src");
    const std::string_view id = Attr(&column, "id");
    const std::string_view source = !src.empty() ? src : id;

    Value value;
    if (source == "index")
      value = Value::Number(row.index);
    else if (source == "unsorted_index")
      value = Value::Number(row.unsortedIndex);
    else if (!source.empty())
    {
      if (const auto found = m_values.find(source); found != m_values.end())
      {
        if (const auto at = found->second.find(row.value); at != found->second.end())
          value = at->second;
      }
    }
    else if (const char* text = column.GetText(); text != nullptr)
      value = Value::Of(text);
    else if (input != nullptr)
      value = *input;

    if (const std::string_view format = Attr(&column, "format"); !format.empty())
      value = ApplyFormats(format, std::move(value), row);
    return value;
  }

  void Render(const XMLElement& output, HighScores& scores)
  {
    for (const XMLElement* part = output.FirstChildElement(); part != nullptr;
         part = part->NextSiblingElement())
    {
      const std::string_view name = part->Name();
      if (!Shown(*part))
        continue;
      if (name == "field")
      {
        // A field of an element the file didn't hold is left out
        const std::string_view fieldSource =
            !Attr(part, "src").empty() ? Attr(part, "src") : Attr(part, "id");
        if (part->GetText() == nullptr && fieldSource != "index" &&
            (m_values.find(fieldSource) == m_values.end() ||
             m_values.find(fieldSource)->second.empty()))
          continue;

        HighScores::Part field;
        field.field.id = !Attr(part, "id").empty() ? Attr(part, "id") : Attr(part, "src");
        Row row;
        if (const std::string_view source =
                !Attr(part, "src").empty() ? Attr(part, "src") : Attr(part, "id");
            !source.empty())
        {
          // A field is one value: the element's own, wherever it was put
          if (const auto found = m_values.find(source);
              found != m_values.end() && !found->second.empty())
            row.value = found->second.begin()->first;
        }
        field.field.value = Cell(*part, row, nullptr).text;
        scores.parts.emplace_back(std::move(field));
      }
      else if (name == "table")
      {
        HighScores::Part table = Table(*part);
        if (!table.table.rows.empty())
          scores.parts.emplace_back(std::move(table));
      }
      else
        throw Unsupported{"output " + std::string(name)};
    }
  }

  void Sources(const XMLElement& column,
               std::set<std::string, std::less<>>& sources,
               int depth) const
  {
    if (depth > 8)
      throw Unsupported{"formats that refer to each other"};
    const std::string_view source =
        !Attr(&column, "src").empty() ? Attr(&column, "src") : Attr(&column, "id");
    if (!source.empty())
      sources.emplace(source);
    for (const std::string& id : Split(Attr(&column, "format"), ';'))
    {
      const auto found = m_formats.find(id);
      if (found == m_formats.end())
        continue;
      for (const XMLElement* op = found->second->FirstChildElement(); op != nullptr;
           op = op->NextSiblingElement())
      {
        for (const XMLElement* part = op->FirstChildElement(); part != nullptr;
             part = part->NextSiblingElement())
        {
          if (std::string_view(part->Name()) == "column" ||
              std::string_view(part->Name()) == "field")
            Sources(*part, sources, depth + 1);
        }
      }
    }
  }

  HighScores::Part Table(const XMLElement& table)
  {
    std::vector<const XMLElement*> columns;
    std::vector<const XMLElement*> shown;
    // Older descriptions list a table's columns as fields
    for (const XMLElement* column = table.FirstChildElement(); column != nullptr;
         column = column->NextSiblingElement())
    {
      const std::string_view kind = column->Name();
      if (kind != "column" && kind != "field")
        throw Unsupported{"table child " + std::string(kind)};
      columns.emplace_back(column);
      if (Shown(*column))
        shown.emplace_back(column);
    }

    // As many rows as the elements the columns read have values for, some
    // read by their formats rather than themselves
    std::set<std::string, std::less<>> sources;
    for (const XMLElement* column : columns)
      Sources(*column, sources, 0);
    std::set<int> indexes;
    for (const std::string& source : sources)
    {
      if (const auto found = m_values.find(source); found != m_values.end())
      {
        for (const auto& [index, value] : found->second)
          indexes.insert(index);
      }
    }
    std::vector<int> rows;
    if (!indexes.empty())
    {
      for (int i = std::min(0, *indexes.begin()); i <= *indexes.rbegin(); ++i)
        rows.emplace_back(i);
    }

    // The index a rank is counted from is the row's place once sorted
    std::map<int, int> place;
    for (const int index : rows)
      place[index] = index;
    if (const std::string_view sortBy = Attr(&table, "sort"); !sortBy.empty())
    {
      const XMLElement* key = nullptr;
      for (const XMLElement* column : columns)
      {
        if (Attr(column, "id") == sortBy)
          key = column;
      }
      if (key == nullptr)
        throw Unsupported{"sort column"};
      const std::string_view sortFormat = Attr(&table, "sort-format");
      std::map<int, Value> keys;
      for (const int index : rows)
      {
        Row row{index, index, index, index};
        Value value = Cell(*key, row, nullptr);
        if (!sortFormat.empty())
          value = ApplyFormats(sortFormat, std::move(value), row);
        keys[index] = std::move(value);
      }
      const bool descending = Attr(&table, "sort-order") == "desc";
      std::ranges::stable_sort(rows,
                               [&](int a, int b)
                               {
                                 const int order = Compare(keys[a], keys[b]);
                                 return descending ? order > 0 : order < 0;
                               });
      for (size_t position = 0; position < rows.size(); ++position)
        place[rows[position]] =
            (rows.empty() ? 0 : std::min(0, *indexes.begin())) + static_cast<int>(position);
    }
    auto rowAt = [&place](int index) { return Row{index, place[index], index, index}; };

    // Lines a table leaves out, such as empty places
    if (const std::string_view ignore = Attr(&table, "line-ignore"); !ignore.empty())
    {
      const size_t colon = ignore.rfind(':');
      const std::string_view columnId = ignore.substr(0, colon);
      const Value limit = Value::Of(
          colon == std::string_view::npos ? std::string() : std::string(ignore.substr(colon + 1)));
      const std::string_view op = Attr(&table, "line-ignore-operator");
      const XMLElement* tested = nullptr;
      for (const XMLElement* column : columns)
      {
        if (Attr(column, "id") == columnId)
          tested = column;
      }
      if (tested == nullptr)
        throw Unsupported{"line-ignore column"};
      std::erase_if(rows, [&](int index)
                    { return Holds(Cell(*tested, rowAt(index), nullptr), op, limit); });
    }

    if (const std::string_view max = Attr(&table, "lines-max"); !max.empty())
    {
      const int lines = Integer(max);
      if (lines >= 0 && static_cast<size_t>(lines) < rows.size())
        rows.resize(static_cast<size_t>(lines));
    }

    HighScores::Part part;
    part.isTable = true;
    part.table.id = Attr(&table, "id");
    for (const XMLElement* column : shown)
      part.table.columns.emplace_back(!Attr(column, "id").empty() ? Attr(column, "id")
                                                                  : Attr(column, "src"));

    for (const int index : rows)
    {
      std::vector<std::string> cells;
      for (const XMLElement* column : shown)
        cells.emplace_back(Cell(*column, rowAt(index), nullptr).text);
      part.table.rows.emplace_back(std::move(cells));
    }
    return part;
  }

  const XMLElement& m_root;
  const HighScoreDetail m_detail;
  std::map<std::string, const XMLElement*, std::less<>> m_formats;
  std::map<std::string, const XMLElement*, std::less<>> m_charsets;
  std::map<std::string, const XMLElement*, std::less<>> m_bitmasks;
  Values m_values;
};
} // namespace

CHighScoreDecoder::CHighScoreDecoder() = default;

void CHighScoreDecoder::SetEntities(const std::string& dtd)
{
  m_entities.clear();
  size_t at = 0;
  while ((at = dtd.find("<!ENTITY", at)) != std::string::npos)
  {
    at += 8;
    const size_t nameStart = dtd.find_first_not_of(" \t\r\n", at);
    if (nameStart == std::string::npos)
      break;
    const size_t nameEnd = dtd.find_first_of(" \t\r\n", nameStart);
    const size_t open = nameEnd != std::string::npos ? dtd.find('"', nameEnd) : std::string::npos;
    const size_t close = open != std::string::npos ? dtd.find('"', open + 1) : std::string::npos;
    if (close == std::string::npos)
      break;
    m_entities.emplace_back("&" + dtd.substr(nameStart, nameEnd - nameStart) + ";",
                            dtd.substr(open + 1, close - open - 1));
    at = close + 1;
  }
}

std::string CHighScoreDecoder::Expand(const std::string& xml) const
{
  if (m_entities.empty() || xml.find('&') == std::string::npos)
    return xml;

  std::string out;
  out.reserve(xml.size());
  for (size_t i = 0; i < xml.size(); ++i)
  {
    if (xml[i] == '&')
    {
      const size_t end = xml.find(';', i);
      if (end != std::string::npos && end - i < 64)
      {
        const std::string_view name(xml.data() + i, end - i + 1);
        const auto found = std::ranges::find_if(m_entities, [&](const auto& entity)
                                                { return entity.first == name; });
        if (found != m_entities.end())
        {
          out += found->second;
          i = end;
          continue;
        }
      }
    }
    out += xml[i];
  }
  return out;
}

CHighScoreDecoder::~CHighScoreDecoder() = default;

bool CHighScoreDecoder::Load(const std::string& xml, const DescriptionLoader& loader)
{
  std::string text = Expand(xml);
  for (int depth = 0; depth <= MAX_SAMEAS_DEPTH; ++depth)
  {
    auto document = std::make_unique<tinyxml2::XMLDocument>();
    if (document->Parse(text.c_str(), text.size()) != tinyxml2::XML_SUCCESS)
      return false;

    const XMLElement* root = document->FirstChildElement("hi2txt");
    if (root == nullptr)
      return false;

    // A clone is described as the same as its parent
    const XMLElement* sameAs = root->FirstChildElement("sameas");
    if (sameAs == nullptr)
    {
      m_document = std::move(document);
      return true;
    }
    const std::string_view parent = Attr(sameAs, "id");
    if (parent.empty() || !loader)
      return false;
    text = Expand(loader(std::string(parent)));
    if (text.empty())
      return false;
  }
  return false;
}

bool CHighScoreDecoder::Decode(const std::string& fileName,
                               const std::vector<uint8_t>& data,
                               HighScoreDetail detail,
                               HighScores& scores) const
{
  scores.parts.clear();
  if (!m_document || data.empty() || data.size() > MAX_DATA_SIZE)
    return false;

  const XMLElement* root = m_document->FirstChildElement("hi2txt");
  if (root == nullptr)
    return false;

  try
  {
    CDecoding decoding(*root, detail);
    return decoding.Decode(fileName, data, scores);
  }
  catch (const Unsupported& unsupported)
  {
    CLog::Log(LOGDEBUG, "GAME: Can't read the high scores in {}: {}", fileName, unsupported.what);
    scores.parts.clear();
    return false;
  }
}
