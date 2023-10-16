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

#ifndef OPEN_SPIEL_GAMES_ABALONE_H_
#define OPEN_SPIEL_GAMES_ABALONE_H_

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "open_spiel/spiel.h"

// Simple game of Abalone
// https://en.wikipedia.org/wiki/Abalone_(board_game)
//
// Parameters: none
// marbles_to_win
// initital_board

namespace open_spiel {
namespace abalone {

enum Direction : int  // counterclockwise order
{
  Direction_First = 0,

  RIGHT = Direction_First,
  UP_RIGHT = 1,
  UP_LEFT = 2,
  LEFT = 3,
  DOWN_LEFT = 4,
  DOWN_RIGHT = 5,

  Direction_Last = 6,
  Direction_Invalid = -1,
};

// Constants.
inline constexpr int kNumPlayers = 2;
inline constexpr int kNumRows = 9;
inline constexpr int kNumCols = 9;
inline constexpr int kNumCells = kNumRows * kNumCols;
inline constexpr int kNumActionsPerDirection = 5;  // single move or slide move x2 or x3 from near or far left
inline constexpr int kNumActionsPerCell = Direction::Direction_Last * kNumActionsPerDirection;
inline constexpr int kHistoryMax = 200;  // a game coudn't last more than that
inline constexpr int kMarblesToWin = 6;  // stop a game when one player lost this number of marbles (default:6 blitz:4)
inline constexpr int kCellStates = 2 + kNumPlayers;  // empty, invalid, and players

// State of a cell.
enum CellState : int
{
	kInvalid = -2,
	kEmpty = -1,
	kPlayer1 = 0,
	kPlayer2 = 1,
};

constexpr CellState VALID_BOARD[kNumRows][kNumCols] = {
	// 1				            2				              3				              4				              5				              6				              7				              8				              9
	{ CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid,	CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty },    // i
	{ CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid,	CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty },    // h
	{ CellState::kInvalid,	CellState::kInvalid,	CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty },    // g
	{ CellState::kInvalid,	CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty },    // f
	{ CellState::kEmpty,	  CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty },    // e
	{ CellState::kEmpty,	  CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kInvalid },  // d
	{ CellState::kEmpty,	  CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kInvalid,	CellState::kInvalid },  // c
	{ CellState::kEmpty,	  CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid },  // b
	{ CellState::kEmpty,	  CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid	},  // a
};

constexpr CellState ABALONE_INIT_CLASSIC[kNumRows][kNumCols] = {
	// 1				            2				              3				              4				              5				              6				              7				              8				              9
	{ CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid,	CellState::kPlayer2,	CellState::kPlayer2,	CellState::kPlayer2,	CellState::kPlayer2,	CellState::kPlayer2 },  // i
	{ CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid,	CellState::kPlayer2,	CellState::kPlayer2,	CellState::kPlayer2,	CellState::kPlayer2,	CellState::kPlayer2,	CellState::kPlayer2 },  // h
	{ CellState::kInvalid,	CellState::kInvalid,	CellState::kEmpty,		CellState::kEmpty,		CellState::kPlayer2,	CellState::kPlayer2,	CellState::kPlayer2,	CellState::kEmpty,		CellState::kEmpty },    // g
	{ CellState::kInvalid,	CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty },    // f
	{ CellState::kEmpty,	  CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty },    // e
	{ CellState::kEmpty,	  CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kInvalid },  // d
	{ CellState::kEmpty,	  CellState::kEmpty,		CellState::kPlayer1,	CellState::kPlayer1,	CellState::kPlayer1,	CellState::kEmpty,		CellState::kEmpty,		CellState::kInvalid,	CellState::kInvalid },  // c
	{ CellState::kPlayer1,	CellState::kPlayer1,	CellState::kPlayer1,	CellState::kPlayer1,	CellState::kPlayer1,	CellState::kPlayer1,	CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid },  // b
	{ CellState::kPlayer1,	CellState::kPlayer1,	CellState::kPlayer1,	CellState::kPlayer1,	CellState::kPlayer1,	CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid },  // a
};

/// <summary>
/// Hexagone board is represented by a square with some kInvalid case:
/// 
/// I     2 2 2 2 2
/// H    2 2 2 2 2 2
/// G   0 0 2 2 2 0 0
/// F  0 0 0 0 0 0 0 0
/// E 0 0 0 0 0 0 0 0 0
/// D  0 0 0 0 0 0 0 0 \9
/// C   0 0 1 1 1 0 0 \8
/// B    1 1 1 1 1 1 \7
/// A     1 1 1 1 1 \6
///        \1\2\3\4\5
/// 
/// Square/Memory representation
/// 
/// I X X X X 2 2 2 2 2
/// H X X X 2 2 2 2 2 2
/// G X X 0 0 2 2 2 0 0
/// F X 0 0 0 0 0 0 0 0
/// E 0 0 0 0 0 0 0 0 0
/// D 0 0 0 0 0 0 0 0 X
/// C 0 0 1 1 1 0 0 X X
/// B 1 1 1 1 1 1 X X X
/// A 1 1 1 1 1 X X X X
///   1 2 3 4 5 6 7 8 9
/// </summary>

// cf https://abaloneonline.wordpress.com/variations/the-classics/
constexpr CellState ABALONE_INIT_BELGIAN_DAISY[kNumRows][kNumCols] = {
	// 1				            2				              3				              4				              5				              6				              7				              8				              9
	{ CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid,	CellState::kPlayer2,	CellState::kPlayer2,	CellState::kEmpty,		CellState::kPlayer1,	CellState::kPlayer1 },  // i
	{ CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid,	CellState::kPlayer2,	CellState::kPlayer2,	CellState::kPlayer2,	CellState::kPlayer1,	CellState::kPlayer1,	CellState::kPlayer1 },  // h
	{ CellState::kInvalid,	CellState::kInvalid,	CellState::kEmpty,		CellState::kPlayer2,	CellState::kPlayer2,	CellState::kEmpty,		CellState::kPlayer1,	CellState::kPlayer1,	CellState::kEmpty },    // g
	{ CellState::kInvalid,	CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty },    // f
	{ CellState::kEmpty,	  CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty },    // e
	{ CellState::kEmpty,	  CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kEmpty,		CellState::kInvalid },  // d
	{ CellState::kEmpty,	  CellState::kPlayer1,	CellState::kPlayer1,	CellState::kEmpty,		CellState::kPlayer2,	CellState::kPlayer2,	CellState::kEmpty,		CellState::kInvalid,	CellState::kInvalid },  // c
	{ CellState::kPlayer1,	CellState::kPlayer1,	CellState::kPlayer1,	CellState::kPlayer2,	CellState::kPlayer2,	CellState::kPlayer2,	CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid },  // b
	{ CellState::kPlayer1,	CellState::kPlayer1,	CellState::kEmpty,		CellState::kPlayer2,	CellState::kPlayer2,	CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid,	CellState::kInvalid },  // a
};

constexpr std::pair<Direction, Direction> Sisters[] = {  // eq to dir+1 and dir+2
	{ Direction::UP_RIGHT, Direction::UP_LEFT },  // Direction::RIGHT
	{ Direction::UP_LEFT, Direction::LEFT },  // Direction::UP_RIGHT
	{ Direction::LEFT, Direction::DOWN_LEFT },  // Direction::UP_LEFT
	{ Direction::DOWN_LEFT, Direction::DOWN_RIGHT },  // Direction::LEFT
	{ Direction::DOWN_RIGHT, Direction::RIGHT },  // Direction::DOWN_LEFT
	{ Direction::RIGHT, Direction::UP_RIGHT },  // Direction::DOWN_RIGHT
};
static_assert(sizeof(Sisters) / sizeof(Sisters[0]) == Direction_Last, "mismatch size");

struct Coordinate {
	int m_row;
	int m_column;

