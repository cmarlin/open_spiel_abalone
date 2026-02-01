// Copyright 2021 DeepMind Technologies Limited
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

#include <chrono>
#include <math.h>
#include <memory>

#include "open_spiel/abseil-cpp/absl/random/distributions.h"
#include "open_spiel/algorithms/minimax.h"
#include "open_spiel/algorithms/mcts.h"
#include "open_spiel/games/breakthrough/breakthrough.h"
#include "open_spiel/games/pig/pig.h"
#include "open_spiel/games/abalone/abalone.h"
#include "open_spiel/games/abalone/abalone_core.h"
#include "open_spiel/games/abalone/abalone_core_ab.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"
#include "open_spiel/utils/json.h"
#include "open_spiel/utils/data_logger.h"

inline constexpr int kSearchDepth = 2;
inline constexpr int kSearchDepthPig = 10;
inline constexpr int kWinscorePig = 30;
inline constexpr int kDiceoutcomesPig = 2;
inline constexpr int kSearchDepthAbalone = 5;
inline constexpr int kSeed = 726345721;

namespace open_spiel {
namespace {

int BlackPieceAdvantage(const State& state) {
  const auto& bstate = down_cast<const breakthrough::BreakthroughState&>(state);
  return bstate.pieces(breakthrough::kBlackPlayerId) -
         bstate.pieces(breakthrough::kWhitePlayerId);
}

void PlayBreakthrough() {
  std::shared_ptr<const Game> game =
      LoadGame("breakthrough", {{"rows", GameParameter(6)},
                                {"columns", GameParameter(6)}});
  std::unique_ptr<State> state = game->NewInitialState();
  while (!state->IsTerminal()) {
    std::cout << std::endl << state->ToString() << std::endl;

    Player player = state->CurrentPlayer();
    std::pair<double, Action> value_action = algorithms::AlphaBetaSearch(
        *game, state.get(), [player](const State& state) {
            return (player == breakthrough::kBlackPlayerId ?
                    BlackPieceAdvantage(state) :
                    -BlackPieceAdvantage(state));
            },
        kSearchDepth, player);

    std::cout << std::endl << "Player " << player << " choosing action "
              << state->ActionToString(player, value_action.second)
              << " with heuristic value (to black) " << value_action.first
              << std::endl;

    state->ApplyAction(value_action.second);
  }

  std::cout << "Terminal state: " << std::endl;
  std::cout << state->ToString() << std::endl;
}

int FirstPlayerAdvantage(const State& state) {
  const auto& pstate = down_cast<const pig::PigState&>(state);
  return pstate.score(0) - pstate.score(1);
}

void PlayPig(std::mt19937& rng) {
  std::shared_ptr<const Game> game =
      LoadGame("pig", {{"winscore", GameParameter(kWinscorePig)},
                       {"diceoutcomes", GameParameter(kDiceoutcomesPig)}});
  std::unique_ptr<State> state = game->NewInitialState();
  while (!state->IsTerminal()) {
    std::cout << std::endl << state->ToString() << std::endl;

    Player player = state->CurrentPlayer();
    if (state->IsChanceNode()) {
      // Chance node; sample one according to underlying distribution.
      ActionsAndProbs outcomes = state->ChanceOutcomes();
      Action action = open_spiel::SampleAction(outcomes, rng).first;
      std::cerr << "Sampled action: " << state->ActionToString(player, action)
                << std::endl;
      state->ApplyAction(action);
    } else {
      std::pair<double, Action> value_action = algorithms::ExpectiminimaxSearch(
          *game, state.get(),
          [player](const State& state) {
            return (player == Player{0} ? FirstPlayerAdvantage(state)
                                        : -FirstPlayerAdvantage(state));
          },
          kSearchDepthPig, player);

      std::cout << std::endl
                << "Player " << player << " choosing action "
                << state->ActionToString(player, value_action.second)
                << " with heuristic value " << value_action.first << std::endl;

      state->ApplyAction(value_action.second);
    }
  }

  std::cout << "Terminal state: " << std::endl;
  std::cout << state->ToString() << std::endl;
}

void PlayAbalone(std::mt19937& rng) {
  std::shared_ptr<const Game> game =
      LoadGame("abalone", {{"marbles_to_win", GameParameter(6)},
                                {"board", GameParameter(abalone_core::kDefaultBoard)}});

  float total_ms_spiel = 0.0;
  float total_ms_ab = 0.0;
  int count = 0;

  std::unique_ptr<State> state = game->NewInitialState();
  while (!state->IsTerminal()) {
    std::cout << std::endl << state->ToString() << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();
    Player player = state->CurrentPlayer();
    auto core_player = reinterpret_cast<const abalone::AbaloneState*>(state.get())->core_state_.ToPlay();
    std::pair<double, Action> value_action = algorithms::AlphaBetaSearch(
        *game,
        state.get(),
        [core_player](const State& state) {
            auto abalone_state = reinterpret_cast<const abalone::AbaloneState&>(state);
            return abalone_core::Heuristic(abalone_state.core_state_, core_player) * 0.001;
            },
        kSearchDepthAbalone,
        player);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> elapsed = end - start_time;
    total_ms_spiel += elapsed.count();

    std::cout << std::endl << "(ab) Player " << player << " choosing action "
              << state->ActionToString(player, value_action.second)
              << " with heuristic value (to black) " << value_action.first
            	<< " in " << total_ms_spiel/count << " ms"
              << std::endl;

    start_time = std::chrono::high_resolution_clock::now();
    auto best_move = abalone_core::AlphaBeta(static_cast<abalone::AbaloneState*>(state.get())->core_state_, kSearchDepthAbalone);
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start_time;
    total_ms_ab += elapsed.count();
    std::cout << std::endl << "(ab_core) Player " << player << " choosing action "
              << abalone_core::Move::ActionToMove(best_move.second).ToString()
              << " with heuristic value " << best_move.first
              << " in " << total_ms_ab/count << " ms"
              << std::endl;
    
    state->ApplyAction(value_action.second);
    count++;
  }

  std::cout << "Terminal state: " << std::endl;
  std::cout << state->ToString() << std::endl;
}

struct MoveEntry {
  int scenario_idx;
  int move_idx;
  std::vector<float> observation;
  int current_player;
  float value;
  int best_child;
  std::vector<std::pair<Action, float>> children;

