/*
 * Flatten a set of possibly self-intersecting polygons, yielding a
 * set of non-intersecting polygons. The points and intersections are
 * robustly snapped to an integer grid.
 */

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

#include <iostream>
using namespace std;

#include <list>
#include <set>
#include <map>

inline void dprintf(const char *, ...)
{
}

// #define dprintf printf

struct assertion_t {
	int flavour;
	assertion_t(int flavour_) : flavour(flavour_) {
	}
};

void assfail()
{
	throw(assertion_t(4));
}

#define assert_(x) do { if (!(x)) { printf("flattener: assertion error line %d: %s\n", __LINE__, #x); assfail(); } } while (0)

// #define assert_(x) do { } while (0)

#define BEGIN_ANON_NAMESPACE namespace {
#define END_NAMESPACE }

using namespace std;

typedef long long int64;

#include "simple_rational.hpp"
#include "flatten_arrangement.h"

BEGIN_ANON_NAMESPACE

int passes, intersections, splits, comparisons;

struct edge_t;
struct vertex_t;

/*
 * the key to it all - compare two edges for their position
 * in the active edge list. If we detect an intersection, throw
 * an exception. Our caller will deal with it and start again.
 */

struct sort_active_edge {
	int operator()(edge_t * const &p, edge_t * const &q) const;
};

/* active edge list is a sorted set of edges */
typedef set<edge_t *, sort_active_edge> active_edge_list_t;


/*
 * winding number for the two shapes we're combining
 */
struct wind_t {
	short a;
	short b;
	wind_t(short a_, short b_) : a(a_), b(b_) {
	}
	wind_t operator+(const wind_t& other) const {
		return wind_t(a + other.a, b + other.b);
	}
	bool is_zero() {
		return a == 0 && b == 0;
	}
	bool is_inside(int flatten_rule) {
		int m = ((a != 0) ? 1 : 0) + ((b != 0) ? 2 : 0);
		return (flatten_rule >> m) & 1;
	}
};


struct line_t {
	int x0, y0;
	int x1, y1;

	line_t(int x0_, int y0_, int x1_, int y1_) : x0(x0_), y0(y0_), x1(x1_), y1(y1_) {
	}

	bool operator==(const line_t& other) const {
		return x0 == other.x0 && y0 == other.y0 && x1 == other.x1 && y1 == other.y1;
	}
};


struct flags_t {
	bool keep: 1;		// edge appears in result set
	bool checked: 1;	// we've already checked whether it appears
	bool active: 1;		// edge is currently in active edge list
	bool sense: 1;		// winding sense
	bool visited: 1;	// appears in an output polygon
	bool todo: 1;		// has entry on todo list
};


struct pin_t {
	vertex_t *v;
	bool above;

	pin_t() : v(0), above(false) {
	}

	pin_t(vertex_t *v_, bool above_) : v(v_), above(above_) {
	}
};


struct pinsort_t {
	vertex_t *from;

	pinsort_t(vertex_t *from_) : from(from_) {
	}

	bool operator()(const pin_t &p, const pin_t &q) const;
};


typedef set<pin_t, pinsort_t> pin_set_t;

struct edge_t {
	/* vertices ordered by y then x - never the same */

	line_t raw; /* underlying line with integer endpoints */

	vertex_t *from; /* possibly rational endpoints of sub-line */
	vertex_t *to;
	wind_t wind;

	wind_t checked_wind;

	union {
		flags_t flags;
		int all_flags;
	};

	pin_set_t *pin_set;

	~edge_t() {
		if (flags.active)
			this->print("flatten: deleting active edge");
		if (flags.todo)
			this->print("flatten: deleting todo edge");
		delete pin_set;
	}

	active_edge_list_t::iterator aelpos;

	edge_t(const line_t& raw_, vertex_t *from_, vertex_t *to_);

	bool equals(const edge_t *other) {
		return other->from == from && other->to == to;
	}

	void print(const char *noun) const;

	void add_pin(vertex_t *p, bool above);

	void snap_to_pins();
};


/*
 * each vertex has a list of the vertices it's connected to above and
 * below. There's one edge structure per vertex pair, referred to from
 * the above list of one vertex and the below list of the other
 */
