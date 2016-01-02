/* Copyright 2016 Centipede Productions Ltd, All Rights Reserved */

/* Commercial Licence Usage
 * ------------------------
 * You may use this file in accordance with the terms contained in a
 * written agreement between You and Centipede Productions Ltd.
 *
 * GNU General Public License Usage
 * --------------------------------
 * Alternatively, this file may be used under the terms of the GNU General
 * Public License version 3.0 as published by the Free Software Foundation
 * and appearing in the file LICENSE.GPL included in the packaging of this
 * file.  Please visit http://www.gnu.org/copyleft/gpl.html and review the
 * information to ensure the GNU General Public License version 3.0
 * requirements will be met.
 */

#include <list>
#include <utility>

struct flattener_t;

/* public interface to polygon flattener */

class flatten_arrangement {

  public:
	// a = 0 b = 0 => 1
	// a = 1 b = 0 => 2
	// a = 0 b = 1 => 4
	// a = 1 b = 1 => 8

	enum { FLATTEN_A = 0xa,
	       FLATTEN_B = 0xc,
	       FLATTEN_A_OR_B = 0xe,
	       FLATTEN_A_AND_B = 0x8,
	       FLATTEN_A_MINUS_B = 0x2,
	       FLATTEN_B_MINUS_A = 0x4,
	       FLATTEN_A_XOR_B = 0x6,
	       FLATTEN_EMPTY = 0x0
	};

	flatten_arrangement(int flatten_rule = FLATTEN_A);
	~flatten_arrangement();

	void add_edge(int x, int y, int u, int v, bool is_shape_b = false);
	bool flatten();

	typedef std::pair<int, int> point_t;
	typedef std::pair<point_t, point_t> edge_t;

	typedef std::list<point_t> polygon_t;
	typedef std::list<polygon_t> polygon_list_t;

	bool get_result(polygon_list_t& result);

	void print(FILE *);
	bool verify();

  private:
	flattener_t *flattener;
};

/* end */
