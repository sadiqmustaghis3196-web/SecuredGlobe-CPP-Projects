#pragma once
#include "repositories/BankRepository.hpp"
class SeedService final {
  public:
    static void initialize(BankRepository &repository);
};