typedef map<vertex_t *, edge_t *> edgemap_t;


struct vertex_t {

	edgemap_t above;
	edgemap_t below;

	rat_t x, y;
	vertex_t(rat_t& x_, rat_t& y_) : x(x_), y(y_) {
	}

	bool equals(const vertex_t *other) {
		return x == other->x && y == other->y;
	}

	edge_t *goes_to(vertex_t *q, line_t raw, wind_t wind, edge_t *state);

	void print(const char *noun) const {
		dprintf("%s %lld + %lld/%lld, %lld + %lld/%lld\n", noun, x.i, x.n, x.d, y.i, y.n, y.d);
	}

	int64 distance(const vertex_t *other) {
		int64 dx = other->x.i - x.i;
		int64 dy = other->y.i - y.i;
		return dx * dx + dy * dy;
	}
};



bool pinsort_t::operator()(const pin_t &p, const pin_t &q) const
{
	/* sort pins by their integer^2 distance from 'from' */

	return from->distance(p.v) < from->distance(q.v);
}


edge_t::edge_t(const line_t& raw_, vertex_t *from_, vertex_t *to_) : raw(raw_), from(from_), to(to_), wind(0, 0), checked_wind(0, 0)
{
	all_flags = 0;
	pin_set = 0;
}


void edge_t::print(const char *noun) const
{
	from->print(noun);
	to->print(" .. ");
	cout << " " << wind.a << " " << wind.b << " " << this << endl;
}

/*
 * orderering function for vertex set -
 * left to right, top to bottom
 */
struct vertexsort_t {
	int operator()(vertex_t * const &p, vertex_t * const &q) const {
		if (p->y == q->y)
			return p->x < q->x;
		return p->y < q->y;
	}
};


/*
 * all the vertices in sweepline order
 */
typedef set<vertex_t *, vertexsort_t> vertexset_t;


/*
 * simple numeric range helper
 */
struct range_t {
	rat_t l, r;
	range_t(rat_t l_, rat_t r_) : l(l_), r(r_) {
		if (r < l) {
			r = l_;
			l = r_;
		}
	}
	bool overlaps(const range_t& other) {
		if (other.r < l || r < other.l)
			return false;
		return true;
	}
};


/* 3-wayed return; which side of edge is vertex ? */

int side(vertex_t * const &v, edge_t * const &p)
{
	/* use the underlying edge */

	int64 x1 = p->raw.x0;
	int64 y1 = p->raw.y0;
	int64 x2 = p->raw.x1;
	int64 y2 = p->raw.y1;

	rat_t x0 = v->x;
	rat_t y0 = v->y;

	/* rational has terms to n^2 so this is n^3 */

	rat_t a = (y0 - y1) * (x1 - x2);
	rat_t b = (x0 - x1) * (y1 - y2);

	if (a == b) return 0;
	if (a < b) return -1;
	return 1;
}



int side(vertex_t * const &v, line_t const &p)
{
	/* use the underlying edge */

	int64 x1 = p.x0;
	int64 y1 = p.y0;
	int64 x2 = p.x1;
	int64 y2 = p.y1;

	int64 x0 = v->x.i;
	int64 y0 = v->y.i;

	int64 a = (y0 - y1) * (x1 - x2);
	int64 b = (x0 - x1) * (y1 - y2);

	if (a == b) return 0;
	if (a < b) return -1;
	return 1;
}


/* fold point to first quadrant */

int quadrant(int64& dx, int64& dy)
{
	assert_(dx != 0 || dy != 0);

	int n = 0;
	while (! (dx > 0 && dy >= 0)) {
		int64 t = -dx;
		dx = dy;
		dy = t;
		n++;
	}
	return n;
}


/* sort edges by angle, as if 'from' end were at origin */

