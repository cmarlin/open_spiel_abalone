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

#include "open_spiel/games/abalone/abalone.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "open_spiel/spiel_utils.h"
#include "open_spiel/utils/tensor_view.h"

namespace open_spiel {
namespace abalone {
namespace {

// Facts about the game.
const GameType kGameType{
    /*short_name=*/"abalone",
    /*long_name=*/"Abalone",
    GameType::Dynamics::kSequential,
    GameType::ChanceMode::kDeterministic,
    GameType::Information::kPerfectInformation,
    GameType::Utility::kZeroSum,
    GameType::RewardModel::kTerminal,
    /*max_num_players=*/2,
    /*min_num_players=*/2,
    /*provides_information_state_string=*/true,
    /*provides_information_state_tensor=*/false,
    /*provides_observation_string=*/true,
    /*provides_observation_tensor=*/true,
    /*parameter_specification=*/{
      {"marbles_to_win", GameParameter(kMarblesToWin)},
      {"marble_reward", GameParameter(kMarbleReward)},
	  {"board", GameParameter(kDefaultBoard)},
	  {"invert", GameParameter(kInvertBoard)}
    }  // no parameters
};

std::shared_ptr<const Game> Factory(const GameParameters& params) {
  return std::shared_ptr<const Game>(new AbaloneGame(params));
}

REGISTER_SPIEL_GAME(kGameType, Factory);

RegisterSingleTensorObserver single_tensor(kGameType.short_name);

}  // namespace

CellState PlayerToState(Player player) {
  switch (player) {
    case kInvalidPlayer:  return CellState::kInvalid;
    case 0:               return CellState::kPlayer1;
    case 1:               return CellState::kPlayer2;
    default:
      SpielFatalError(absl::StrCat("Invalid player id ", player));
      return CellState::kEmpty;
  }
}

std::string StateToString(CellState state) {
  switch (state)
  {
  case CellState::kInvalid:      return " ";
  case CellState::kEmpty:        return ".";
  case CellState::kPlayer1:      return "1";
  case CellState::kPlayer2:      return "2";
  default:
    SpielFatalError("Unknown state.");
  }
}


/**
* Valid single Move have m_end-m_start == Offset[m_direction]. 
* For slide moves 1 <= length(end-start) <= 2 and slide selection is on left of move direction
*/
struct Move {
	Direction m_direction;
	Coordinate m_start;
	Coordinate m_end;

	bool operator == (const Move& other) const {
		return this->m_direction == other.m_direction &&
			this->m_start == other.m_start &&
			this->m_end == other.m_end;
	}

	bool _IsValidSlide(const AbaloneState& _state) const
	{
		auto player = _state.BoardAt(m_start.m_row, m_start.m_column);
		if (player == CellState::kEmpty || player == CellState::kInvalid)
			return false;
		auto rSize = std::abs(m_end.m_row - m_start.m_row) + 1;
		auto cSize = std::abs(m_end.m_column - m_start.m_column) + 1;
		if(rSize>3)
			return false;
		if (cSize>3)
			return false;
		auto slide_line = std::max(std::min(m_end.m_row - m_start.m_row, 1), -1);
		auto slide_column = std::max(std::min(m_end.m_column - m_start.m_column, 1), -1);
		auto valid_direction = false;
		for(auto dir=Direction::Direction_First; dir<Direction::Direction_Last; dir=Direction(dir+1))
			if(Offsets[dir].m_row==slide_line && Offsets[dir].m_column== slide_column)
				valid_direction = true;
		if (!valid_direction)
			return false;
		auto direction = Offsets[m_direction];
		auto current_row = m_start.m_row;
		auto current_column = m_start.m_column;
		auto size = std::max(cSize, rSize);
		for(auto i=0; i<size; ++i)
		{
			if (_state.BoardAt(current_row, current_column) != player)
				return false;
			auto destination_row = current_row + direction.m_row;
			auto destination_column = current_column + direction.m_column;
			if (destination_row < 0 || destination_row >= kNumRows)
				return false;
			if (destination_column  < 0 || destination_column >= kNumCols)
				return false;
			if (_state.BoardAt(destination_row, destination_column) != CellState::kEmpty)
				return false;
			current_row += slide_line;
			current_column += slide_column;
		}

		return true;
	}

