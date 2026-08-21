/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PdfDocumentCache.h"

#include "PdfDocument.h"

using namespace KODI::GAME;

CPdfDocumentCache::CPdfDocumentCache() = default;

CPdfDocumentCache::~CPdfDocumentCache() = default;

CPdfDocumentCache& CPdfDocumentCache::GetInstance()
{
  static CPdfDocumentCache instance;
  return instance;
}

const CPdfDocument* CPdfDocumentCache::GetOrOpen(const std::string& path)
{
  if (m_document && m_path == path)
    return m_document.get();

  // A different document is wanted, so the held one is of no further use.
  // Released before opening the new one so that two manuals are never in
  // memory at once.
  m_document.reset();
  m_path.clear();

  std::unique_ptr<CPdfDocument> document = CPdfDocument::Open(path);
  if (!document)
    return nullptr;

  m_document = std::move(document);
  m_path = path;

  return m_document.get();
}

unsigned int CPdfDocumentCache::GetPageCount(const std::string& path)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  const CPdfDocument* document = GetOrOpen(path);
  if (document == nullptr)
    return 0;

  return document->GetPageCount();
}

void CPdfDocumentCache::Clear()
{
  std::unique_lock<std::mutex> lock(m_mutex);

  m_document.reset();
  m_path.clear();
}
