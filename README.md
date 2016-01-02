# polygon-clipper
C++ code to flatten a set of possibly self-intersecting polygons drawn
on an planar integer grid, yielding a set of non-intersecting polygons.

Optionally a second set of possibly self-intersecting polygons can be
combined (AND, OR, XOR, etc.)  with the first set, again yielding a
set of non-intersecting polygons.

The resulting vertices and intersections are robustly snap-rounded to
the integer grid, maintaining the vertex / line arrangement.

--------------
# LICENSING INFORMATION
This library has a dual license, a commercial one suitable for closed
source projects and a GPL license that can be used in open source
software.

Depending on your needs, you must choose one of them and follow its
policies. A detail of the policies and agreements for each license
type are available in the LICENSE.COMMERCIAL and LICENSE.GPL files.