	bool operator == (const Coordinate& other) const {
		return m_row == other.m_row &&
			m_column == other.m_column;
	}

	Coordinate operator + (const Coordinate& other) const {
		Coordinate ret;
		ret.m_row = m_row + other.m_row;
		ret.m_column = m_column + other.m_column;
		return ret;
	}
};

constexpr Coordinate Offsets[] = {
	// row, column
	{ 0,  1 },  // Direction::RIGHT
	{ -1,  1 },  // Direction.UP_RIGHT
	{ -1,  0 },  // Direction.UP_LEFT
	{ 0, -1 },  // Direction.LEFT
	{ 1, -1 },  // Direction.DOWN_LEFT
	{ 1,  0 },  // Direction.DOWN_RIGHT
};
static_assert(sizeof(Offsets) / sizeof(Offsets[0]) == Direction_Last, "mismatch size");


// State of an in-play game.
class AbaloneState : public State {
 public:
  AbaloneState(std::shared_ptr<const Game> game);

  AbaloneState(const AbaloneState&) = default;
  AbaloneState& operator=(const AbaloneState&) = default;

  Player CurrentPlayer() const override {
    return IsTerminal() ? kTerminalPlayerId : current_player_;
  }
  std::string ActionToString(Player player, Action action_id) const override;
  std::string ToString() const override;
  bool IsTerminal() const override;
  // std::vector<double> Rewards() const override;
  std::vector<double> Returns() const override;
  std::string InformationStateString(Player player) const override;
  std::string ObservationString(Player player) const override;
  void ObservationTensor(Player player,
                         absl::Span<float> values) const override;
  std::unique_ptr<State> Clone() const override;
  std::vector<Action> LegalActions() const override;
  void SetBoard(int row, int column, CellState state) {
    board_[row * kNumCols + column] = state;
  }
  CellState BoardAt(int row, int column) const {
    return board_[row * kNumCols + column];
  }
  Player outcome() const { return outcome_; }

 protected:
  std::array<CellState, kNumCells> board_;
  void DoApplyAction(Action action) override;

 private:
  friend struct Move;
  Player current_player_ = 0;         // Player zero goes first
  Player outcome_ = kInvalidPlayer;   // winner
  int num_moves_ = 0;
};

// Game object.
class AbaloneGame : public Game {
 public:
  explicit AbaloneGame(const GameParameters& params);
  int NumDistinctActions() const override { return kNumCells*kNumActionsPerCell; }
  std::unique_ptr<State> NewInitialState() const override;
  int NumPlayers() const override { return kNumPlayers; }
  double MinUtility() const override { return -1; }
  absl::optional<double> UtilitySum() const override { return 0; }
  double MaxUtility() const override { return 1; }
  std::vector<int> ObservationTensorShape() const override {
    return {kCellStates, kNumRows, kNumCols};
  }
  int MaxGameLength() const override { return kHistoryMax; }
  std::string ActionToString(Player player, Action action_id) const override;
};

CellState PlayerToState(Player player);
std::string StateToString(CellState state);

inline std::ostream& operator<<(std::ostream& stream, const CellState& state) {
  return stream << StateToString(state);
}

}  // namespace abalone
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_ABALONE_H_
