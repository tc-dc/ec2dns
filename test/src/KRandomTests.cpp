#include <string>

#include "gtest/gtest.h"
#include "KRandom.h"

void _Test(size_t numNodes, std::vector<std::string> expected){
  tls_random::seed_thread(1);
  std::vector<std::string> nodes;
  for (size_t a = 0; a < numNodes; a++) {
    nodes.push_back(std::to_string(a));
  }

  auto rndIter = k_random<std::string>(nodes, expected.size());
  auto i1 = expected.begin(); auto i2 = rndIter.begin();
  for (; i1 != expected.end() && i2 != rndIter.end(); ++i1, ++i2) {
    ASSERT_EQ(*i1, *i2);
  }
}

TEST(KRandom, KRandomSeqPickOne) {
  _Test(3, {"2"});
}

TEST(KRandom, KRandomSeqPickAll) {
  _Test(3, {"2", "1", "0"});
}

TEST(KRandom, KRandomHugeSeq) {
  _Test(10000, {"6113"});
}