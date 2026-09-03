# Game scraper protocol, version 1

A game scraper is a Python add-on with the extension point
`xbmc.metadata.scraper.games`. Kodi drives it exactly as it drives the movie
scrapers: by opening `plugin://<addon id>/?action=...` URLs and reading the
list items the script adds. Every result carries its payload as **JSON in a
list-item property**, so the protocol can grow without touching the Python
API.

```xml
<addon id="metadata.games.libretro" name="libretro-database" version="1.0.0" provider-name="...">
  <requires>
    <import addon="xbmc.python" version="3.0.0"/>
    <import addon="xbmc.metadata" version="2.1.0"/>
  </requires>
  <extension point="xbmc.metadata.scraper.games" library="scraper.py"/>
  <extension point="xbmc.addon.metadata">...</extension>
</addon>
```

Every URL carries `pathSettings=<json>` (the add-on's settings as Kodi's
scraper framework serialises them; may be `{}`) and `protocol=1`.

## `action=find`

Kodi asks which catalogue entries a file might be.

Query parameters (absent when unknown):

| key | meaning |
|---|---|
| `title` | the title parsed from the file name, tags stripped, article restored ("The Legend of Zelda") |
| `filename` | the file's base name as on disk, with extension |
| `platform` | the platform slug, e.g. `megadrive` (see platforms.xml) |
| `platformids` | JSON object of provider ids for that platform, e.g. `{"screenscraper":"1","libretro":"Sega - Mega Drive - Genesis","retroachievements":"1","esde":"megadrive"}` |
| `crc32` | 8 hex chars, lower case, of the ROM data (archive member for zip/7z) |
| `md5`, `sha1` | hex, lower case, when computed |
| `serial` | disc serial normalised to upper-case letters and digits only (`SLUS00300`, `T14402M`) |
| `rahash` | the RetroAchievements hash, when the platform has one |
| `size` | the ROM data size in bytes |
| `regions` | comma list of regions parsed from the name, in full (`USA,Europe`) |
| `languages` | comma list of ISO 639-1 codes parsed from the name |
| `year` | a year parsed from the name (TOSEC) |

The script adds one `xbmcgui.ListItem` per candidate with
`xbmcplugin.addDirectoryItem(handle, url=<candidate id>, listitem=item, isFolder=False)`
and finishes with `xbmcplugin.endOfDirectory(handle)`. The item's label is the
candidate's title. Property `gamelibrary.candidate` holds:

```json
{"id": "<provider id>", "title": "Sonic the Hedgehog 2", "year": 1992,
 "platform": "megadrive", "score": 1.0, "matchedby": "hash",
 "subtitle": "Sonic The Hedgehog 2 (World)", "regions": ["World"],
 "provider": "libretro", "thumb": "https://..."}
```

`subtitle`, `regions`, `provider` and `thumb` are optional and exist for one
reason: when several candidates share a title, a person has to be able to tell
them apart in the selection dialog. Send the catalogue's own name for the entry
as `subtitle`, the regions it covers written in full, and an image if one is
free to give.

A candidate may also carry the property `gamelibrary.details` holding the
same JSON `getdetails` would answer with for it; Kodi then skips the
`getdetails` call for that candidate. Do this when the details cost nothing
extra, as with an offline catalogue.

`matchedby` is one of `hash`, `serial`, `name`. `score` is 0..1; a hash or
serial match is 1.0. Return candidates best first. Return nothing when nothing
matches: **never guess**. A name match must be exact after normalisation
(case, punctuation, whitespace and a leading article ignored); when several
entries share the normalised title, return all of them so Kodi can decide.

## `action=findmany`

A scan of a full set spends nearly all its time in the round trip to the
add-on, not in the lookup, so Kodi asks about a folder's games a hundred at a
time.

Query: `platform`, `platformids`, `pathSettings` and `batch=<path to a file>`.
The file holds the queries, in the order they are to be answered:

```json
{"version": 1, "queries": [
  {"title": "Sonic the Hedgehog 2", "filename": "Sonic 2 (World).md",
   "crc32": "24ab4c3a", "size": 1048576, "regions": "World"},
  {"title": "Streets of Rage", "filename": "Streets of Rage (USA, Europe).md"}
]}
```

