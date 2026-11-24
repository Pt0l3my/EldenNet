#pragma once
// EldenNet's bundled documents (Docs, PAGES §4.6.9; GraceUI D251, eldennet-changes §77): the ten drafts of
// src/en-overlay/docs/, converted by tools/docs/build.py into src/docs_data.cpp and compiled into eldennet.dll, so
// nothing reads them from disk or fetches them (PV6). The space (space.hpp) publishes them: the master list
// `docs.list`, a row a document ({id, title, headings}, the list's search reading the title and the headings alone,
// D250), and each document as the member `docs/<id>` of the family `docs`, {"title", "blocks"}, About's three slots
// filled from what EldenNet knows when it publishes.
#include <span>
#include <string_view>

namespace en::overlay {

inline constexpr std::string_view kDocsList = "docs.list";  // the master list
inline constexpr std::string_view kDocsFamily = "docs";     // its documents: docs/<id>
inline constexpr std::string_view kDocsAbout = "about";     // the one document with slots

struct BundledDoc {
  std::string_view id;        // GraceUI's document id ([a-z0-9._-], 64 at most)
  std::string_view title;
  std::string_view headings;  // its headings, levels 2 and 3, joined by " · "
  std::string_view body;      // {"blocks", "title"}: compact JSON, its keys sorted, as nlohmann/json writes it
};

// The documents in Docs' order (docs_data.cpp, generated).
[[nodiscard]] std::span<const BundledDoc> bundled_docs() noexcept;

}  // namespace en::overlay