  json::Object ToJson() const {
    std::map<std::string, json::Value> json_children;
    for(auto child: children)
    {
      json_children.insert({std::to_string(child.first), child.second});
    }

    return json::Object({
      {"scenario_idx", scenario_idx},
      {"move_idx", move_idx},
      {"observation", json::CastToArray(observation)},
      {"current_player", current_player},
      {"value", value},
      {"best_child", best_child},
      {"children", json::Object(json_children)},  //, [](auto c){ return {{std::to_string(c.first), c.second}}); }}
    });
  }
};

struct MoveDatabase {
  std::string game_name;
  std::string scenario_path;
  int num_distinct_actions;
  std::vector<int> observation_tensor_shape;
  std::vector<MoveEntry> moves;

  json::Object ToJson() const {
    return json::Object({
        {"game_name", game_name},
        {"scenario_path", scenario_path},
        {"num_distinct_actions", num_distinct_actions},
        {"observation_tensor_shape", json::CastToArray(observation_tensor_shape)},
        {"moves", json::TransformToArray(moves, [](auto v) { return v.ToJson(); })}
      });
    }

  // void FromJson(const json::Object& config_json) {
  //   game = config_json.at("game").GetString();
  //   path = config_json.at("path").GetString();
  // }
};

std::vector<std::string> split(const std::string& s, char seperator)
{
   std::vector<std::string> output;

    std::string::size_type prev_pos = 0, pos = 0;

    while((pos = s.find(seperator, pos)) != std::string::npos)
    {
        std::string substring( s.substr(prev_pos, pos-prev_pos) );

        output.push_back(substring);

        prev_pos = ++pos;
    }

    output.push_back(s.substr(prev_pos, pos-prev_pos)); // Last word

    return output;
}

MoveEntry _BuildRefMove(const std::string& _game_name, const std::string& _scenario, int _scenario_idx, int _seed)
{
  std::mt19937 rng_(_seed);
  auto game = LoadGame(_game_name);

  auto actions_str = split(_scenario, ' ');
  auto rnd_length = absl::Uniform(rng_, (std::size_t)1, actions_str.size());
  
  std::unique_ptr<State> state = game->NewInitialState();
  // std::vector<int> obs_shape = game->ObservationTensorShape();

  auto action_idx = 0;
  while(action_idx<rnd_length)
  {
    auto action = state->StringToAction(state->CurrentPlayer(), actions_str[action_idx]);
    state->ApplyAction(action);
    action_idx++;
  }

  // std::cout << "action_idx: " << action_idx << std::endl;
  // std::cout << "obs: " << state->ObservationString() << std::endl;

  MoveEntry move;
  Player player = state->CurrentPlayer();
  move.scenario_idx = _scenario_idx;
  move.move_idx = action_idx;
  move.observation = state->ObservationTensor(player);
  move.current_player = player;

  // openspiel AB
  #if 0
  auto kSearchDepthAbalone = 3;
  auto best_move = abalone::AllAbaloneMoves_ABSpiel(state, kSearchDepthAbalone, &(move.children));
  move.value = best_move.second;
  move.best_child = best_move.first;
  #endif

  // abalone AB
  #if 0
  auto kSearchDepthAbalone = 3;
  // std::vector<std::pair<abalone_core::core_Action, float>> all_moves;
  // auto start_time = std::chrono::high_resolution_clock::now();
  auto best_move = abalone_core::AlphaBeta(static_cast<abalone::AbaloneState*>(state.get())->core_state_, kSearchDepthAbalone, -1.f, 1.f, &move.children);
  // auto end = std::chrono::high_resolution_clock::now();
  // std::chrono::duration<float, std::milli> elapsed = end - start_time;

  // total_ms_ab += elapsed.count();
  // std::cout << std::endl << "(ab_core) Player " << player << " choosing action "
  //           << best_move.second.ToString()
  //           << " with heuristic value " << best_move.first
  //           << " in " << elapsed.count() << " ms"
  //           << std::endl;
  // move.children.push_back(std::make_pair(0, 0.001));
  move.value = best_move.second; // state->Returns()[player];  // move.best_child_total_reward;
  move.best_child = best_move.first;
  #endif

  // MCTS
  #if 1
  auto evaluator = std::make_shared<open_spiel::algorithms::RandomRolloutEvaluator>(20, _seed);
  // auto evaluator = std::make_shared<AbaloneHeuristicEvaluator>();
  float UCT_C = sqrtf(2.f);
  algorithms::MCTSBot bot(*game, evaluator, UCT_C,
                    /*max_simulations=*/ 10000,  //10000
                    /*max_memory_mb=*/ 10,
                    /*solve=*/ true,
                    /*seed=*/ _seed,
                    /*verbose=*/ false);
  auto best_value = -1.001f;
  auto best_action = -1;
  for(auto action: state->LegalActions())
  {
    auto childstate = state->Child(action);
    if(childstate.get()->IsTerminal())
    {
      auto q_value = childstate->Returns()[player];
      if(q_value>best_value)
      {
        best_action = action;
        best_value = q_value;
      }
      move.children.push_back(std::make_pair(action, q_value));
      continue;
    }

    std::unique_ptr<algorithms::SearchNode> root = bot.MCTSearch(*childstate);
    auto best_child = root->BestChild();
    float q_value = -best_child.UCTValue(1, 0.f);
    if(q_value>best_value)
    {
      best_action = action;
      best_value = q_value;
    }
    move.children.push_back(std::make_pair(action, q_value));
  }
  move.value = best_value;
  move.best_child = best_action;
  #endif

  return move;
}

void DatasetAbalone()
{
  // auto scenario_path = "/home/user/projects/space_explorer/dataset/abalone_16k.log";
  // auto path = "/home/user/projects/space_explorer/dataset/abalone_ref-moves.json";
  // auto game_name = "abalone";

  // auto scenario_path = "/home/user/projects/space_explorer/dataset/tic_tac_toe_1k.log";
  // auto path = "/home/user/projects/space_explorer/dataset/tic_tac_toe_ref-moves.json";
  // auto game_name = "tic_tac_toe";

  auto scenario_path = "/home/user/projects/space_explorer/dataset/connect4_mcts512_mcts512.log";
  auto path = "/home/user/projects/space_explorer/dataset/connect_four_ref-moves.json";
  auto game_name = "connect_four";


  file::File scenario_fd(scenario_path, "r");
  std::string scenarios_raw = scenario_fd.ReadContents();

  std::string::size_type prev_pos = 0, pos = 0;
  auto scenarios = split(scenarios_raw, '\n');
  // int line = 0;
  // for(auto s: scenarios)
  // {
  //   std::cout << line << " => " << s << std::endl;
  //   line++;
  // }

  auto game = LoadGame(game_name);
  MoveDatabase config;
  config.game_name = game_name;
  config.scenario_path = scenario_path;
  config.num_distinct_actions = game->NumDistinctActions();
  config.observation_tensor_shape = game->ObservationTensorShape();

  printf("\n");
  int scenario_id = 0;
  for(auto scenario: scenarios)
  {
    printf("\r%d/100", scenario_id*100/1024);
    fflush(stdout);
    auto move = _BuildRefMove(game_name, scenario, scenario_id, 42+scenario_id);
    config.moves.push_back(move);
    scenario_id ++;
    if(scenario_id>=1024)
    {
      break;
    }
  }
  printf("\n");

  // 
  // json::CastToArray

  file::File fd(path, "w");
  fd.Write(json::ToString(config.ToJson(), true) + "\n");
    
  // auto path = "/tmp";
  // DataLoggerJsonLines data_logger(
  //   path, "name");
  
  // DataLogger::Record record = {
  //     {"step", 2},
  //     {"states_per_s", 12.5f}
  // };

  // // record.emplace(json::Object({{"size", 3}, {"hits", 10000}}));
  // // data_logger.Write(config);
  // data_logger.Write(record);
  // data_logger.Write(record);
}

void MakeRollout(int p0_depth = 2, int p1_depth = 2)
{
	//auto kSeed = 42;
	//std::time_t result = std::time(nullptr);
	std::random_device rd;
	std::mt19937 rng(rd());

	auto rollout = std::string();
	abalone_core::core_state state;
	state.Reset(abalone_core::ABALONE_INIT_CLASSIC);
	//state.reset(ABALONE_INIT_BELGIAN_DAISY);
	while (state.outcome_ == abalone_core::CellState::Invalid)
	{
		auto current_player = state.ToPlay();
		auto depth = current_player == abalone_core::CellState::Player0 ? p0_depth : p1_depth;
		std::vector<std::pair<abalone_core::core_Action, float>> all_moves;
		auto best_move = abalone_core::AlphaBeta(state, depth, -1.f, 1.f, &all_moves);
		//auto best_move.second;
		all_moves.erase(std::remove_if(all_moves.begin(), all_moves.end(), [best_move](auto m) {return m.second != best_move.second; }), all_moves.end());
		//std::shuffle(all_moves.begin(), all_moves.end(), rng);
		std::uniform_int_distribution<int> uid(0, all_moves.size()-1);
		auto random_number = uid(rng);
		auto move = abalone_core::Move::ActionToMove(all_moves[random_number].first);
		move.Apply(state);
		if (rollout.length() > 0)
		{
			rollout += " ";
		}
		rollout += move.ToString();
	}
	std::cout << rollout << std::endl;
	//std::cout << state.outcome_ << std::endl;
}

}  // namespace
}  // namespace open_spiel

int main(int argc, char **argv) {
  std::mt19937 rng(kSeed);  // Random number generator.
  // open_spiel::PlayBreakthrough();
  // open_spiel::PlayPig(rng);
  // open_spiel::PlayAbalone(rng);
  // open_spiel::DatasetAbalone();

  int p0_depth = 2;
  int p1_depth = 2;
  if(argc>1)
  {
    p0_depth = atoi(argv[1]);
  }
  if(argc>2)
  {
    p1_depth = atoi(argv[2]);
  }
  open_spiel::MakeRollout(p0_depth, p1_depth);
}