struct edgesort_t {
	int operator()(edge_t const &p, edge_t const &q) const {

		/* sort underlying edges */

		int64 px = p.raw.x1 - p.raw.x0;
		int64 py = p.raw.y1 - p.raw.y0;
		int64 qx = q.raw.x1 - q.raw.x0;
		int64 qy = q.raw.y1 - q.raw.y0;

		if (px == qx && py == qy)
			return 0;

		int pp = quadrant(px, py);
		int qq = quadrant(qx, qy);

		if (pp != qq)
			return pp < qq;

		/* same angle ? shouldn't get here -
		   intersect handler should have noticed */

		assert_(px * qy != qx * py);

		return px * qy > qx * py;
	}
};


/*
 * remove an edge from the edgelists which reference it
 */
static void unlink(edge_t * const &p)
{
	p->from->below.erase(p->to);
	p->to->above.erase(p->from);
}


/*
 * remove an edge completely
 */
static void remove(edge_t * const &p)
{
	unlink(p);

	assert_(p->flags.active == false);
	assert_(p->flags.todo == false);

	delete p;
}


void edge_t::add_pin(vertex_t *p, bool above)
{
	assert_(p->x.n == 0 && p->y.n == 0);
	if (pin_set == 0)
		pin_set = new pin_set_t(from);
	pin_set->insert(pin_t(p, above));
}


/*
 * walk all the pins for this edge (there will be at least two,
 * representing its endpoints) and reroute the edge via the pins
 * so that edge above and below pin relationships are maintained
 */
void edge_t::snap_to_pins()
{
	pin_t pin_a;
	pin_t pin_b;
	list<pin_t> pinlist;
	int pincount = 0;

	/* order left to right is list_back..list_front pin_b pin_a */

	for (pin_set_t::iterator it = pin_set->begin(); it != pin_set->end(); it++) {

		while (pincount >= 2) {

			/* does adding this pin create a right turn on the
			   previous 'below' pin (or left...above) ? Trim the
			   ear */

			int a = side(it->v, line_t(pin_b.v->x.i, pin_b.v->y.i, pin_a.v->x.i, pin_a.v->y.i));

			if (a == 0 || (a < 0) == it->above)
				break;

			pin_a = pin_b;
			if (! pinlist.empty()) {
				pin_b = *pinlist.begin();
				pinlist.pop_front();
			}

			pincount--;
		}

		if (pincount >= 2)
			pinlist.push_front(pin_b);

		pin_b = pin_a;
		pin_a = *it;

		pincount++;
	}

	while (pincount >= 2) {
		edge_t *e = pin_b.v->goes_to(pin_a.v, this->raw, this->wind, this);
		pin_a = pin_b;

		if (e->wind.is_zero())
			remove(e);

		if (! pinlist.empty()) {
			pin_b = *pinlist.begin();
			pinlist.pop_front();
		}
		pincount--;
	}
}


/*
 * create a new edge this..q and add it to the appropriate
 * above and below lists. If the edge already exists just
 * combine in the winding number of this new edge.
 */
edge_t *vertex_t::goes_to(vertex_t *q, line_t raw, wind_t wind, edge_t *state)
{
	if (q == this)
		return 0;

	bool flip = false;

	edge_t *e;
	if (vertexsort_t()(q, this)) {

		wind = wind_t(-wind.a, -wind.b);
		raw = line_t(raw.x1, raw.y1, raw.x0, raw.y0);

		if ((e = above[q]) == 0)
			e = above[q] = q->below[this] = new edge_t(raw, q, this);

		flip = true;
	}
	else {
		if ((e = below[q]) == 0)
			e = below[q] = q->above[this] = new edge_t(raw, this, q);
	}

	/* if we have two edges with the same
	   endpoints, combine their winding rules */

	e->wind = e->wind + wind;

	/* maybe inherit another edge's state if we're splitting that edge */

	if (state) {
		e->flags.checked = state->flags.checked;
		e->flags.sense = state->flags.sense ^ flip;
		e->flags.keep = state->flags.keep;	
	}

	return e;
}


/*
 * exception thrown when we detect an intersection
 */
struct action_t {
	edge_t *p;
	edge_t *q;
	vertex_t *v;
	enum flavour { split, intersect } f;
	action_t(edge_t *p_, edge_t *q_, vertex_t *v_, flavour f_) : p(p_), q(q_), v(v_), f(f_) {
	}
};

