// Copyright 2023 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.



#include "open_spiel/games/crazy_eights.h"

#include "open_spiel/abseil-cpp/absl/strings/str_format.h"
#include "open_spiel/spiel.h"
#include "open_spiel/tests/basic_tests.h"


namespace open_spiel {
namespace crazy_eights {
namespace {

void BasicGameTests() {
  testing::LoadGameTest("crazy_eights");
  for(int players = 2; players <= 6; ++players){
    for(bool b: {false, true}){
      testing::RandomSimTest(*LoadGame(
          "crazy_eights", {{"players", GameParameter(players)}, {"use_special_cards", GameParameter(b)}}), 5);
    }
  }

}

}  // namespace
}  // namespace crazy_eights
}  // namespace open_spiel

int main() {
  open_spiel::crazy_eights::BasicGameTests();
}