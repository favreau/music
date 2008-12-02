/*
 *  This file is part of MUSIC.
 *  Copyright (C) 2007, 2008 CSC, KTH
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

#ifndef MUSIC_SETUP_HH

#include <mpi.h>

#include <string>
#include <vector>

#include <music/port.hh>

#include <music/index_map.hh>
#include <music/linear_index.hh>
#include <music/cont_data.hh>
#include <music/configuration.hh>
#include <music/connector.hh>

using std::string;

namespace MUSIC {
  
  class Setup {
  private:
    Configuration* _config;
    MPI::Intracomm comm;
    std::vector<Port*> _ports;
    std::vector<Connector*> _connectors;

  public:
    Setup (int& argc, char**& argv);

    Setup (int& argc, char**& argv, int required, int* provided);

    virtual ~Setup ();

    bool launchedByMusic ();

    void init (int& argc, char**& argv);

    MPI::Intracomm communicator ();

    ConnectivityInfo* portConnectivity (const std::string localName);

    //*fixme* unused
    bool isConnected (const std::string localName);

    ConnectivityInfo::PortDirection
    portDirection (const std::string localName);

    int portWidth (const std::string localName);

    PortConnectorInfo portConnections (const std::string localName);

    bool config (string var, string* result);

    bool config (string var, int* result);

    bool config (string var, double* result);

    ContInputPort* publishContInput (string identifier);

    ContOutputPort* publishContOutput (string identifier);

    EventInputPort* publishEventInput (string identifier);

    EventOutputPort* publishEventOutput (string identifier);

    MessageInputPort* publishMessageInput (string identifier);

    MessageOutputPort* publishMessageOutput (string identifier);

    std::vector<Port*>* ports ()
    {
      return &_ports;
    }    

    void addPort (Port* p);
    
    std::vector<Connector*>* connectors ()
    {
      return &_connectors;
    }
    
    void addConnector (Connector* c);
    
  };
  
}

#define MUSIC_SETUP_HH
#endif