/*
 * comparison function for edges in active edge list. Left-to-right
 * ordering according to a sweep line. Throw an exception if we get
 * two edges which cross or touch.
 */
int sort_active_edge::operator()(edge_t * const&p, edge_t * const &q) const
{
	comparisons++;

	if (p == q)
		return 0;

	/* if we have two edges which differ only in their winding rule
	   they should already have been combined */

	assert_(! p->equals(q));

	if (! range_t(p->from->x, p->to->x).overlaps(range_t(q->from->x, q->to->x)))
		return p->from->x < q->from->x; /* no x overlap */

	/* must overlap in y - this is active edge sweep */
	assert_(range_t(p->from->y, p->to->y).overlaps(range_t(q->from->y, q->to->y)));

	/* adding an inactive edge to ael */
	assert_(p->flags.active || q->flags.active);

	/* vertex is removed before insert */
	assert_(p->from != q->to);
	assert_(p->to != q->from);

	if (p->from == q->from) { /* ^ */
		if (side(q->to, p) != 0)
			return (edgesort_t()(*q, *p));
		/* same top point and collinear but not identical */
		if (vertexsort_t()(p->to, q->to))
			throw(action_t(q, p, p->to, action_t::split));
		throw(action_t(p, q, q->to, action_t::split));
	}

	if (p->to == q->to) { /* v */
		if (side(q->from, p) != 0)
			return (edgesort_t()(*p, *q));
		/* same bottom point and collinear but not identical */
		if (vertexsort_t()(p->from, q->from))
			throw(action_t(p, q, q->from, action_t::split));
		throw(action_t(q, p, p->from, action_t::split));
	}

	/* overlap in x and y - do they cross ? */

	/* note that we only need the sign of the determinant,
	   so these calculations are exact */

	int a = side(p->from, q);
	int b = side(p->to, q);

	if (a * b > 0) {
		/* both the same sign and not zero. Looking
		   down edge p from the top to the bottom, are
		   the points on edge q to the left or the
		   right ? */

		return a < 0;
	}

	int c = side(q->from, p);
	int d = side(q->to, p);

	if (c * d > 0)
		return c > 0;

	if (a == 0 && b == 0) {
		assert_(c == 0 && d == 0);

		/* lines are collinear and we know they
		   overlap; each edge is split at one of the
		   other edge's endpoints. Just do one now
		   and catch the other the next time around.
		   All the point are in the vertex set which
		   gives us an ordering */

		if (vertexsort_t()(p->from, q->from))
			throw(action_t(p, q, q->from, action_t::split)); /* split p at q->from */

		throw(action_t(q, p, p->from, action_t::split));
	}

	/* final case, full intersection */

	throw(action_t(p, q, 0, action_t::intersect));
}


void walklist(active_edge_list_t &ael, int flatten_rule)
{
	wind_t wind(0, 0);

	assert_(! wind.is_inside(flatten_rule));

	for (active_edge_list_t::iterator it = ael.begin(); it != ael.end(); it++) {

		edge_t *e = *it;

		assert_(e->flags.active);

		wind_t new_wind = wind + e->wind;

		if (e->flags.checked) {
			assert_(new_wind.a == e->checked_wind.a);
			assert_(new_wind.b == e->checked_wind.b);
			wind = new_wind;
			continue;
		}

		/* for all operations we must be inside at least one of the shapes. An
		   edge is an active edge if we were previously logically outside and
		   now we are inside, or vice versa */

		bool a = wind.is_inside(flatten_rule);
		bool b = new_wind.is_inside(flatten_rule);

		if (a ^ b) {
			assert_(e->flags.checked == false || e->flags.keep == true);
			e->flags.keep = true;
			/* when we're tracing edges it's important to
			   keep the zero side edge on the same side */
			e->flags.sense = b;
		}
		else {
			assert_(e->flags.checked == false || e->flags.keep == false);
			e->flags.keep = false;
		}
		e->flags.checked = true;
		e->checked_wind = new_wind;

		wind = new_wind;
	}
	assert_(wind.is_zero());
}


