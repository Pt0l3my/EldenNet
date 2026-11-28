// BLAKE3 against its published test vectors (BLAKE3's test_vectors/test_vectors.json at tag 1.8.2, CC0 1.0 or
// Apache 2.0; THIRD_PARTY.md): the hash, keyed-hash and derive-key modes, whole and fed in pieces of awkward sizes.
#include <doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include "en/proto/blake3.hpp"

using en::proto::Blake3;

namespace {

// The vectors' input: byte i is i mod 251.
std::vector<std::byte> vector_input(std::size_t n) {
  std::vector<std::byte> v(n);
  for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<std::byte>(i % 251);
  return v;
}

std::string hex(const Blake3::Digest& d) {
  static constexpr char kDigits[] = "0123456789abcdef";
  std::string out;
  for (const std::uint8_t b : d) {
    out.push_back(kDigits[b >> 4]);
    out.push_back(kDigits[b & 0xF]);
  }
  return out;
}

std::string in_pieces(Blake3 h, const std::vector<std::byte>& input, std::size_t piece) {
  for (std::size_t at = 0; at < input.size(); at += piece) {
    h.update(std::span<const std::byte>(input).subspan(at, std::min(piece, input.size() - at)));
  }
  return hex(h.finish());
}

constexpr std::size_t kPieces[] = {std::size_t{1} << 20, 1, 63, 64, 1000, 1024, 4097};

// The vectors' key and context string.
Blake3::Key test_key() {
  const std::string k = "whats the Elvish word for friend";
  Blake3::Key key{};
  for (std::size_t i = 0; i < key.size(); ++i) key[i] = static_cast<std::uint8_t>(k[i]);
  return key;
}
constexpr const char* kContext = "BLAKE3 2019-12-27 16:29:52 test vectors context";

}  // namespace

TEST_CASE("BLAKE3: the hash mode's published vectors, whole and in pieces of every awkward size") {
  // The first 32 bytes of each case's `hash`.
  const std::vector<std::pair<std::size_t, const char*>> vectors = {
      {0, "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"},
      {1, "2d3adedff11b61f14c886e35afa036736dcd87a74d27b5c1510225d0f592e213"},
      {63, "e9bc37a594daad83be9470df7f7b3798297c3d834ce80ba85d6e207627b7db7b"},
      {64, "4eed7141ea4a5cd4b788606bd23f46e212af9cacebacdc7d1f4c6dc7f2511b98"},
      {65, "de1e5fa0be70df6d2be8fffd0e99ceaa8eb6e8c93a63f2d8d1c30ecb6b263dee"},
      {1023, "10108970eeda3eb932baac1428c7a2163b0e924c9a9e25b35bba72b28f70bd11"},
      {1024, "42214739f095a406f3fc83deb889744ac00df831c10daa55189b5d121c855af7"},
      {1025, "d00278ae47eb27b34faecf67b4fe263f82d5412916c1ffd97c8cb7fb814b8444"},
      {2048, "e776b6028c7cd22a4d0ba182a8bf62205d2ef576467e838ed6f2529b85fba24a"},
      {2049, "5f4d72f40d7a5f82b15ca2b2e44b1de3c2ef86c426c95c1af0b6879522563030"},
      {3072, "b98cb0ff3623be03326b373de6b9095218513e64f1ee2edd2525c7ad1e5cffd2"},
      {3073, "7124b49501012f81cc7f11ca069ec9226cecb8a2c850cfe644e327d22d3e1cd3"},
      {8193, "bab6c09cb8ce8cf459261398d2e7aef35700bf488116ceb94a36d0f5f1b7bc3b"},
      {16384, "f875d6646de28985646f34ee13be9a576fd515f76b5b0a26bb324735041ddde4"},
      {31744, "62b6960e1a44bcc1eb1a611a8d6235b6b4b78f32e7abc4fb4c6cdcce94895c47"},
      {102400, "bc3e3d41a1146b069abffad3c0d44860cf664390afce4d9661f7902e7943e085"},
  };
  for (const auto& [len, want] : vectors) {
    CAPTURE(len);
    const auto input = vector_input(len);
    for (const std::size_t piece : kPieces) {
      CAPTURE(piece);
      CHECK(in_pieces(Blake3(), input, piece) == want);
    }
  }
}