	bool IsValid(const AbaloneState& _state) const
	{
		if (m_start.m_row < 0 || m_start.m_row >= kNumRows)
			return false;
		if (m_start.m_column < 0 || m_start.m_column >= kNumCols)
			return false;
		if (m_end.m_row < 0 || m_end.m_row >= kNumRows)
			return false;
		if (m_end.m_column < 0 || m_end.m_column >= kNumCols)
			return false;

		auto current_row = m_start.m_row;
		auto current_column = m_start.m_column;
		auto player = _state.BoardAt(current_row, current_column);
    if (player != PlayerToState(_state.CurrentPlayer()))
        return false;
		// if (player == CellState::kInvalid)
		// 	return false;
		// if (player == CellState::kEmpty)
		// 	return false;

		auto vectorR = m_end.m_row - m_start.m_row;
		auto vectorC = m_end.m_column - m_start.m_column;
		auto direction = Offsets[m_direction];
		if (direction.m_row != vectorR || direction.m_column != vectorC)
			return _IsValidSlide(_state);

		// check in-row moves
		constexpr int line_size = 6;
		CellState line_content[line_size];
		for (auto i = 0; i < line_size; ++i)
		{
			if (current_row >= 0 && current_row < kNumRows &&
				current_column >= 0 && current_column < kNumCols)
			{
				line_content[i] = _state.BoardAt(current_row, current_column);
			}
			else
			{
				line_content[i] = CellState::kInvalid;
			}
			current_row += vectorR;
			current_column += vectorC;
		}

		auto opponent = player == CellState::kPlayer1 ? CellState::kPlayer2 : CellState::kPlayer1;

		if (line_content[1] == CellState::kEmpty)
			return true;
		//if (line_content[1] == CellState::kInvalid)
		//	return false;
		//if (line_content[1] == opponent)
		//	return false;

		if (line_content[1] == player && line_content[2] == CellState::kEmpty)
			return true;
		//if (line_content[1] == player && line_content[2] == CellState::kInvalid)
		//	return false;

		if (line_content[1] == player && line_content[2] == opponent && (line_content[3] == CellState::kInvalid || line_content[3] == CellState::kEmpty))  // 2VS1
			return true;

		if (line_content[1] == player && line_content[2] == player && line_content[3] == CellState::kEmpty)
			return true;
		//if (line_content[1] == player && line_content[2] == player && line_content[3] == CellState::kInvalid)
		//	return false;

		if (line_content[1] == player && line_content[2] == player && line_content[3] == opponent && (line_content[4] == CellState::kInvalid || line_content[4] == CellState::kEmpty))  // 3VS1
			return true;

		if (line_content[1] == player &&
			line_content[2] == player &&
			line_content[3] == opponent &&
			line_content[4] == opponent &&
			(line_content[5] == CellState::kInvalid || line_content[5] == CellState::kEmpty)
			)  // 3VS2
			return true;

		return false;
	}

	/**
	@param _dr _dc offsets from move direction
	*/
	inline void _ApplyParallelMove(AbaloneState& _state, int _dr, int _dc) const
	{
		auto slide_row = std::max(std::min(m_end.m_row - m_start.m_row, 1), -1);
		auto slide_column = std::max(std::min(m_end.m_column - m_start.m_column, 1), -1);
		auto rSize = std::abs(m_end.m_row - m_start.m_row);
		auto cSize = std::abs(m_end.m_column - m_start.m_column);
		auto size = std::max(rSize, cSize);
		auto r = m_start.m_row;
		auto c = m_start.m_column;
		auto player = _state.BoardAt(r, c);
		for (int i = 0; i <= size; i++)
		{
			if (r < 0 || r >= kNumRows || c < 0 || c >= kNumCols)
				break;
			auto id = _state.BoardAt(r, c);
			if (id != player)
				break;
			auto dst_r = r + _dr;
			auto dst_c = c + _dc;
			if (dst_r < 0 || dst_r >= kNumRows || dst_c < 0 || dst_c >= kNumCols)
				break;
			auto nextId = _state.BoardAt(dst_r, dst_c);
			if (nextId != CellState::kEmpty)  // we could slide on kEmpty cells and own cells
				break;
			_state.SetBoard(dst_r, dst_c, player);
			_state.SetBoard(r, c, CellState::kEmpty);
			r += slide_row;
			c += slide_column;
		}
	}

