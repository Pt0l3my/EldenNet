// R1's compile-fail tests (ARCH §10.1): each EN_CF_* case uses the game thread's token in a way the type forbids, and
// CTest expects its build to fail (WILL_FAIL). Built with none of them, the file is the control: it must build, so that
// a failure of the others is theirs and not the file's.
#include <utility>

#include "en/proto/thread.hpp"

using en::proto::GameThread;

namespace {

void read_with(const GameThread&) {}
void keep(GameThread) {}

}  // namespace

int main() {
  GameThread gt = GameThread::issue();
  read_with(gt);
  GameThread moved = std::move(gt);
  keep(std::move(moved));
#if defined(EN_CF_COPY)
  GameThread token = GameThread::issue();
  GameThread copy = token;  // a copy of a token
  read_with(copy);
#elif defined(EN_CF_COPY_FROM_REFERENCE)
  const GameThread token = GameThread::issue();
  const GameThread& ref = token;
  keep(ref);  // a copy taken from a reference, as code off the game thread would need
#elif defined(EN_CF_ASSIGN)
  GameThread a = GameThread::issue();
  GameThread b = GameThread::issue();
  a = std::move(b);  // assignment
#elif defined(EN_CF_MAKE)
  GameThread made;  // a token made without issue()
  read_with(made);
#elif defined(EN_CF_MAKE_BRACED)
  read_with(GameThread{});  // the same, braced
#endif
  return 0;
}
