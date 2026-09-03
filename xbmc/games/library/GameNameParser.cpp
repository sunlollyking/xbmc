/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameNameParser.h"

#include "utils/RegExp.h"
#include "utils/StringUtils.h"

#include <array>
#include <cctype>
#include <utility>

using namespace KODI;
using namespace GAME;

namespace
{
constexpr std::array<std::pair<std::string_view, std::string_view>, 41> regionNames{{
    {"usa", "USA"},
    {"us", "USA"},
    {"u", "USA"},
    {"canada", "Canada"},
    {"europe", "Europe"},
    {"eu", "Europe"},
    {"e", "Europe"},
    {"japan", "Japan"},
    {"jp", "Japan"},
    {"j", "Japan"},
    {"world", "World"},
    {"w", "World"},
    {"asia", "Asia"},
    {"australia", "Australia"},
    {"brazil", "Brazil"},
    {"china", "China"},
    {"france", "France"},
    {"germany", "Germany"},
    {"italy", "Italy"},
    {"spain", "Spain"},
    {"netherlands", "Netherlands"},
    {"sweden", "Sweden"},
    {"korea", "Korea"},
    {"taiwan", "Taiwan"},
    {"hong kong", "Hong Kong"},
    {"uk", "United Kingdom"},
    {"united kingdom", "United Kingdom"},
    {"russia", "Russia"},
    {"poland", "Poland"},
    {"finland", "Finland"},
    {"denmark", "Denmark"},
    {"norway", "Norway"},
    {"portugal", "Portugal"},
    {"greece", "Greece"},
    {"israel", "Israel"},
    {"india", "India"},
    {"mexico", "Mexico"},
    {"argentina", "Argentina"},
    {"scandinavia", "Scandinavia"},
    {"latin america", "Latin America"},
    {"unknown", "Unknown"},
}};

constexpr std::array<std::string_view, 40> languageCodes{
    "en", "ja", "fr", "de", "es", "it", "nl", "pt", "sv", "no", "da", "fi", "zh", "ko",
    "pl", "ru", "cs", "hu", "el", "tr", "ar", "he", "ca", "th", "hr", "ro", "bg", "uk",
    "sk", "sl", "lt", "lv", "et", "ga", "eu", "gl", "id", "ms", "vi", "fa",
};

constexpr std::array<std::string_view, 15> noiseTags{
    "gb compatible",     "sgb enhanced",  "enhancement chip",   "rumble version",
    "virtual console",   "nintendo power", "np",                "st",
    "mb",                "wii virtual console", "3ds virtual console", "wii u virtual console",
    "switch online",     "lock-on",       "en,ja",
};

constexpr std::array<std::string_view, 7> articles{"the", "a", "an", "le", "la", "les", "der"};

std::string Lower(std::string_view s)
{
  return StringUtils::ToLower(std::string(s));
}

std::vector<std::string> SplitTrim(const std::string& s, char sep)
{
  std::vector<std::string> parts = StringUtils::Split(s, sep);
  for (std::string& p : parts)
    StringUtils::Trim(p);
  std::erase_if(parts, [](const std::string& p) { return p.empty(); });
  return parts;
}

bool AllRegions(const std::vector<std::string>& parts, std::vector<std::string>& codes)
{
  codes.clear();
  for (const std::string& p : parts)
  {
    const std::string name = CGameNameParser::RegionName(Lower(p));
    if (name.empty())
      return false;
    codes.emplace_back(name);
  }
  return !codes.empty();
}

bool AllLanguages(const std::vector<std::string>& parts, std::vector<std::string>& codes)
{
  codes.clear();
  for (const std::string& p : parts)
  {
    const std::string code = Lower(p);
    if (code.size() != 2)
      return false;
    bool known = false;
    for (std::string_view lang : languageCodes)
    {
      if (lang == code)
      {
        known = true;
        break;
      }
    }
    if (!known)
      return false;
    codes.emplace_back(code);
  }
  return !codes.empty();
}

bool Matches(CRegExp& re, const std::string& text)
{
  return re.RegFind(text) >= 0;
}

struct Patterns
{
  CRegExp revision{true};
  CRegExp disc{true};
  CRegExp tosecYear{true};
  CRegExp beta{true};
  CRegExp proto{true};
  CRegExp sample{true};
  CRegExp demo{true};
  CRegExp alpha{true};
  CRegExp debug{true};
  CRegExp program{true};
  CRegExp alt{true};
  CRegExp extension{true};
  CRegExp leadingNumber{true};

