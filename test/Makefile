# Copyright 2016 Centipede Productions Ltd, All Rights Reserved

# Commercial Licence Usage
# ------------------------
# You may use this file in accordance with the terms contained in a
# written agreement between You and Centipede Productions Ltd.
# 
# GNU General Public License Usage
# --------------------------------
# Alternatively, this file may be used under the terms of the GNU General
# Public License version 3.0 as published by the Free Software Foundation
# and appearing in the file LICENSE.GPL included in the packaging of this
# file.  Please visit http://www.gnu.org/copyleft/gpl.html and review the
# information to ensure the GNU General Public License version 3.0
# requirements will be met.

VPATH := ../source
CXXFLAGS := -Wall -O3 -I../source

flatten_test: flatten_test.o flatten_arrangement.o
	$(CXX) $(CXXFLAGS) -o $@ $^

flatten_test.o: \
	flatten_test.cpp \
	flatten_arrangement.h

flatten_arrangement.o: \
	flatten_arrangement.cpp \
	flatten_arrangement.h \
	simple_rational.hpp

clean:
	rm *.o flatten_test

# end
