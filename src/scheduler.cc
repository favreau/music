/*
 *  This file is part of MUSIC.
 *  Copyright (C) 2012 INCF
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

//#define MUSIC_DEBUG
#include "music/scheduler.hh"

#include "music/debug.hh"

#if MUSIC_USE_MPI

#include <cmath>
#include <iostream>

#ifdef MUSIC_DEBUG
#include <cstdlib>
#endif

namespace MUSIC {

  Scheduler::Node::Node (int id, const Clock &localTime, int leader, int nProcs)
    : id_ (id), localTime_ (localTime), leader_ (leader), nProcs_ (nProcs)
  {
  }

  void
  Scheduler::Node::advance ()
  {
#ifdef MUSIC_DEBUG
    double time = localTime_.time ();
#endif
    localTime_.tick ();
    MUSIC_LOG0 ("advanced " << id_ <<" from "<<time << " to " <<  localTime_.time ()) ;
  }

  void
  Scheduler::Node::addConnection (SConnection *conn, bool input)
  {
    if (input)
      inputConnections_.push_back (conn);
    else
      outputConnections_.push_back (conn);
  }

  double
  Scheduler::Node::nextReceive () const
  {
    std::vector<SConnection*>::const_iterator conn;
    double nextTime = std::numeric_limits<double>::infinity ();
    for (conn = inputConnections_.begin ();
	 conn < inputConnections_.end ();
	 conn++)
      {
	if ((*conn)->nextReceive ().time () < nextTime)
	  nextTime = (*conn)->nextReceive ().time ();
      }
    return nextTime;
  }

  Scheduler::SConnection::SConnection (int pre,
				       int post,
				       const ClockState &latency,
				       int maxBuffered,
				       bool interpolate,
				       bool multiComm,
				       int port_code)
    : pre_id (pre),
      post_id (post),
      latency_ (latency),
      maxBuffered_ (maxBuffered),
      interpolate_ (interpolate),
      multiComm_ (multiComm),
      port_code_ (port_code),
      connector_ (NULL)
  {
  }
  
  void
  Scheduler::SConnection::initialize (std::vector<Node*> &nodes)
  {
    pre_ = nodes.at (pre_id);
    post_ = nodes.at (post_id);
    pre_->addConnection (this);
    post_->addConnection (this, true);
    nextSend_.configure (pre_->localTime ().timebase (),
			 pre_->localTime ().tickInterval ());
    nextReceive_.configure (post_->localTime ().timebase (),
			    post_->localTime ().tickInterval ());
    advance ();
  }

  void
  Scheduler::SConnection::advance ()
  {
    _advance ();
    Clock  r = nextReceive_;
    Clock  s = nextSend_;
    _advance ();
    while (nextReceive_ == r)
      {
	s = nextSend_;
	_advance ();
      }
    nextReceive_ = r;
    nextSend_ = s;
    MUSIC_LOG0 (pre_id << "->"<<post_id << "::"<<nextSend_.time () << "::" <<  nextReceive_.time ()) ;
  }

  void
  Scheduler::SConnection::_advance ()
  {
    ClockState limit = (nextSend_.integerTime ()
			+ latency_
			- nextReceive_.tickInterval ());
    while (nextReceive_.integerTime () <= limit)
      nextReceive_.tick ();

    /*limit = nextReceive_.integerTime () + nextReceive_.tickInterval () - latency_;
      int bCount = 0;
      while (nextSend_.integerTime () < limit) {
      nextSend_.tick ();
      ++bCount;
      }
      // Advance send time according to precalculated buffer
      if (bCount < maxBuffered_)
      nextSend_.ticks (maxBuffered_ - bCount);
    */
    nextSend_.ticks (maxBuffered_+1);
  }
  
  Scheduler::Scheduler (int node_id)
    :self_node (node_id)
  {
  }

  Scheduler::~Scheduler ()
  {
    for (std::vector<SConnection*>::iterator conn=connections.begin ();
	 conn < connections.end ();
	 conn++)
      delete (*conn);
    for (std::vector<Node*>::iterator node=nodes.begin ();
	 node < nodes.end ();
	 node++)
      delete *node;
  }

  void
  Scheduler::addNode (int id, const Clock &localTime, int leader, int nProcs)
  {
    nodes.push_back (new Node (id, localTime, leader, nProcs));
  }

  void
  Scheduler::addConnection (int pre_id,
			    int post_id,
			    const ClockState &latency,
			    int maxBuffered,
			    bool interpolate,
			    bool multiComm,
			    int port_code)
  {
    connections.push_back (new SConnection (pre_id,
					   post_id,
					   latency,
					   maxBuffered,
					   interpolate,
					   multiComm,
					   port_code));
  }

  void
  Scheduler::initialize (std::vector<Connector*>& connectors)
  {
    std::vector<SConnection*>::iterator conn;

    //MUSIC_LOGR ("#of nodes:" << nodes.size () << ":#of connections:" <<  connections.size ());
    for (conn = connections.begin (); conn < connections.end (); conn++)
      {
	(*conn)->initialize (nodes);
	bool foundLocalConnector = false;
	for (std::vector<Connector*>::iterator c = connectors.begin ();
	     c != connectors.end ();
	     ++c)
	  {
	    if ((*conn)->portCode () == (*c)->receiverPortCode ())
	      {
		foundLocalConnector = true;
		(*conn)->setConnector ((*c));
		(*c)->setInterpolate ((*conn)->getInterpolate ());
		(*c)->setLatency ((*conn)->getLatency ());
		(*c)->initialize ();
		break;
	      }
	  }
#if 1
	if (!foundLocalConnector && (*conn)->needsMultiCommunication ())
	  {
	    (*conn)->setConnector
	      (new ProxyConnector ((*conn)->preNode ()->getId (),
				   (*conn)->preNode ()->leader (),
				   (*conn)->preNode ()->nProcs (),
				   (*conn)->postNode ()->getId (),
				   (*conn)->postNode ()->leader (),
				   (*conn)->postNode ()->nProcs ()));
	  }
#endif
      }
  }


  void
  Scheduler::createMultiConnectors (Clock localTime,
				    std::vector<Connector*>& connectors,
				    MultiBuffer* multiBuffer,
				    std::vector<MultiConnector*>& multiConnectors)
  {
    //if (MPI::COMM_WORLD.Get_rank () == 2)
    //  std::cout << "prep localTime = " << localTime.time () << std::endl;
    // We need to create the MultiConnectors ahead of time since their
    // creation requires communication between all members of
    // COMM_WORLD.  Temporary solution: Create all MultiConnectors
    // that will be needed during the first 100 steps.
    std::vector<std::pair<double, Connector *> > schedule;

    std::vector<bool> multiProxies;
    multiProxies.resize (Connector::proxyIdRange ());
    
    for (int self_node = 0; self_node < (int) nodes.size (); ++self_node)
      {
	localTime.reset ();

	createMultiConnNext (self_node,
			     localTime,
			     connectors,
			     multiBuffer,
			     schedule);
    
	localTime.ticks (-1);

	std::vector<Connector*> cCache;
	for (int i = 0; i < 100; ++i)
	  {
	    localTime.tick ();
	    createMultiConnStep (self_node,
				 localTime,
				 connectors,
				 multiBuffer,
				 multiConnectors,
				 multiProxies,
				 cCache,
				 schedule,
				 false);
	  }
	createMultiConnStep (self_node,
			     localTime,
			     connectors,
			     multiBuffer,
			     multiConnectors,
			     multiProxies,
			     cCache,
			     schedule,
			     true);
	schedule.clear ();

	// Now reset node and connection clocks to starting values
	for (std::vector<Node*>::iterator node = nodes.begin ();
	     node != nodes.end ();
	     ++node)
	  (*node)->resetClock ();

	for (std::vector<SConnection*>::iterator conn = connections.begin ();
	     conn != connections.end ();
	     ++conn)
	  {
	    (*conn)->resetClocks ();
	    (*conn)->advance ();
	  }
      }
  }


  void
  Scheduler::createMultiConnNext (int self_node,
				  Clock& localTime,
				  std::vector<Connector*>& connectors,
				  MultiBuffer* multiBuffer_,
				  std::vector<std::pair<double, Connector *> > &schedule)
  {
    while (schedule.empty () 
	   // always plan forward past the first time point to make
	   // sure that all events at that time are scheduled
	   || schedule.front ().first == schedule.back ().first)
      {
	std::vector<Node*>::iterator node;
	for ( node=nodes.begin (); node < nodes.end (); node++ )
	  {
	    if ((*node)->nextReceive () > (*node)->localTime ().time ())
	      (*node)->advance ();

	    std::vector<SConnection*>* conns = (*node)->outputConnections ();
	    std::vector<SConnection*>::iterator conn;
	    for (conn = conns->begin (); conn < conns->end (); conn++)
	      //do we have data ready to be sent?
	      if ((*conn)->nextSend () <= (*conn)->preNode ()->localTime ()
		  && (*conn)->nextReceive () == (*conn)->postNode ()->localTime ())
		{
		  if (self_node == (*conn)->postNode ()->getId () //input
		      || self_node == (*conn)->preNode ()->getId ()) //output
		    {
		      double nextComm
			= (self_node == (*conn)->postNode ()->getId ()
			   ? (*conn)->postNode ()->localTime ().time ()
			   : (*conn)->preNode ()->localTime ().time ());
		      schedule.push_back
			(std::pair<double, Connector*>(nextComm,
						       (*conn)->getConnector ()));
		    }

		  MUSIC_LOG0 ("Scheduled communication:"<< (*conn)->preNode ()->getId () <<"->"<< (*conn)->postNode ()->getId () << "at(" << (*conn)->preNode ()->localTime ().time () << ", "<< (*conn)->postNode ()->localTime ().time () <<")");
		  (*conn)->advance ();
		}
	  }
      }
  }


  void
  Scheduler::createMultiConnStep (int self_node,
				  Clock& localTime,
				  std::vector<Connector*>& connectors,
				  MultiBuffer* multiBuffer_,
				  std::vector<MultiConnector*>& multiConnectors,
				  std::vector<bool>& multiProxies,
				  std::vector<Connector*>& cCache,
				  std::vector<std::pair<double, Connector *> > &schedule,
				  bool finalize)
  {
    std::set<int>* connsToFinalize;
    int finalCount = 0;

    if (finalize)
      {
	connsToFinalize = new std::set<int>;
	for (std::vector<Connector*>::iterator c = connectors.begin ();
	     c != connectors.end ();
	     ++c)
	  connsToFinalize->insert ((*c)->receiverPortCode ());
      }

    while (schedule[0].first <= localTime.time()
	   || (finalize && //!connsToFinalize->empty ()
	       ++finalCount <= 4000))
      {
	std::vector< std::pair<double, Connector*> >::iterator comm;
	for (comm = schedule.begin ();
	     comm != schedule.end() && comm->first <= localTime.time ();
	     ++comm)
	  {
	    Connector* connector = comm->second;

	    if (connector == NULL)
	      continue;

	    if (finalize)
	      connsToFinalize->erase (connector->receiverPortCode ());

	    if (!connector->idFlag ())
	      continue;

	    unsigned int multiId = 0;
	    unsigned int multiProxyId = 0;
	    if (!connector->isProxy ())
	      {
		multiId = connector->idFlag ();
		cCache.push_back (connector);
	      }
	    else
	      {
		dynamic_cast<ProxyConnector*> (connector)->setNode (self_node);
		multiProxyId = connector->idFlag ();
	      }
	    bool isProxy = connector->isProxy ();
	    int remoteLeader = connector->remoteLeader ();
	    bool direction = connector->isInput ();
	    // yes, horrible quadratic algorithm for now
	    for (std::vector< std::pair<double, Connector*> >::iterator c
		   = comm + 1;
		 c != schedule.end () && c->first <= localTime.time();
		 ++c)
	      {
		if (c->second == NULL || c->second->idFlag () == 0)
		  continue;
		if (c->second->isProxy ())
		  dynamic_cast<ProxyConnector*> (c->second)->setNode (self_node);
		// lumping criterion
		if (!(c->second->isProxy () ^ isProxy)
		    && c->second->remoteLeader () == remoteLeader
		    && c->second->isInput () == direction)
		  {
		    if (finalize)
		      connsToFinalize->erase (c->second->receiverPortCode ());
		    if (!c->second->isProxy ())
		      {
			cCache.push_back (c->second);
			multiId |= c->second->idFlag ();
		      }
		    else
		      {
			multiProxyId |= c->second->idFlag ();
		      }
		    c->second = NULL; // taken
		  }
	      }

	    //if (MPI::COMM_WORLD.Get_rank () == 2)
	    //	std::cout << "Prep multiId = " << multiId << std::endl;
	    if (cCache.size () == 0)
	      {
		if (!multiProxies[multiProxyId])
		  {
		    MPI::COMM_WORLD.Create (MPI::GROUP_EMPTY);
		    MPI::COMM_WORLD.Barrier ();
		    //*fixme* Impossible to delete the following object
		    multiProxies[multiProxyId] = true;
		  }
	      }
	    else if (multiConnectors[multiId] == NULL)
	      {
		multiConnectors[multiId]
		  = new MultiConnector (multiBuffer_, cCache);
	      }
	    cCache.clear ();
	  }
	schedule.erase (schedule.begin (), comm);
	createMultiConnNext (self_node,
			     localTime,
			     connectors,
			     multiBuffer_,
			     schedule);
      }

    if (finalize)
      delete connsToFinalize;
    
  }


  void
  Scheduler::nextCommunication (Clock& localTime,
				std::vector<std::pair<double, Connector *> > &schedule)
  {
    //if (MPI::COMM_WORLD.Get_rank () == 2)
    //  std::cout << "localTime = " << localTime.time () << std::endl;
    while (schedule.empty () 
	   // always plan forward past the first time point to make
	   // sure that all events at that time are scheduled
	   || schedule.front ().first == schedule.back ().first)
      {
	std::vector<Node*>::iterator node;
	for ( node=nodes.begin (); node < nodes.end (); node++ )
	  {
	    if ((*node)->nextReceive () > (*node)->localTime ().time ())
	      (*node)->advance ();

	    std::vector<SConnection*>* conns = (*node)->outputConnections ();
	    std::vector<SConnection*>::iterator conn;
	    for (conn = conns->begin (); conn < conns->end (); conn++)
	      //do we have data ready to be sent?
	      if ((*conn)->nextSend () <= (*conn)->preNode ()->localTime ()
		  && (*conn)->nextReceive () == (*conn)->postNode ()->localTime ())
		{
		  if (self_node == (*conn)->postNode ()->getId () //input
		      || self_node == (*conn)->preNode ()->getId ()) //output
		    {
		      double nextComm
			= (self_node == (*conn)->postNode ()->getId ()
			   ? (*conn)->postNode ()->localTime ().time ()
			   : (*conn)->preNode ()->localTime ().time ());
		      schedule.push_back
			(std::pair<double, Connector*>(nextComm,
						       (*conn)->getConnector ()));
		    }

		  MUSIC_LOG0 ("Scheduled communication:"<< (*conn)->preNode ()->getId () <<"->"<< (*conn)->postNode ()->getId () << "at(" << (*conn)->preNode ()->localTime ().time () << ", "<< (*conn)->postNode ()->localTime ().time () <<")");
		  (*conn)->advance ();
		}
	  }
      }
  }
}

#endif