  Patterns()
  {
    revision.RegComp("^(rev|revision|version|v)\\.? ?([0-9][0-9a-z.]*|[a-z])$");
    disc.RegComp("^(disc|disk|side|tape|cd|cart|part) ?([0-9a-z]+)( of ([0-9]+))?$");
    tosecYear.RegComp("^(19|20)([0-9x?]{2})(-[0-9]{2}(-[0-9]{2})?)?$");
    beta.RegComp("^beta( ?[0-9]+)?$");
    proto.RegComp("^proto(type)?( ?[0-9]+)?$");
    sample.RegComp("^sample$");
    demo.RegComp("^(demo|kiosk|preview|trial|taikenban)");
    alpha.RegComp("^alpha$");
    debug.RegComp("^debug( version)?$");
    program.RegComp("^(check |sample |test )?program$");
    alt.RegComp("^alt( ?[0-9]+)?$");
    extension.RegComp("\\.[a-z0-9]{1,4}$");
    leadingNumber.RegComp("^0[0-9]{2,4} +-? *");
  }
};

Patterns& GetPatterns()
{
  static Patterns patterns;
  return patterns;
}
} // namespace

std::string CGameNameParser::RegionName(std::string_view region)
{
  for (const auto& [alias, name] : regionNames)
  {
    if (alias == region)
      return std::string(name);
  }
  return "";
}

std::string CGameNameParser::DisplayTitle(std::string_view title)
{
  // "Legend of Zelda, The - A Link to the Past" -> article first, subtitle kept
  const std::string text(title);
  const size_t subtitle = text.find(" - ");
  const std::string head = subtitle == std::string::npos ? text : text.substr(0, subtitle);
  const std::string tail = subtitle == std::string::npos ? "" : text.substr(subtitle);

  const size_t comma = head.rfind(", ");
  if (comma == std::string::npos)
    return text;

  const std::string article = head.substr(comma + 2);
  const std::string lower = Lower(article);
  for (std::string_view a : articles)
  {
    if (a == lower)
      return article + " " + head.substr(0, comma) + tail;
  }
  return text;
}