TEST_CASE("BLAKE3: the keyed-hash and derive-key modes' published vectors") {
  // The first 32 bytes of each case's `keyed_hash` and `derive_key`.
  const std::vector<std::tuple<std::size_t, const char*, const char*>> vectors = {
      {0, "92b2b75604ed3c761f9d6f62392c8a9227ad0ea3f09573e783f1498a4ed60d26",
       "2cc39783c223154fea8dfb7c1b1660f2ac2dcbd1c1de8277b0b0dd39b7e50d7d"},
      {1, "6d7878dfff2f485635d39013278ae14f1454b8c0a3a2d34bc1ab38228a80c95b",
       "b3e2e340a117a499c6cf2398a19ee0d29cca2bb7404c73063382693bf66cb06c"},
      {63, "bb1eb5d4afa793c1ebdd9fb08def6c36d10096986ae0cfe148cd101170ce37ae",
       "b6451e30b953c206e34644c6803724e9d2725e0893039cfc49584f991f451af3"},
      {64, "ba8ced36f327700d213f120b1a207a3b8c04330528586f414d09f2f7d9ccb7e6",
       "a5c4a7053fa86b64746d4bb688d06ad1f02a18fce9afd3e818fefaa7126bf73e"},
      {65, "c0a4edefa2d2accb9277c371ac12fcdbb52988a86edc54f0716e1591b4326e72",
       "51fd05c3c1cfbc8ed67d139ad76f5cf8236cd2acd26627a30c104dfd9d3ff8a8"},
      {1023, "c951ecdf03288d0fcc96ee3413563d8a6d3589547f2c2fb36d9786470f1b9d6e",
       "74a16c1c3d44368a86e1ca6df64be6a2f64cce8f09220787450722d85725dea5"},
      {1024, "75c46f6f3d9eb4f55ecaaee480db732e6c2105546f1e675003687c31719c7ba4",
       "7356cd7720d5b66b6d0697eb3177d9f8d73a4a5c5e968896eb6a689684302706"},
      {1025, "357dc55de0c7e382c900fd6e320acc04146be01db6a8ce7210b7189bd664ea69",
       "effaa245f065fbf82ac186839a249707c3bddf6d3fdda22d1b95a3c970379bcb"},
      {2048, "879cf1fa2ea0e79126cb1063617a05b6ad9d0b696d0d757cf053439f60a99dd1",
       "7b2945cb4fef70885cc5d78a87bf6f6207dd901ff239201351ffac04e1088a23"},
      {2049, "9f29700902f7c86e514ddc4df1e3049f258b2472b6dd5267f61bf13983b78dd5",
       "2ea477c5515cc3dd606512ee72bb3e0e758cfae7232826f35fb98ca1bcbdf273"},
      {3072, "044a0e7b172a312dc02a4c9a818c036ffa2776368d7f528268d2e6b5df191770",
       "050df97f8c2ead654d9bb3ab8c9178edcd902a32f8495949feadcc1e0480c46b"},
      {3073, "68dede9bef00ba89e43f31a6825f4cf433389fedae75c04ee9f0cf16a427c95a",
       "72613c9ec9ff7e40f8f5c173784c532ad852e827dba2bf85b2ab4b76f7079081"},
      {8193, "954a2a75420c8d6547e3ba5b98d963e6fa6491addc8c023189cc519821b4a1f5",
       "af1e0346e389b17c23200270a64aa4e1ead98c61695d917de7d5b00491c9b0f1"},
      {16384, "9e9fc4eb7cf081ea7c47d1807790ed211bfec56aa25bb7037784c13c4b707b0d",
       "160e18b5878cd0df1c3af85eb25a0db5344d43a6fbd7a8ef4ed98d0714c3f7e1"},
      {31744, "efa53b389ab67c593dba624d898d0f7353ab99e4ac9d42302ee64cbf9939a419",
       "39772aef80e0ebe60596361e45b061e8f417429d529171b6764468c22928e28e"},
      {102400, "1c35d1a5811083fd7119f5d5d1ba027b4d01c0c6c49fb6ff2cf75393ea5db4a7",
       "4652cff7a3f385a6103b5c260fc1593e13c778dbe608efb092fe7ee69df6e9c6"},
  };
  const Blake3::Key key = test_key();
  for (const auto& [len, keyed, derived] : vectors) {
    CAPTURE(len);
    const auto input = vector_input(len);
    for (const std::size_t piece : kPieces) {
      CAPTURE(piece);
      CHECK(in_pieces(Blake3::keyed(key), input, piece) == keyed);
      CHECK(in_pieces(Blake3::derive_key(kContext), input, piece) == derived);
    }
  }
}

TEST_CASE("BLAKE3: the modes give different digests of the same input, and a string feeds as its bytes") {
  Blake3 plain;
  plain.update("abc");
  Blake3 bytes;
  const std::string abc = "abc";
  bytes.update(std::as_bytes(std::span<const char>(abc)));
  CHECK(hex(plain.finish()) == hex(bytes.finish()));
  Blake3 keyed = Blake3::keyed(test_key());
  keyed.update("abc");
  Blake3 derived = Blake3::derive_key(kContext);
  derived.update("abc");
  CHECK(hex(plain.finish()) != hex(keyed.finish()));
  CHECK(hex(plain.finish()) != hex(derived.finish()));
  CHECK(hex(keyed.finish()) != hex(derived.finish()));
}
