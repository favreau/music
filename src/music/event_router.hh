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
   * Also considering EventRoutingData partly realizes IndexInterval interface,
   * inheritance was introduced.
   */

  class IndexProcessor
  {
  public:
    virtual ~IndexProcessor () { };
    virtual void process(double t, int id)=0;
    virtual void *getPtr() = 0;
    virtual IndexProcessor* clone() = 0;
  };

  class GlobalIndexProcessor: public IndexProcessor {
    EventHandlerGlobalIndex* global;
  public:
    GlobalIndexProcessor (EventHandlerPtr* h) { global = h->global (); }
    void process (double t, int id) {
      (*global) (t, id);
    }
    void *getPtr () { return global; }
    IndexProcessor* clone () {return new GlobalIndexProcessor (*this);}
  };

  class LocalIndexProcessor: public IndexProcessor {
    EventHandlerLocalIndex* local;
  public:
    LocalIndexProcessor(EventHandlerPtr* h) {local = h->local();}
    void process(double t, int id){
      (*local) (t, id);
    }
    void *getPtr(){return local;}
    IndexProcessor* clone(){return new LocalIndexProcessor(*this);}
  };

  class EventRoutingData : public IndexInterval{
  public:
    EventRoutingData () { }
    EventRoutingData (const IndexInterval &i):  IndexInterval (i.begin(), i.end(), i.local() ){ };
    EventRoutingData (const EventRoutingData& e) : IndexInterval (e) { }
    ~EventRoutingData(){};
    int offset () const { return this->local (); }
  };

  class InputRoutingData : public EventRoutingData {
    IndexProcessor *specialized_processor_;
    InputRoutingData(const IndexInterval &i,  IndexProcessor *specialized_processor);
  public:
    InputRoutingData (const IndexInterval &i, EventHandlerPtr* h);
    InputRoutingData () : specialized_processor_ (NULL) {}
    ~InputRoutingData();
    InputRoutingData (const InputRoutingData& data);
    InputRoutingData& operator= (const InputRoutingData& data);
    void *Data();
    void process (double t, int id);
  };

  class OutputRoutingData : public EventRoutingData {
    FIBO* buffer_;
  public:
    OutputRoutingData (const IndexInterval &i, FIBO* b);
    OutputRoutingData () {}
    OutputRoutingData& operator= (const OutputRoutingData& data)
    {
      EventRoutingData::operator= (data);
      buffer_ = data.buffer_;
      return *this;
    }
    void *Data();
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
    virtual void insertRoutingData (OutputRoutingData& data) {}
    virtual void insertRoutingData (InputRoutingData& data) {}
    virtual void processEvent (double t, int id) {};
  };

  template<class RoutingData>
  class TableProcessingRouter : public EventRouter {
    //routingTable maps different buffers/even handlers of the current rank
    //to the global index range, current rank is responsible for.
    std::vector<RoutingData> routingData; /*remove routingData*/
    std::map<int, std::vector<RoutingData> > routingTable;
  public:
    void insertRoutingData (RoutingData& data);
    void processEvent (double t, int id);
  };

  class TableProcessingOutputRouter : public TableProcessingRouter<OutputRoutingData> {
  };

  class TableProcessingInputRouter : public TableProcessingRouter<InputRoutingData> {
  };

  template<class RoutingData>
  void
  TableProcessingRouter<RoutingData>::processEvent(double t, int id)
  {
    typename std::vector<RoutingData>::iterator it;
    if (routingTable.count(id) > 0)
      for(it = routingTable[id].begin(); it != routingTable[id].end(); it++)
	{
	  it->process (t, id - it->offset ());
	}
  }

  template<class RoutingData>
  void
  TableProcessingRouter<RoutingData>::insertRoutingData(RoutingData &data)
  {
    routingData.push_back (data);
    for(int i=data.begin(); i < data.end(); ++i){
      //std::cerr << "insert:" <<  i  << ":" << data.offset() << std::endl;
      routingTable[i+data.offset()].push_back(routingData.back());
    }
  }

  template<class RoutingData>
  class TreeProcessingRouter : public EventRouter {
    class Processor : public IntervalTree<int, RoutingData>::Action {
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
    
    IntervalTree<int, RoutingData> routingTable;
  public:
    void insertRoutingData (RoutingData& data);
    void buildTable ();
    void processEvent (double t, int id);
  };

  class TreeProcessingOutputRouter: public TreeProcessingRouter<OutputRoutingData> {
  };

  class TreeProcessingInputRouter: public TreeProcessingRouter<InputRoutingData> {
  };

  template<class RoutingData>
  void
  TreeProcessingRouter<RoutingData>::insertRoutingData (RoutingData &data)
  {
    routingTable.add (data);
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

}
#define MUSIC_EVENT_ROUTER_HH
#endif
