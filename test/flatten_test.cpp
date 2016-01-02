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

#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>

inline double realt()
{
    struct timeval  tv;
    struct timezone ov;
   
    gettimeofday(&tv, &ov);
    return tv.tv_sec + tv.tv_usec * 0.000001;
}

#include "flatten_arrangement.h"


void add_file(flatten_arrangement *flattener, const char *filename, bool is_shape_b, FILE *g)
{
	FILE *f = fopen(filename, "r");

	if (f == 0)
		exit(6);

	char buffer[100];

	fgets(buffer, sizeof(buffer), f);
	if (atoi(buffer) != 1)
		exit(5);

	fgets(buffer, sizeof(buffer), f);
	int polygon_count = atoi(buffer);

	printf("get %d polygons\n", polygon_count);

	while (polygon_count--) {
		fgets(buffer, sizeof(buffer), f);
		int edge_count = atoi(buffer);

		printf("%d edges\n", edge_count);

		bool first = true;

		int oldx, oldy;
		int firstx, firsty;

		const char *action = "moveto";
		
		while (edge_count--) {
			fgets(buffer, sizeof(buffer), f);

			char *endp;
			int x = strtol(buffer, &endp, 10);
			int y = strtol(endp + 1, &endp, 10);

			if (first == true) {
				firstx = oldx = x;
				firsty = oldy = y;
				first = false;
				continue;
			}

			flattener->add_edge(oldx, oldy, x, y, is_shape_b);

			fprintf(g, "%d %d %s\n", x, y, action);
			action = "lineto";

			if (edge_count == 0) {
				flattener->add_edge(x, y, firstx, firsty, is_shape_b);
				fprintf(g, "closepath\n");
			}

			oldx = x;
			oldy = y;
		}
	}
}

int main(int argc, char *argv[])
{
	FILE *f = fopen("t.ps", "w");

	fprintf(f, "%%!PS\n");
	fprintf(f, "100 0 translate\n");
	fprintf(f, "0.004 0.004 scale\n");

	flatten_arrangement *flattener = new flatten_arrangement(flatten_arrangement::FLATTEN_A_AND_B);

	if (flattener == 0) {
		printf("failed to create flattener\n");
		exit(4);
	}

	add_file(flattener, "c.wlr", false, f);
	fprintf(f, "closepath 1 0.7 1 setrgbcolor fill\n");

	add_file(flattener, "s.wlr", true, f);
	fprintf(f, "closepath 1 1 0.7 setrgbcolor fill\n");

	double then = realt();

	bool ok = flattener->flatten();

	double now = realt();

	printf("flattened in %f milliseconds\n", (now - then) * 1000.0);

	if (! ok)
		exit(4);

	flatten_arrangement::polygon_list_t result;

	flattener->get_result(result);

	double finished = realt();

	printf("got %d outlines in %f milliseconds\n", (int)result.size(), (finished - now) * 1000.0);
	printf("total = %f milliseconds\n", (finished - then) * 1000.0);

	srand(0);

	if (true) {

		int edges = 0;
		for (flatten_arrangement::polygon_list_t::iterator it = result.begin(); it != result.end(); it++) {

			edges += (*it).size();

			bool first = true;

			for (flatten_arrangement::polygon_t::iterator pi = (*it).begin(); pi != (*it).end(); pi++) {
				fprintf(f, "%d %d %s\n", pi->first, pi->second, (first) ? "moveto" : "lineto");
				first = false;
			}
		}

		fprintf(f, "closepath 0.7 1 1 setrgbcolor fill\n");
		
		printf("emitted %d edges\n", edges);

		fprintf(f, "showpage\n");
	}

	fclose(f);

	delete flattener;
	return 0;
}