inline void cull(edgemap_t& el)
{
	/* strip out edges that we don't need, from both above and
	   below sets.  Every edge appears in both sets, so we only
	   need to walk one of them. */

	for (edgemap_t::iterator it = el.begin(); it != el.end(); /* it++ */) {

		edge_t *e = (it++)->second;
		if (e->flags.keep == false)
			remove(e);
	}
}


inline void fold(vertex_t *v)
{
	/* delete any vertex which has only a single edge
	   (with the same raw data) passing through it */

	if (v->above.size() != 1 || v->below.size() != 1)
		return;

	edge_t *a = v->above.begin()->second;
	edge_t *b = v->below.begin()->second;

	if (a->raw == b->raw && a->flags.sense == b->flags.sense) {
		a->from->goes_to(b->to, a->raw, a->wind, a);
		remove(a);
		remove(b);
	}
}


END_NAMESPACE

/*
 * We process vertex by vertex. The todo list contains only
 * the edges which originate from the current vertex (dot).
 * The same edge shouldn't appear more than once on the todo
 * list.
 */

struct flattener_t {

	vertexset_t vertexset;
	int flatten_rule;

	vertex_t *dot;		// the vertex we're currently traversing
	list<edge_t *> todo;	// the edges we need to insert at this vertex

	active_edge_list_t ael;

	flattener_t(int flatten_rule_) : flatten_rule(flatten_rule_) {
	}

	~flattener_t() {
		set<edge_t *>all_edges;

		/* find all the remaining vertices and edges, and delete them */

		for (vertexset_t::iterator it = vertexset.begin(); it != vertexset.end(); it++) {
			for (edgemap_t::iterator et = (*it)->above.begin(); et != (*it)->above.end(); et++) {
				all_edges.insert(et->second);
			}
		}
		for (vertexset_t::iterator it = snapset.begin(); it != snapset.end(); it++) {
			for (edgemap_t::iterator et = (*it)->above.begin(); et != (*it)->above.end(); et++) {
				all_edges.insert(et->second);
			}
		}
		for (set<edge_t *>::iterator it = all_edges.begin(); it != all_edges.end(); it++)
			delete *it;
		for (vertexset_t::iterator it = vertexset.begin(); it != vertexset.end(); it++)
			delete *it;
		for (vertexset_t::iterator it = snapset.begin(); it != snapset.end(); it++)
			delete *it;
	}

	vertex_t *find(rat_t x, rat_t y) {
		vertex_t *v = new vertex_t(x, y);

		pair<vertexset_t::iterator, bool> r = vertexset.insert(v);
 
		if (r.second == false)
			delete v;

		return *r.first;
	}

	void add_edge(int x, int y, int u, int v, bool is_shape_b) {

		vertex_t *p = find(x, y);
		vertex_t *q = find(u, v);

		line_t raw(x, y, u, v);

		p->goes_to(q, raw, wind_t(! is_shape_b, is_shape_b), 0);
	}

	void push(edge_t *e) {

		assert_(e != 0);

		/* only want edges which span the current point */

		if (dot == e->to || vertexsort_t()(e->to, dot))
			return;

		/* push an edge onto the todo list */

		assert_(e->from != e->to);

		if (e->flags.todo == true)
			return;

		if (vertexsort_t()(dot, e->from)) {
			assert_(e->flags.checked == false);
			return;
		}

		e->flags.checked = false;
		e->flags.todo = true;
		todo.push_front(e);
	}

	/*
	 * split an edge at a vertex, creating two new edges
	 */
	void split(edge_t * const &p, vertex_t * const &v) {

		assert_(p->flags.active == false);
		assert_(p->flags.todo == false);

		if (v == p->from || v == p->to) {
			push(p->from->goes_to(p->to, p->raw, p->wind, p));
			return;
		}

		assert_(dot == v || vertexsort_t()(dot, v)); /* not processed v yet */
		assert_(vertexsort_t()(p->from, v));
		assert_(vertexsort_t()(v, p->to));

		push(p->from->goes_to(v, p->raw, p->wind, p));
		push(v->goes_to(p->to, p->raw, p->wind, 0));
	}