	inline void _ApplySingleMove(AbaloneState& _state, int _dr, int _dc) const
	{
		auto r = m_start.m_row;
		auto c = m_start.m_column;
		auto nextId = CellState::kEmpty;
		while (r >= 0 && r < kNumRows && c >= 0 && c < kNumCols) // obvious
		{
			auto currentId = _state.BoardAt(r, c);
			if (currentId == CellState::kInvalid)
				break;
			_state.SetBoard(r, c, nextId);
			if (currentId == CellState::kEmpty)
				break;
			nextId = currentId;
			r += _dr;
			c += _dc;
		}
	}

	void Apply(AbaloneState& _state) const
	{
		auto offset = Offsets[m_direction];
		auto vr = m_end.m_row - m_start.m_row;
		auto vc = m_end.m_column - m_start.m_column;
			
		if (offset.m_row != vr || offset.m_column != vc)
		{
			_ApplyParallelMove(_state, offset.m_row, offset.m_column);
		}
		else
		{
			_ApplySingleMove(_state, offset.m_row, offset.m_column);
		}
		// _state.m_turn += 1;
		// auto eval = Eval(_state);
		// _state.m_done = std::get<0>(eval);
		// _state.m_winner = std::get<1>(eval);
	}

	static std::tuple<bool, Move> from_string(const std::string& _str)
	{
    auto local_str = _str;
		if (local_str.length() != 4 && local_str.length() != 6)
			return std::make_tuple(false, Move());
		for (int i = 0; i < local_str.length(); ++i)
			local_str[i] = tolower(local_str[i]);
		int rowIndex = (kNumRows-1) - (local_str[0] - 'a');  // rows are stored in invert order
		int colIndex = local_str[1] - '1';
		if (rowIndex<0 || rowIndex >= kNumRows || colIndex<0 || colIndex >= kNumCols)
			return std::make_tuple(false, Move());
		auto start = Coordinate{ rowIndex, colIndex };
		rowIndex = (kNumRows-1) - (local_str[2] - 'a');
		colIndex = local_str[3] - '1';
		if (rowIndex<0 || rowIndex >= kNumRows || colIndex<0 || colIndex >= kNumCols)
			return std::make_tuple(false, Move());
		auto end = Coordinate{ rowIndex, colIndex };
    //
		// inline move
    //
		if (local_str.length() == 4)
		{
			auto dir = Direction_Invalid;
			auto vl = end.m_row - start.m_row;
			auto vc = end.m_column - start.m_column;
			for (auto dir = Direction::Direction_First; dir < Direction::Direction_Last; dir=Direction(dir+1))
			{
				auto offset = Offsets[dir];
				if (vl == offset.m_row && vc == offset.m_column)
				{
					return std::make_tuple(true, Move{ dir, start, end });
				}
			}
			return std::make_tuple(false, Move());
		}

    //
		// slide move
    //
		auto end_slide = end;
		auto slide_row = end_slide.m_row - start.m_row;
		auto slide_column = end_slide.m_column - start.m_column;
		if (std::max(std::abs(slide_row), std::abs(slide_column)) > 2)
			return std::make_tuple(false, Move());

    // clip vector to match sister's entry
    slide_row = std::max(std::min(slide_row, 1), -1);
		slide_column = std::max(std::min(slide_column, 1), -1);
		auto slide_vec = Coordinate{ slide_row, slide_column };

		rowIndex = (kNumRows-1) - (local_str[4] - 'a');
		colIndex = local_str[5] - '1';
		if (rowIndex<0 || rowIndex >= kNumRows || colIndex<0 || colIndex >= kNumCols)
			return std::make_tuple(false, Move());

		end = Coordinate{ rowIndex, colIndex };
		auto move_vec = Coordinate{ end.m_row - start.m_row, end.m_column - start.m_column};
    
		for (auto dir = Direction::Direction_First; dir < Direction::Direction_Last; dir = Direction(dir + 1))
		{
			auto offset = Offsets[dir];
			if (move_vec.m_row == offset.m_row && move_vec.m_column == offset.m_column)
			{
				// we suppose slide on left (dir+N) of move direction: 
				if(Offsets[Sisters[dir].first] == slide_vec || Offsets[Sisters[dir].second] == slide_vec)
					return std::make_tuple(true, Move{ dir, start, end_slide });
				// we have to swap start / end
				return std::make_tuple(true, Move{ dir, end_slide, start});
			}
		}

		return std::make_tuple(false, Move());
	}

