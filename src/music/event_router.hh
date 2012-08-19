/*
 *  This file is part of MUSIC.
 *  Copyright (C) 2008, 2009 INCF
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

#ifndef MUSIC_EVENT_ROUTER_HH
#include <map>
#include <vector>

#include <music/FIBO.hh>
#include <music/interval_tree.hh>
#include <music/index_map.hh>
#include <music/event.hh>

namespace MUSIC {
  /* remedius
   * Since the router can act as on the output (point-to-point communication)
   * as well as on the input (collective communication) sides and
   * pre-/post-processing approaches are different
   * (in one case it's insertion to the buffer <FIBO>,
   * in another case it's calling appropriate handler <EventHandlerGlobalIndex>),
   * two more successors for EventRoutingData were created:
   * InputRoutingData and OutputRoutingData.
   */

  class EventRoutingData {
    int offset_;
  public:
    EventRoutingData () { }
    EventRoutingData (const IndexInterval &i) : offset_ (i.local ()) { }
    EventRoutingData (const EventRoutingData& e) : offset_ (e.offset_) { }
    ~EventRoutingData(){};
    int offset () const { return offset_; }
  };

  template<class EventHandler>
  class InputRoutingData : public EventRoutingData {
    EventHandler* eventHandler_;
  public:
    InputRoutingData (const IndexInterval &i, EventHandlerPtr* h)
      : EventRoutingData (i), eventHandler_ (*h) { }
    InputRoutingData () : eventHandler_ (NULL) { }
    void process (double t, int id) { (*eventHandler_) (t, id); }
  };

  class OutputRoutingData : public EventRoutingData {
    FIBO* buffer_;
  public:
    OutputRoutingData (const IndexInterval &i, FIBO* b);
    OutputRoutingData () {}
    void process (double t, int id);
  };

  /* remedius
   * We've decided to try different methods for pre-/post-processing the data:
   * using tree algorithm and using table.
   * Hence to this two EventRouter successors were created:
   * TableProcessingRouter and TreeProcessingRouter.
   * Currently TableProcessingRouter can handle only GlobalIndex processing.
   */
  class EventRouter {
  public:
    virtual ~EventRouter() {};
    virtual void buildTable () {};
    /* remedius
     * insertEvent method was renamed to processEvent method,
     * since we've introduced event processing on the input side as well.
     */
    virtual void insertRoutingData (Interval& i, OutputRoutingData& data) {}
    virtual void insertRoutingData (Interval& i, InputRoutingData<EventHandlerGlobalIndex>& data) {}
    virtual void insertRoutingData (Interval& i, InputRoutingData<EventHandlerLocalIndex>& data) {}
    virtual void processEvent (double t, int id) {};
  };

  template<class RoutingData>
  class TableProcessingRouter : public EventRouter {
    //routingTable maps different buffers/even handlers of the current rank
    //to the global index range, current rank is responsible for.
    std::vector<RoutingData> routingData; /*remove routingData*/
    std::map<int, std::vector<RoutingData> > routingTable;
  public:
    void insertRoutingData (Interval& i, RoutingData& data);
    void processEvent (double t, int id);
  };

  template<class RoutingData>
  void
  TableProcessingRouter<RoutingData>::processEvent (double t, int id)
  {
    typename std::vector<RoutingData>::iterator it;
    if (routingTable.count (id) > 0)
      for (it = routingTable[id].begin(); it != routingTable[id].end (); it++)
	{
	  it->process (t, id - it->offset ());
	}
  }

  template<class RoutingData>
  void
  TableProcessingRouter<RoutingData>::insertRoutingData (Interval& ival,
							 RoutingData &data)
  {
    routingData.push_back (data);
    for (int i = ival.begin (); i < ival.end (); ++i) {
      //std::cerr << "insert:" <<  i  << ":" << data.offset() << std::endl;
      routingTable[i + data.offset ()].push_back (routingData.back ());
    }
  }

  class TableProcessingOutputRouter : public TableProcessingRouter<OutputRoutingData> {
  };

  class TableProcessingInputGlobalRouter : public TableProcessingRouter<InputRoutingData<EventHandlerGlobalIndex> > {
  };

  class TableProcessingInputLocalRouter : public TableProcessingRouter<InputRoutingData<EventHandlerLocalIndex> > {
  };

  template<class RoutingData>
  class TreeProcessingRouter : public EventRouter {
    class Processor : public IntervalTree<int, Interval, RoutingData>::Action {
    protected:
      double t_;
      int id_;
    public:
      Processor (double t, int id) : t_ (t), id_ (id) { };
      void operator() (RoutingData& data)
      {
	/* remedius
	 * for Global index ((RoutingData &)data).offset () is set to 0 during
	 * SpatialNegotiator::wrapIntervals();
	 */
	data.process (t_, id_ - data.offset ());
      }
    };
    
    IntervalTree<int, Interval, RoutingData> routingTable;
  public:
    void insertRoutingData (Interval& i, RoutingData& data);
    void buildTable ();
    void processEvent (double t, int id);
  };

  template<class RoutingData>
  void
  TreeProcessingRouter<RoutingData>::insertRoutingData (Interval& i,
							RoutingData &data)
  {
    routingTable.add (i, data);
  }


  template<class RoutingData>
  void
  TreeProcessingRouter<RoutingData>::buildTable ()
  {
    MUSIC_LOG0 ("Routing table size for rank 0 = " << routingTable.size ());
    routingTable.build ();
  }

  template<class RoutingData>
  void
  TreeProcessingRouter<RoutingData>::processEvent (double t, int id)
  {
    Processor i (t, id);
    routingTable.search (id, &i);
  }

  class TreeProcessingOutputRouter : public TreeProcessingRouter<OutputRoutingData> {
  };

  class TreeProcessingInputGlobalRouter : public TreeProcessingRouter<InputRoutingData<EventHandlerGlobalIndex> > {
  };

  class TreeProcessingInputLocalRouter : public TreeProcessingRouter<InputRoutingData<EventHandlerLocalIndex> > {
  };

}
#define MUSIC_EVENT_ROUTER_HH
#endif
