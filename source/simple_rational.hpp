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

#if 0

#include "boost/rational.hpp"

/* for debug / comparison. operation< is very slow */

struct rat_t : public boost::rational<int64> {

	rat_t(int64 i_) : boost::rational<int64>(i_) {
	}

	rat_t(int64 i_, int64 n_, int64 d_) : boost::rational<int64>(i_ * d_ + n_, d_) {
	}

	rat_t(boost::rational<int64> const &other) {
		assign(other.numerator(), other.denominator());
	}

	int64 intval() const {
		assert_(denominator());
		return numerator() / denominator();
	}
};

#else

struct rat_t {

	/* the flattener uses a very restricted set of rational
	   operations; just implement the ones we need. The
	   denominator never exceeds n^2, but the numerator can
	   temporarily reach n^3 - so we should be safe for 20 bits */

	int64 i;	/* (signed) integer part */
	int64 n, d;	/* fractional part - always positive and less than one */

	void check() {
		assert_(d > 0);
		assert_(n >= 0 && n < d);
	}

	int64 intval() const {
		return i;
	}

	rat_t(int64 i_) : i(i_), n(0), d(1) {
	}

	rat_t(int64 i_, int64 n_, int64 d_) : i(i_), n(n_), d(d_) {
		normalize(true);
	}

	rat_t operator-(const int64& other) {
		i -= other;
		return *this;
	}

	rat_t operator*(const int64& other) {

		i *= other;
		n *= other;
		normalize(true);
		return *this;
	}

	rat_t times(const int64& other) {

		i *= other;
		n *= other;
		normalize(false);
		return *this;
	}

	bool operator==(const rat_t& other) const {
		return i == other.i && n == other.n && d == other.d;
	}

	bool operator<(const rat_t& other) const {

		if (i != other.i)
			return i < other.i;

		if (d == other.d)
			return n < other.n;

		/* create binary fraction from each rational, and
		   compare bitwise as we go. We know the normalized
		   values differ. For example if one ratio is less
		   than half, and the other is greater, we'll drop out
		   first time round. Profiling shows this to be 25%
		   faster than continued fractions for random data */

		int64 na = n;
		int64 nb = other.n;

		for (;;) {
			bool a = ((na += na) >= d);
			bool b = ((nb += nb) >= other.d);

			if (a != b)
				return b;

			if (a) {
				na -= d;
				nb -= other.d;
			}
		}
	}

	inline int64 gcd(int64 a, int64 b) {

		assert_(a >= 0);
		assert_(b >= 0);

		for (;;) {
			if (a == 0)
				return b;
			b %= a;
			if (b == 0)
				return a;
			a %= b;
		}
	}

	inline void normalize(bool canonical) {

		assert_(d != 0);

		if (n == 0) {
			d = 1;
			return;
		}

		if (d < 0) {
			n = -n;
			d = -d;
		}

		assert_(d > 0);

		i += n / d;
		n %= d;

		if (n < 0) {
			n += d;
			--i;
		}

		if (canonical)
			canonicalize();
	}

	void canonicalize() {

		/* use a canonical representation for the fractional
		   part so that we can compare quicky for equality */

		int64 g = gcd(n, d);

		assert_(g > 0);

		n /= g;
		d /= g;

		check();
	}
};

#endif

/* end */
