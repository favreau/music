/*
 *  This file is part of MUSIC.
 *  Copyright (C) 2007, 2008 INCF
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

#ifndef MUSIC_SUBCONNECTOR_HH

#include <mpi.h>

#include <string>

#include <music/synchronizer.hh>
#include <music/FIBO.hh>
#include <music/event.hh>

namespace MUSIC {

  const int SPIKE_MSG = 1;
  const int SPIKE_BUFFER_MAX = 10000 * sizeof (Event);

  // The subconnector is responsible for the local side of the
  // communication between two MPI processes, one for each port of a
  // port pair.  It is created in connector::connect ().
  
  class Subconnector {
  private:
  protected:
    Synchronizer* synch;
    MPI::Intercomm intercomm;
    int _remoteRank;
    int _receiverRank;
    std::string _receiverPortName;
  public:
    Subconnector () { }
    Subconnector (Synchronizer* synch,
		  MPI::Intercomm intercomm,
		  int remoteRank,
		  int receiverRank,
		  std::string receiverPortName);
    virtual ~Subconnector ();
    virtual void tick () = 0;
    int remoteRank () const { return _remoteRank; }
    int receiverRank () const { return _receiverRank; }
    std::string receiverPortName () const { return _receiverPortName; }
    void connect ();
  };
  
  class OutputSubconnector : virtual public Subconnector {
  protected:
    FIBO _buffer;
  public:
    OutputSubconnector (Synchronizer* synch,
			 MPI::Intercomm intercomm,
			 int remoteRank,
			 int receiverRank,
			 std::string receiverPortName,
			 int elementSize);
    FIBO* buffer () { return &_buffer; }
    void send ();
    int startIdx ();
    int endIdx ();
  };
  
  class InputSubconnector : virtual public Subconnector {
  public:
    InputSubconnector ();
  };

  class ContOutputSubconnector : public OutputSubconnector {
  public:
    void mark ();
  };
  
  class ContInputSubconnector : public InputSubconnector {
  };

  class EventSubconnector : virtual public Subconnector {
  public:
    EventSubconnector ();
  };
  
  class EventOutputSubconnector : public OutputSubconnector {
  public:
    EventOutputSubconnector (Synchronizer* _synch,
			       MPI::Intercomm _intercomm,
			       int remoteRank,
			       std::string _receiverPortName);
    void tick ();
    void send ();
  };
  
  class EventInputSubconnector : public InputSubconnector {
  public:
    EventInputSubconnector (Synchronizer* synch,
			      MPI::Intercomm intercomm,
			      int remoteRank,
			      int receiverRank,
			      std::string receiverPortName);
    void tick ();
    virtual void receive () = 0;
  };

  class EventInputSubconnectorGlobal : public EventInputSubconnector {
    EventHandlerGlobalIndex* handleEvent;
  public:
    EventInputSubconnectorGlobal (Synchronizer* synch,
				     MPI::Intercomm intercomm,
				     int remoteRank,
				     int receiverRank,
				     std::string receiverPortName,
				     EventHandlerGlobalIndex* eh);
    void receive ();
  };

  class EventInputSubconnectorLocal : public EventInputSubconnector {
    EventHandlerLocalIndex* handleEvent;
  public:
    EventInputSubconnectorLocal (Synchronizer* synch,
				    MPI::Intercomm intercomm,
				    int remoteRank,
				    int receiverRank,
				    std::string receiverPortName,
				    EventHandlerLocalIndex* eh);
    void receive ();
  };

}

#define MUSIC_SUBCONNECTOR_HH
#endif