Answer with the same list items `find` uses, one per candidate, each
candidate's JSON carrying `"query": <index into queries>`. Finish with one
extra item whose property `gamelibrary.batch` holds
`{"version": 1, "queries": <how many were read>}`: that is what tells Kodi the
batch was understood, so an empty answer means nothing matched rather than
"this scraper has no batches". A scraper that does not implement `findmany`
should fail the directory listing or leave the marker out; Kodi then asks
about each file on its own for the rest of the scan.

Kodi deletes the file after the call.

## `action=getdetails`

Query: `id=<candidate id>`, `platform`, `platformids`, and the same identity
parameters as `find`.

One item via `xbmcplugin.setResolvedUrl(handle, True, item)` with property
`gamelibrary.details`:

```json
{
  "version": 1,
  "title": "Sonic the Hedgehog 2",
  "sorttitle": "",
  "originaltitle": "",
  "overview": "",
  "releasedate": "1992-11-21",
  "year": 1992,
  "developers": ["Sega Technical Institute"],
  "publishers": ["Sega"],
  "genres": ["Platform"],
  "collections": ["Sonic the Hedgehog"],
  "tags": [],
  "players": {"min": 1, "max": 2},
  "coop": true,
  "category": "retail",
  "ratings": {"libretro": {"rating": 8.5, "max": 10, "votes": 0}},
  "ageratings": [{"board": "ESRB", "value": "E", "descriptors": ""}],
  "uniqueids": {"libretro": "Sonic the Hedgehog 2 (World)"},
  "releases": [
    {"title": "Sonic the Hedgehog 2 (World)", "regions": ["World"], "languages": [],
     "revision": "", "status": "retail", "licence": "licensed", "serial": "",
     "releasedate": "", "crc32": "24ab4c3a", "md5": "", "sha1": "", "size": 1048576}
  ],
  "art": {"boxfront": [{"url": "https://...png", "region": "USA"}],
          "titlescreen": [{"url": "https://...png"}],
          "screenshot": [{"url": "https://...png"}]},
  "achievements": {"total": 82, "points": 1045},
  "trailer": "",
  "manual": ""
}
```

Every key is optional except `version` and `title`. Words for `category`,
`status` and `licence` are the ones in `GameLibraryTypes.cpp`:
category `retail|hack|homebrew|demo|bios|application`; status
`retail|beta|proto|sample|demo|alpha|kiosk|debug|program`; licence
`licensed|unlicensed|aftermarket|homebrew|pirate`. Regions are written in
full, as No-Intro writes them: `USA`, `Europe`, `Japan`, `World`, `Asia`,
`Australia`, `Brazil`, `Canada`, `China`, `France`, `Germany`, `Italy`,
`Korea`, `Netherlands`, `Spain`, `Sweden`, `United Kingdom`, `Unknown` and
the rest. A provider that speaks region codes translates them at its own
edge. Art type names are Kodi's: `boxfront boxback boxspine boxfull box3d cart
clearlogo marquee banner screenshot titlescreen fanart icon mix flyer map
bezel`.

## `action=getplatform`

Query: `platform`, `platformids`. One resolved item with property
`gamelibrary.platform`:

```json
{"version": 1, "name": "Sega Mega Drive", "manufacturer": "Sega",
 "released": 1988, "discontinued": 1998, "overview": "...",
 "art": {"clearlogo": [{"url": ""}], "fanart": [{"url": ""}], "photo": [{"url": ""}]}}
```

## Behaviour

- Never write outside the add-on's profile directory
  (`xbmcvfs.translatePath('special://profile/addon_data/<id>/')`).
- Cache downloaded catalogues there; a cache is refreshed by age, never on
  every call.
- Respect provider limits; sleep rather than fail when throttled.
- Log with `xbmc.log(msg, xbmc.LOGDEBUG)`; errors at `LOGERROR` with the URL
  and the provider's message, never the user's credentials.
- Credentials and preferences live in the add-on's `settings.xml` and are
  entered by the user in the add-on's settings.