ParsedGameName CGameNameParser::Parse(std::string_view fileName)
{
  Patterns& re = GetPatterns();
  ParsedGameName out;

  std::string name(fileName);
  if (Matches(re.extension, Lower(name)))
    name.erase(name.size() - re.extension.GetFindLen());
  if (Matches(re.leadingNumber, name))
    name.erase(0, re.leadingNumber.GetFindLen());

  // Bracket tags: GoodTools and TOSEC dump flags
  std::string rest;
  size_t pos = 0;
  while (pos < name.size())
  {
    const size_t open = name.find('[', pos);
    if (open == std::string::npos)
    {
      rest += name.substr(pos);
      break;
    }
    const size_t close = name.find(']', open);
    if (close == std::string::npos)
    {
      rest += name.substr(pos);
      break;
    }
    rest += name.substr(pos, open - pos);

    std::string tag = name.substr(open + 1, close - open - 1);
    StringUtils::Trim(tag);
    const std::string tl = Lower(tag);

    if (tl == "!")
      out.verified = true;
    else if (tl.starts_with("t+") || tl.starts_with("t-"))
      out.translation = tl.substr(2, 2);
    else if (!tl.empty() && tl[0] == 'b' && tl.find_first_not_of("0123456789", 1) == std::string::npos)
      out.bad = true;
    else if (!tl.empty() && tl[0] == 'a' && tl.find_first_not_of("0123456789", 1) == std::string::npos)
      out.alternate = true;
    else if (!tl.empty() && tl[0] == 'h' && tl.find_first_not_of("0123456789", 1) == std::string::npos)
      out.hack = true;
    else if (!tl.empty() && tl[0] == 'p' && tl.find_first_not_of("0123456789", 1) == std::string::npos)
      out.licence = Licence::PIRATE;
    else if (tl == "bios")
      out.status = ReleaseStatus::PROGRAM;
    else
      out.unknownTags.emplace_back(tag);

    pos = close + 1;
  }
  name = rest;

  // Parenthesised tags: No-Intro, Redump and TOSEC attributes
  rest.clear();
  pos = 0;
  bool tosec = false;
  while (pos < name.size())
  {
    const size_t open = name.find('(', pos);
    if (open == std::string::npos)
    {
      rest += name.substr(pos);
      break;
    }
    const size_t close = name.find(')', open);
    if (close == std::string::npos)
    {
      rest += name.substr(pos);
      break;
    }
    rest += name.substr(pos, open - pos);
    pos = close + 1;

    std::string tag = name.substr(open + 1, close - open - 1);
    StringUtils::Trim(tag);
    if (tag.empty())
      continue;
    const std::string tl = Lower(tag);
    const std::vector<std::string> parts = SplitTrim(tag, ',');

    std::vector<std::string> codes;
    if (AllRegions(parts, codes))
    {
      out.regions.insert(out.regions.end(), codes.begin(), codes.end());
      continue;
    }
    if (AllLanguages(parts, codes))
    {
      out.languages.insert(out.languages.end(), codes.begin(), codes.end());
      continue;
    }
    if (Matches(re.revision, tl))
    {
      out.revision = tag;
      continue;
    }
    if (Matches(re.disc, tl))
    {
      const std::string number = re.disc.GetMatch(2);
      const std::string total = re.disc.GetMatch(4);
      out.disc = std::isdigit(static_cast<unsigned char>(number[0]))
                     ? std::stoi(number)
                     : static_cast<int>(std::tolower(static_cast<unsigned char>(number[0])) - 'a' + 1);
      out.discs = total.empty() ? 0 : std::stoi(total);
      continue;
    }
    if (Matches(re.beta, tl))
    {
      out.status = ReleaseStatus::BETA;
      continue;
    }
    if (Matches(re.proto, tl))
    {
      out.status = ReleaseStatus::PROTOTYPE;
      continue;
    }
    if (Matches(re.sample, tl))
    {
      out.status = ReleaseStatus::SAMPLE;
      continue;
    }
    if (Matches(re.demo, tl))
    {
      out.status = ReleaseStatus::DEMO;
      continue;
    }
    if (Matches(re.alpha, tl))
    {
      out.status = ReleaseStatus::ALPHA;
      continue;
    }
    if (Matches(re.debug, tl))
    {
      out.status = ReleaseStatus::DEBUG;
      continue;
    }
    if (Matches(re.program, tl))
    {
      out.status = ReleaseStatus::PROGRAM;
      continue;
    }
    if (tl == "unl" || tl == "unlicensed")
    {
      out.licence = Licence::UNLICENSED;
      continue;
    }
    if (tl == "pirate")
    {
      out.licence = Licence::PIRATE;
      continue;
    }
    if (tl == "aftermarket")
    {
      out.licence = Licence::AFTERMARKET;
      continue;
    }
    if (tl == "homebrew")
    {
      out.licence = Licence::HOMEBREW;
      continue;
    }
    if (Matches(re.alt, tl))
    {
      out.alternate = true;
      continue;
    }
    if (tl == "hack" || tl == "hacked")
    {
      out.hack = true;
      continue;
    }
    if (Matches(re.tosecYear, tl))
    {
      tosec = true;
      if (std::isdigit(static_cast<unsigned char>(tl[2])) &&
          std::isdigit(static_cast<unsigned char>(tl[3])))
        out.year = std::stoi(tl.substr(0, 4));
      continue;
    }
    bool noise = false;
    for (std::string_view n : noiseTags)
    {
      if (n == tl)
      {
        noise = true;
        break;
      }
    }
    if (noise)
      continue;
    // TOSEC: the tag after the year names the publisher
    if (tosec && out.publisher.empty() && !std::isdigit(static_cast<unsigned char>(tag[0])))
    {
      out.publisher = (tag == "-") ? "" : tag;
      continue;
    }
    out.unknownTags.emplace_back(tag);
  }

  std::string title = rest;
  StringUtils::Replace(title, "  ", " ");
  StringUtils::Trim(title, " -_");
  out.title = title;
  out.displayTitle = DisplayTitle(title);
  return out;
}