	std::string to_string() const
	{
		auto offset = Offsets[m_direction];
		auto vl = m_end.m_row - m_start.m_row;
		auto vc = m_end.m_column - m_start.m_column;
		std::string ret;
		if (offset.m_row != vl || offset.m_column != vc)
		{
			char buff[100];
			snprintf(
				buff, sizeof(buff),
				"%c%c%c%c%c%c",
#if ORIGIN_BOTTOM
				char('a' + ((kNumRows-1) - this->m_start.m_row)),
				char('1' + this->m_start.m_column),
				char('a' + ((kNumRows-1) - this->m_end.m_row)),
				char('1' + this->m_end.m_column),
				char('a' + ((kNumRows-1) - (this->m_start.m_row + offset.m_row))),
				char('1' + this->m_start.m_column + offset.m_column)
#else
				char('a' + this->m_start.m_row),
				char('1' + this->m_start.m_column),
				char('a' + this->m_end.m_row),
				char('1' + this->m_end.m_column),
				char('a' + (this->m_start.m_row + offset.m_row)),
				char('1' + this->m_start.m_column + offset.m_column)
#endif //ORIGIN_BOTTOM
				);
			ret = std::string(buff);

		}
		else
		{
			char buff[100];
			snprintf(
				buff, sizeof(buff),
				"%c%c%c%c",
#if ORIGIN_BOTTOM
				char('a' + ((kNumRows-1) - this->m_start.m_row)),
				char('1' + this->m_start.m_column),
				char('a' + ((kNumRows-1) - this->m_end.m_row)),
				char('1' + this->m_end.m_column)
#else
				char('a' + this->m_start.m_row),
				char('1' + this->m_start.m_column),
				char('a' + this->m_end.m_row),
				char('1' + this->m_end.m_column)
#endif //ORIGIN_BOTTOM
			);
			ret = std::string(buff);
		}
		return ret;
	}
};

Move ActionToMove(Action moveId)
{
	auto remains = moveId;
	auto moveType = remains % kNumActionsPerDirection;
	remains /= kNumActionsPerDirection;
	Direction dir = Direction(remains % Direction::Direction_Last);
	remains /= Direction::Direction_Last;
	auto column = remains % kNumCols;
	remains /= kNumCols;
	auto row = remains;
	Move move;
	move.m_direction = dir;
	move.m_start.m_row = row;
	move.m_start.m_column = column;
	auto offset = Offsets[dir];
	switch (moveType)
	{
	case 0:  // single move
	{
		move.m_end.m_row = row + offset.m_row;
		move.m_end.m_column = column + offset.m_column;
	}
	break;
	case 1:  // slideX2 right front
	{
		auto slide = Offsets[(dir + 1) % Direction::Direction_Last];
		move.m_end.m_row = row + slide.m_row;
		move.m_end.m_column = column + slide.m_column;
	}
	break;
	case 2:  // slideX2 right back
	{
		auto slide = Offsets[(dir + 2) % Direction::Direction_Last];
		move.m_end.m_row = row + slide.m_row;
		move.m_end.m_column = column + slide.m_column;
	}
	break;
	case 3:  // slideX3 right front
	{
		auto slide = Offsets[(dir + 1) % Direction::Direction_Last];
		move.m_end.m_row = row + 2 * slide.m_row;
		move.m_end.m_column = column + 2 * slide.m_column;

	}
	break;
	case 4:  // slideX3 right back
	{
		auto slide = Offsets[(dir + 2) % Direction::Direction_Last];
		move.m_end.m_row = row + 2 * slide.m_row;
		move.m_end.m_column = column + 2 * slide.m_column;

	}
	break;
	}
	return move;
}

Action MoveToAction(const Move& move)
{
	auto result = move.m_start.m_row;
	result *= kNumCols;
	result += move.m_start.m_column;
	result *= Direction::Direction_Last;
	result += move.m_direction;
	result *= 5;

	auto offset = Offsets[move.m_direction];
	// test slide move
	if (move.m_start.m_row + offset.m_row != move.m_end.m_row ||
		move.m_start.m_column + offset.m_column != move.m_end.m_column)
	{
		// look for direction
		auto slideF = Offsets[Sisters[move.m_direction].first];
		auto slideB = Offsets[Sisters[move.m_direction].second];

		if (move.m_start.m_row + slideF.m_row == move.m_end.m_row &&
			move.m_start.m_column + slideF.m_column == move.m_end.m_column)
		{
			result += 1;
		}
		else if (move.m_start.m_row + slideB.m_row == move.m_end.m_row &&
			move.m_start.m_column + slideB.m_column == move.m_end.m_column)
		{
			result += 2;
		}
		else if (move.m_start.m_row + slideF.m_row + slideF.m_row == move.m_end.m_row &&
			move.m_start.m_column + slideF.m_column + slideF.m_column == move.m_end.m_column)
		{
			result += 3;
		}
		else if (move.m_start.m_row + slideB.m_row + slideB.m_row == move.m_end.m_row &&
			move.m_start.m_column + slideB.m_column + slideB.m_column == move.m_end.m_column)
		{
			result += 4;
		}
	}
		
	return static_cast<Action>(result);
}

void AbaloneState::UndoAction(Player player, Action action) {
  // We don't have direct undo functionality, we try to apply whole history
  history_.pop_back();
  --move_number_;
  ResetBoard();
  for (auto [_, action] : history_) {
    DoApplyAction(action);
  }
}

void AbaloneState::DoApplyAction(Action action) {
  //SPIEL_CHECK_EQ(board_[move], CellState::kEmpty);

  // update board state
  Move move = ActionToMove(action);
  if(move.IsValid(*this)) {
    move.Apply(*this);
  }
  else {
    outcome_ = 1 - current_player_;
    return;
  }
  // moveStruct.Apply();
  //board_[move] = PlayerToState(CurrentPlayer());

  // test game winner
  // if (HasLine(current_player_)) {
  //   outcome_ = current_player_;
  // }
  auto returns = Returns();
  if(returns[current_player_]==1.0){
    outcome_ = current_player_;
  }
  // update step
  current_player_ = 1 - current_player_;
  // num_moves_ += 1;
}

std::vector<Action> AbaloneState::LegalActions() const {
  if (IsTerminal()) return {};
  // Can move in any empty cell.
  std::vector<Action> moves;
  for (auto i = 0; i < kNumCells*kNumActionsPerCell; ++i) {
    auto move = ActionToMove(i);
    if (move.IsValid(*this)) {
      moves.push_back(i);
    }
  }
  return moves;
}

std::string AbaloneState::ActionToString(Player player,
                                           Action action_id) const {
  return game_->ActionToString(player, action_id);
}

void AbaloneState::ResetBoard(){
  //std::fill(begin(board_), end(board_), CellState::kEmpty);
  const auto &up_game = static_cast<const AbaloneGame&>(*this->game_);
  auto init_board = ABALONE_INIT_CLASSIC;
  if(up_game.m_init_board.compare("classic")==0)
  {
    init_board = ABALONE_INIT_CLASSIC;
  }
  else if(up_game.m_init_board.compare("belgian_daisy")==0)
  {
    init_board = ABALONE_INIT_BELGIAN_DAISY;
  } else {
    SPIEL_CHECK_TRUE(false);  // board init not found
  }
  

  auto invert_board = [invert=up_game.m_init_invert](CellState c){
	if(invert && c==CellState::kPlayer1)
		return CellState::kPlayer2;
	if(invert && c==CellState::kPlayer2)
		return CellState::kPlayer1;
	return c;
  };
  
  for(int r=0; r<kNumRows; r++)
  {
    for(int c=0; c<kNumCols; c++)
    {
      SetBoard(r, c, invert_board(init_board[r][c]));
    }
  }

  current_player_ = 0;
  outcome_ = kInvalidPlayer;
}

/*Action AbaloneState::StringToAction(Player player, const std::string& action_str) const {
  const auto &up_game = static_cast<const AbaloneGame&>(*GetGame());
  return up_game.StringToAction(player, action_str);
}*/

AbaloneState::AbaloneState(std::shared_ptr<const Game> game) : State(game) {
  ResetBoard();
}

std::string AbaloneState::ToString() const {
  std::string str;
  absl::StrAppend(&str, "board = \n");
  auto display_line = [&](std::string prefix, int line, int start, int end, std::string postfix)
  {
    absl::StrAppend(&str, prefix);
    for (auto i = start; i < end; ++i)
    {
      absl::StrAppend(&str, "   ");
      absl::StrAppend(&str, StateToString(BoardAt(line, i)));
    }
    absl::StrAppend(&str, postfix);
    absl::StrAppend(&str, "\n");
  };
#if ORIGIN_BOTTOM
  display_line("<i>        ", 0, 4, 9, "");
	display_line("<h>      ", 1, 3, 9, "");
	display_line("<g>    ", 2, 2, 9, "");
	display_line("<f>  ", 3, 1, 9, "");
	display_line("<e>", 4, 0, 9, "");
	display_line("<d>  ", 5, 0, 8, "  <9>");
	display_line("<c>    ", 6, 0, 7, "  <8>");
	display_line("<b>      ", 7, 0, 6, "  <7>");
	display_line("<a>        ", 8, 0, 5, "  <6>");
#else
  display_line("<i>        ", 8, 4, 9, "");
	display_line("<h>      ", 7, 3, 9, "");
	display_line("<g>    ", 6, 2, 9, "");
	display_line("<f>  ", 5, 1, 9, "");
	display_line("<e>", 4, 0, 9, "");
	display_line("<d>  ", 3, 0, 8, "  <9>");
	display_line("<c>    ", 2, 0, 7, "  <8>");
	display_line("<b>      ", 1, 0, 6, "  <7>");
	display_line("<a>        ", 0, 0, 5, "  <6>");
#endif

  absl::StrAppend(&str, "               <1> <2> <3> <4> <5>\n");

  absl::StrAppend(&str, "move_number_ = ");
  absl::StrAppend(&str, move_number_);
  absl::StrAppend(&str, "\n");

  auto returns = Returns();
  absl::StrAppend(&str, "returns = ");
  absl::StrAppend(&str, returns[0]);
  absl::StrAppend(&str, ", ");
  absl::StrAppend(&str, returns[1]);
  absl::StrAppend(&str, "\n");

  absl::StrAppend(&str, "winner = ");
  absl::StrAppend(&str, StateToString(PlayerToState(outcome_)));
  absl::StrAppend(&str, "\n");

  absl::StrAppend(&str, "done = ");
  absl::StrAppend(&str, IsTerminal());
  absl::StrAppend(&str, "\n");

  return str;
}

bool AbaloneState::IsTerminal() const {
  return outcome_ != kInvalidPlayer || move_number_ >= kHistoryMax;
}

// std::vector<double> AbaloneState::Rewards() const {
// }

std::vector<double> AbaloneState::Returns() const {
  if (outcome_!=kInvalidPlayer)  // set by invalid move
  {
    if (outcome_==0)
      return {1.0, -1.0};
    if (outcome_==1)
      return {-1.0, 1.0};
  }
  int ballCount[2] = { 0, 0 };
  for (int line = 0; line < kNumRows; ++line)
  {
    for (int column = 0; column < kNumCols; ++column)
    {
      auto slot = BoardAt(line, column);
      if (slot == CellState::kPlayer1)
      {
        ballCount[0]++;
      }
      else if (slot == CellState::kPlayer2)
      {
        ballCount[1]++;
      }
    }
  }

  const auto &up_game = static_cast<const AbaloneGame&>(*GetGame());
  if (ballCount[0] <= 14 - up_game.m_marbles_to_win)
  {
    return {-1.0, 1.0};
  }
  if (ballCount[1] <= 14 - up_game.m_marbles_to_win)
  {
    return {1.0, -1.0};
    // (ballCount[0]>ballCount[1])      
    // if (ballCount[1]>ballCount[0])      
    // return {0.0, 0.0};
  }
  //return {0.0, 0.0};
  const double marble_reward = up_game.m_marble_reward;
  auto marble_balance = (14-ballCount[1])-(14-ballCount[0]);
  return {marble_balance*marble_reward, -marble_balance*marble_reward};
}

std::string AbaloneState::InformationStateString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);
  return HistoryString();
}

