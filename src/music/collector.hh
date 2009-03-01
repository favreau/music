/*
 *  This file is part of MUSIC.
 *  Copyright (C) 2009 INCF
 *
 *  MUSIC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  MUSIC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MUSIC_COLLECTOR_HH

#include <map>
#include <vector>

#include <music/BIFO.hh>
#include <music/data_map.hh>

namespace MUSIC {

  class Collector {
    class Interval : public MUSIC::Interval {
    public:
      Interval (IndexInterval& interval);
      bool operator< (const Interval& ref) const
      { return begin () < ref.begin (); }
      // length field is stored overlapping the end field so that the
      // interval information can be "recompiled" for space and time
      // efficiency
      int length () const { return end (); }
      void setLength (int length) { setEnd (length); }
    };
    
    typedef std::vector<Interval> Intervals;
    typedef std::map<BIFO*, Intervals> BufferMap;

    DataMap* dataMap;
    int allowedBuffered_;
    BufferMap buffers;
  public:
    // caller manages deallocation but guarantees existence
    void configure (DataMap* dmap, int allowedBuffered);
    void initialize ();
    void addRoutingInterval (IndexInterval i, BIFO* b);
    void collect ();
    void collect (ContDataT* base);
  };
    
}

#define MUSIC_COLLECTOR_HH
#endif