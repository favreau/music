/*
 *  This file is part of MUSIC.
 *  Copyright (C) 2008 INCF
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

#ifndef EVENT_ROUTER_HH

#include <vector>

#include <music/FIBO.hh>
#include <music/interval_tree.hh>
#include <music/index_map.hh>
#include <music/event.hh>

namespace MUSIC {

  class EventRoutingData {
    IndexInterval _interval;
    FIBO* buffer;
  public:
    EventRoutingData () { }
    EventRoutingData (IndexInterval i, FIBO* b)
      : _interval (i), buffer (b) { }
    int begin () const { return _interval.begin (); }
    int end () const { return _interval.end (); }
    int offset () const { return _interval.local (); }
    void insert (double t, int id) {
      Event* e = static_cast<Event*> (buffer->insert ());
      e->t = t;
      e->id = id;
    }
  };


  class EventRouter {
    class Inserter : public IntervalTree<int, EventRoutingData>::Action {
    protected:
      double _t;
      int _id;
    public:
      Inserter (double t, int id) : _t (t), _id (id) { };
      void operator() (EventRoutingData& data)
      {
	data.insert (_t, _id - data.offset ());
      }
    };
    
    IntervalTree<int, EventRoutingData> routingTable;
  public:
    void insertRoutingInterval (IndexInterval i, FIBO* b);
    void buildTable ();
    void insertEvent (double t, GlobalIndex id);
    void insertEvent (double t, LocalIndex id);
  };
    
}

#define EVENT_ROUTER_HH
#endif
