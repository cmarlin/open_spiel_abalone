#pragma once
#ifndef _ABALONE_CORE_AB_H_
#define _ABALONE_CORE_AB_H_

#include <vector>
#include "abalone_core.h"


namespace abalone_core
{
	/**
	 * @return score relative to _player; range in ]-1000; 1000[
	 */
	int Heuristic(const core_state& _state, CellState _player);

  /**
   * @return best move with associated value (ie current state's value)
   */
	std::pair<core_Action, float> AlphaBeta(const core_state& _state, int _depth, float _alpha = -1.f, float _beta = 1.f, std::vector<std::pair<core_Action, float>>* _all_moves = NULL);
}

#endif  // _ABALONE_CORE_AB_H_