std::string AbaloneState::ObservationString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);
  return ToString();
}

void AbaloneState::ObservationTensor(Player player,
                                       absl::Span<float> values) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);

  // Treat `values` as a 3-d tensor.
  TensorView<3> view(values, {kNumPlayers + 1, kNumRows, kNumCols}, true);

  // encode current player's observation to be at the same layer
  auto player1_index = 0;
  auto player2_index = 0;
  switch(player)
  {
    case CellState::kPlayer1:
      player1_index = 1;
      player2_index = 2;
      break;
    case CellState::kPlayer2:
      player1_index = 2;
      player2_index = 1;
      break;
  }

  for (int row = 0; row < kNumRows; ++row) {
    for (int col = 0; col < kNumCols; ++col) {
      auto index = 0;
      switch(BoardAt(row, col)){
        case CellState::kInvalid:
          continue;
        case CellState::kEmpty:
          index = 0;
          break;
        case CellState::kPlayer1:
          index = player1_index;
          break;
        case CellState::kPlayer2:
          index = player2_index;
          break;
      }
      view[{index, row, col}] = 1.f;
    }
  }
}

std::unique_ptr<State> AbaloneState::Clone() const {
  return std::unique_ptr<State>(new AbaloneState(*this));
}

std::unique_ptr<State> AbaloneGame::NewInitialState() const {
  return std::unique_ptr<State>(new AbaloneState(shared_from_this()));
}

std::string AbaloneGame::ActionToString(Player player,
                                          Action action_id) const {
  auto move = ActionToMove(action_id);
  return absl::StrCat(move.to_string());
}

AbaloneGame::AbaloneGame(const GameParameters& params)
    : Game(kGameType, params) {
  m_marbles_to_win = ParameterValue<int>("marbles_to_win");
  m_marble_reward = ParameterValue<double>("marble_reward");
  m_init_board = ParameterValue<std::string>("board");
  m_init_invert = ParameterValue<bool>("invert"); 
}

/*Action AbaloneGame::StringToAction(Player player, const std::string& action_str) const {
  auto maybe_move = Move::from_string(action_str);
  if(std::get<0>(maybe_move))
  {
    return MoveToAction(std::get<1>(maybe_move));
  }

  SpielFatalError(
      absl::StrCat("Couldn't find an action matching ", action_str));
}*/

}  // namespace abalone
}  // namespace open_spiel