	void intersect(edge_t * const &p, edge_t * const &q) {

		/* intersect the underlying edges to give a rational result */

		int64 x1 = p->raw.x0;
		int64 x2 = p->raw.x1;
		int64 x3 = q->raw.x0;
		int64 x4 = q->raw.x1;

		int64 y1 = p->raw.y0;
		int64 y2 = p->raw.y1;
		int64 y3 = q->raw.y0;
		int64 y4 = q->raw.y1;

		int64 un = (x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3);
		int64 ud = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);

		vertex_t *v = find(rat_t(x1, (x2 - x1) * un, ud),
				   rat_t(y1, (y2 - y1) * un, ud));

		split(p, v);
		split(q, v);
	}

	/*
	 * attempt to insert a new edge into the active edge list. The
	 * sort function will have to compare the new edge with its
	 * immediate left and right neighbours, and it will throw an
	 * exception if it detects an intersection.
	 */

	void insert(edge_t *e) {

		assert_(e);

		if (e->flags.active)
			return;

		try {
			e->aelpos = ael.insert(e).first;
			e->flags.active = true; /* only if no exception */
		}
		catch(action_t& i) {

			assert_(e->flags.active == false);

			/* the insert failed, and the list is
			   unchanged. The edge we're inserting
			   collided with an active edge; remove the
			   active edge, find the intersection, and
			   queue two, three or four new edges */

			assert_(e == i.p || e == i.q);
			assert_(i.p != i.q);

			if (i.p->flags.active) {
				ael.erase(i.p->aelpos);
				i.p->flags.active = false;
			}
			unlink(i.p);
			i.p->flags.checked = false;

			if (i.q->flags.active) {
				ael.erase(i.q->aelpos);
				i.q->flags.active = false;
			}
			unlink(i.q);
			i.q->flags.checked = false;

			if (i.f == action_t::intersect) {
				intersect(i.p, i.q);
				intersections++;
				assert_(! i.q->flags.todo);
				delete i.q;
			}
			else if (i.f == action_t::split) {
				split(i.p, i.v);
				split(i.q, i.v);
				splits++;
			}
			assert_(! i.p->flags.todo);
			delete i.p;
		}
	}


	void sweep() {

		assert_(todo.empty());
		active_edge_list_t::iterator next = ael.end();

		for (edgemap_t::iterator it = dot->above.begin(); it != dot->above.end(); it++) {

			edge_t *e = it->second;

			if (e->flags.active) {
				next = e->aelpos;
				next++;

				ael.erase(e->aelpos);
				e->flags.active = false;
			}
		}

		/* if we're not inserting any new edges, we may
		   have revealed two new neighbours which have
		   not been compared yet. Remove and reinsert
		   one of them */

		if (next != ael.end()) {

			edge_t *e = *next;
			assert_(e->flags.active);

			ael.erase(next);
			e->flags.active = false;

			if (e->flags.todo == false) {
				e->flags.todo = true;
				todo.push_back(e);
			}
		}

		for (edgemap_t::iterator it = dot->below.begin(); it != dot->below.end(); it++) {

			edge_t *e = it->second;

			assert_(e->flags.active == false);

			if (e->flags.todo == false) {
				e->flags.todo = true;
				todo.push_back(e);
			}
		}

		while (! todo.empty()) {

			edge_t *e = *todo.begin();
			todo.pop_front();

			assert_(e->flags.todo == true);
			e->flags.todo = false;

			if (e->wind.is_zero())
				continue;

			/* insert may detect an intersection and throw an exception;
			   the exception handler will add the sub-edges to our todo
			   list and we'll try and insert them again in due course */

			insert(e);
		}
	}


	int edgecount() {
		int n = 0;
		for (vertexset_t::iterator it = vertexset.begin(); it != vertexset.end(); it++)
			n += (*it)->below.size();
		return n;
	}


	void print_sweep(active_edge_list_t &ael, vertex_t *dot) {
		dot->print("--- start sweep ---"); cout << endl;
		int a = 0, b = 0;
		for (active_edge_list_t::iterator it = ael.begin(); it != ael.end(); it++) {
			assert_((*it)->from == dot || vertexsort_t()((*it)->from, dot));
			assert_(vertexsort_t()(dot, (*it)->to));
			a += (*it)->wind.a;
			b += (*it)->wind.b;
			char buffer[60];
			sprintf(buffer, " [%d %d]", a, b);
			(*it)->print(buffer);
		}
		dprintf("--- end sweep ---\n");


		for (active_edge_list_t::iterator it = ael.begin(); it != ael.end(); it++) {

			active_edge_list_t::iterator nit = it;
			nit++;

			if (nit != ael.end()) {
				try {
					assert_(sort_active_edge()(*it, *nit));
				}
				catch(...) {
					(*it)->print("compare");
					(*nit)->print("with");
					assert_(0 && "exception in sweep check");
				}
			}
		}
	}


	void intersect() {

		assert_(ael.empty());

		dprintf("%d vertices\n", (int)vertexset.size());

		for (vertexset_t::iterator it = vertexset.begin(); it != vertexset.end(); it++) {

			dot = *it; /* focus on this point... */

			sweep();
			// print_sweep(ael, dot);

			walklist(ael, flatten_rule);
		}

		for (vertexset_t::iterator it = vertexset.begin(); it != vertexset.end(); it++)
			cull((*it)->below);

		for (vertexset_t::iterator it = vertexset.begin(); it != vertexset.end(); it++)
			fold(*it);
	}


	/* round rational vertices to their nearest integer grid point, bending
	   edges round grid points where they would otherwise change sides. */

	set<edge_t *> snaplist;

	void hittest(vertex_t *v) {

		/* we know this point overlaps the integer snaplist in y; if
		   it overlaps an edge in x too, check whether the point lays
		   on different sides of the original and the snapped versions
		   of the edge; if it does, it is a pin for the edge  */

		for (set<edge_t *>::iterator it = snaplist.begin(); it != snaplist.end(); it++) {

			edge_t *p = *it;

			if (! range_t(p->from->x, p->to->x).overlaps(range_t(v->x, v->x)))
				continue;

			int a = side(v, line_t(p->from->x.i, p->from->y.i, p->to->x.i, p->to->y.i));
			int b = side(v, p->raw);

			/* point exactly on the original line ? not a problem, unless
			   it's also exactly on the snapped line in which case we need
			   to split the edge at the point */

			if (b == 0 || a * b < 0)
				p->add_pin(find(v->x, v->y), b <= 0);
		}
	}

	vertexset_t snapset;

	/* round rational point to nearest integer
	   boundary and insert into snapset */

	vertex_t *snap(rat_t x, rat_t y) {

		if (x.n != 0) {
			if (! (x < rat_t(x.i, 1, 2)))
				++x.i;
			x.n = 0;
			x.d = 1;
		}
		if (y.n != 0) {
			if (! (y < rat_t(y.i, 1, 2)))
				++y.i;
			y.n = 0;
			y.d = 1;
		}

		vertex_t *v = new vertex_t(x, y);

		pair<vertexset_t::iterator, bool> r = snapset.insert(v);
 
		if (r.second == false)
			delete v;

		return *r.first;
	}

	void stable_snap_round() {

		/* Remove irrational edges from vertexset and insert
		   into snapset. We have to use a separate set because
		   the vertexset may already contain an edge joining
		   the snapped endpoints of the edge we're processing,
		   so we can't simply remove and reinsert into the
		   vertexset because the edges would be combined. */

		for (vertexset_t::iterator it = vertexset.begin(); it != vertexset.end(); it++) {
			for (edgemap_t::iterator et = (*it)->above.begin(); et != (*it)->above.end(); /* et++ */) {

				edge_t *e = (et++)->second;
				assert_(e->flags.keep == true);

				/* set winding number as per clipped
				 * shape, so we can detect cancelling
				 * edges */

				e->wind = wind_t(e->flags.sense ? -1 : 1, 0);

				/* insert snapped endpoints into snapset */

				vertex_t *p = snap(e->from->x, e->from->y);
				vertex_t *q = snap(e->to->x, e->to->y);

				if (e->from->x.n == 0 && e->from->y.n == 0 &&
				    e->to->x.n == 0 && e->to->y.n == 0)
					continue;

				/* merge edge into snapset and remove from vertexset */

				p->goes_to(q, e->raw, e->wind, e);
				remove(e);
			}
		}

		/* now sweep the snapped point set, looking for points
		   which are on different sides of the original and
		   the snapped edges, inserting snapped vertices
		   (pins) into the vertexset as we go */

		assert_(snaplist.empty());

		for (vertexset_t::iterator it = snapset.begin(); it != snapset.end(); it++) {

			vertex_t *v = *it;

			for (edgemap_t::iterator et = v->above.begin(); et != v->above.end(); et++) {
				edge_t *e = et->second;

				e->add_pin(find(v->x, v->y), true);
				snaplist.erase(e);
			}

			hittest(v);

			for (edgemap_t::iterator et = v->below.begin(); et != v->below.end(); et++) {
				edge_t *e = et->second;

				e->add_pin(find(v->x, v->y), true);
				snaplist.insert(e);
			}
		}

		assert_(snaplist.empty());

		/* and finally remove each edge from the snapset and
		   merge it (possibly via intermediate pins) back into
		   the vertexset at its snapped position */

		for (vertexset_t::iterator it = snapset.begin(); it != snapset.end(); it++) {
			for (edgemap_t::iterator et = (*it)->above.begin(); et != (*it)->above.end(); /* et++ */) {

				edge_t *e = (et++)->second;

				e->snap_to_pins();
				remove(e);
			}
		}
	}


	/* find a vertex and follow edges until we get back to the start;
	   edges are removed as we go, to speed up search */

	void get_result(flatten_arrangement::polygon_list_t &result) {

		flatten_arrangement::polygon_t poly;

		for (vertexset_t::iterator it = vertexset.begin(); it != vertexset.end(); it++) {

			if ((*it)->below.empty())
				continue;

			edgemap_t::iterator et = (*it)->below.begin();

			edge_t *cur = et->second;

			vertex_t *first = cur->from;
			bool sense = cur->flags.sense;
			bool flip = false;

			for (;;) {
			  continue_:
				vertex_t *from = (flip) ? cur->to : cur->from;
				vertex_t *to = (flip) ? cur->from : cur->to;

				poly.push_back(make_pair(from->x.i, from->y.i));

				remove(cur);

				if (to == first)
					break;

				/* all the edges in the above set are flipped */
				for (edgemap_t::iterator et = to->above.begin(); et != to->above.end(); et++) {
					if (et->second->flags.sense != sense) {
						cur = et->second;
						flip = true;
						goto continue_;
					}
				}
				for (edgemap_t::iterator et = to->below.begin(); et != to->below.end(); et++) {
					if (et->second->flags.sense == sense) {
						cur = et->second;
						flip = false;
						goto continue_;
					}
				}
				dprintf("no closing edge (%d)\n", (int)poly.size());
				break;
			}
			if (sense)
				poly.reverse();
			result.push_back(poly);
			poly.clear();
		}
		dprintf("%d dangling edges\n", edgecount());
	}
}; /* struct flattener */


flatten_arrangement::flatten_arrangement(int flatten_rule)
{
	flattener = new flattener_t(flatten_rule);
}


flatten_arrangement::~flatten_arrangement()
{
	delete flattener;
}


void flatten_arrangement::add_edge(int x, int y, int u, int v, bool is_shape_b)
{
	try {
		flattener->add_edge(x, y, u, v, is_shape_b);
	}
	catch (...) {
		dprintf("flatten: add edge failed\n");
	}
}


bool flatten_arrangement::flatten()
{
	dprintf("starting with %d edges\n", flattener->edgecount());

	passes = 0;
	intersections = 0;
	splits = 0;
	comparisons = 0;

	try {
		flattener->intersect();
		flattener->stable_snap_round();
	}
	catch(...) {
		dprintf("flatten: uncaught exception\n");
		return false;
	}
	return true;
}


bool flatten_arrangement::get_result(polygon_list_t& result)
{
	try {
		flattener->get_result(result);
	}
	catch(...) {
		dprintf("flatten: get_result: uncaught exception\n");
		return false;
	}
	return true;
}

/* end */

