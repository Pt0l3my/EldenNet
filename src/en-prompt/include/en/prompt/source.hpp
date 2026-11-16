#pragma once
// Where an environment's templates come from (ARCH §2.3): a folder on disk, for the tools, or names and texts held in
// memory, for the engine's core, which does no I/O and hands over the content pack's templates already loaded.
//
// A source answers four questions about an exact name: what it is (a file, a folder or nothing), which files a folder
// holds, what a file's text is, and whether the name leads out of the source. The rest is the environment's (the name
// rules, a bare name's extensions, a folder's numbered parts and their order, composition and its guards), so every
// source resolves a name the same way, with the same errors. A name reaches a source in its checked form: '/' between
// folders and none at either end, with no '.', '..' or empty folder names. One source may serve several environments
// and threads at once, so its methods must be safe to call concurrently.
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "en/prompt/errors.hpp"

namespace en::prompt {

class TemplateSource {
 public:
  enum class Entry { none, file, folder };

  virtual ~TemplateSource() = default;

  // What the exact name is here.
  virtual Entry find(std::string_view name) const = 0;
  // The names of the files directly inside a folder (not those of its folders), in any order; nothing when the folder
  // cannot be read.
  virtual std::optional<std::vector<std::string>> files(std::string_view folder) const = 0;
  // A file's text; nothing when it cannot be read.
  virtual std::optional<std::string> read(std::string_view name) const = 0;
  // Whether a name that find() found leads out of the source, as a link on disk can. False unless a source says so.
  virtual bool leads_outside(std::string_view name) const;
};

// Templates in a folder on disk, for the tools. Names resolve from the root. find() follows links, as looking at a
// name on disk does, and leads_outside() tells whether one leads out of the root; files() and read() refuse such a
// name, and a name that is not in the checked form finds nothing, so the source never lists or reads anything
// outside its root, even when it is asked directly.
class FolderSource final : public TemplateSource {
 public:
  // `root` must be an existing folder. Throws std::invalid_argument.
  explicit FolderSource(std::filesystem::path root);

  Entry find(std::string_view name) const override;
  std::optional<std::vector<std::string>> files(std::string_view folder) const override;
  std::optional<std::string> read(std::string_view name) const override;
  bool leads_outside(std::string_view name) const override;

  const std::filesystem::path& root() const noexcept { return root_; }

 private:
  std::optional<std::filesystem::path> path_of(std::string_view name) const;

  std::filesystem::path root_;
  std::filesystem::path canonical_root_;
};

// Templates held in memory, for the engine's core: no file is opened. Each key is a file's name from the root, under
// the rules every template name follows (letters, digits, '_', '-' and '.', with '/' or '\' between folders, and no
// '.', '..' or empty folder names), and is kept in its checked form ("registers\calm.md" as "registers/calm.md"). The
// folders are the ones the names' paths imply, so none is empty. A key that is not a template file, such as
// "dialogue/README.txt", behaves as that file does in a folder: it is no part, and naming it is refused.
class MemorySource final : public TemplateSource {
 public:
  // Throws TemplateError, at the key, for a key that breaks the name rules (with the message a render gives for that
  // name), for two keys with one checked form ("a/b.j2" and "a\b.j2"), and for a name that is a file and a folder.
  explicit MemorySource(std::map<std::string, std::string> templates);

  Entry find(std::string_view name) const override;
  std::optional<std::vector<std::string>> files(std::string_view folder) const override;
  std::optional<std::string> read(std::string_view name) const override;

 private:
  std::map<std::string, std::string, std::less<>> files_;  // by checked name
  std::set<std::string, std::less<>> folders_;
};

}  // namespace en::prompt
