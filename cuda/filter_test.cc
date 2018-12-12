
#include <iostream>
#include <vector>

#include "filter.h"

int main() {
  std::cout << "[FILTER_TEST] Starts" << std::endl;
  ruda::ConditionContext ctx = {
    ruda::EQ, 5,
  };

  std::vector<int> values{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  std::vector<int> results;

  std::cout << "[FILTER_TEST] Run SST Filter" << std::endl;
  ruda::sstIntFilter(values, ctx, results);

  std::cout << "[FILTER_TEST] Results" << std::endl;
  for (int i = 0; i < results.size(); ++i) {
    std::cout << results[i] << " ";
  }
  std::cout << std::endl;
  return 0;
}
