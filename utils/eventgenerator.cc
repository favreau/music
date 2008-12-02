/*
 *  This file is part of MUSIC.
 *  Copyright (C) 2008 CSC, KTH
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

// Leave as first include---required by BG/L
#include <mpi.h>

#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cmath>

extern "C" {
#include <unistd.h>
#include <getopt.h>
};

#include <music.hh>

#include "datafile.h"

const double DEFAULT_TIMESTEP = 1e-2;
const double DEFAULT_FREQUENCY = 10.0; // Hz

void
usage (int rank)
{
  if (rank == 0)
    {
      std::cerr << "Usage: eventgenerator [OPTION...] N_UNITS" << std::endl
		<< "`eventgenerator' generates spikes from a Poisson distribution." << std::endl << std:: endl
		<< "  -t, --timestep  TIMESTEP time between tick() calls (default " << DEFAULT_TIMESTEP << " s)" << std::endl
		<< "  -f, --frequency FREQ average frequency (default " << DEFAULT_FREQUENCY << " Hz)" << std::endl
		<< "  -m, --imaptype  TYPE     linear (default) or roundrobin" << std::endl
		<< "  -i, --indextype TYPE    global (default) or local" << std::endl
		<< "  -h, --help              print this help message" << std::endl << std::endl
		<< "Report bugs to <mikael@djurfeldt.com>." << std::endl;
    }
  exit (1);
}

int nUnits;
double timestep = DEFAULT_TIMESTEP;
double freq = DEFAULT_FREQUENCY;
string imaptype = "linear";
string indextype = "global";

void
getargs (int rank, int argc, char* argv[])
{
  opterr = 0; // handle errors ourselves
  while (1)
    {
      static struct option longOptions[] =
	{
	  {"timestep",  required_argument, 0, 't'},
	  {"frequency", required_argument, 0, 'f'},
	  {"imaptype",  required_argument, 0, 'm'},
	  {"indextype", required_argument, 0, 'i'},
	  {"help",      no_argument,       0, 'h'},
	  {0, 0, 0, 0}
	};
      /* `getopt_long' stores the option index here. */
      int option_index = 0;

      // the + below tells getopt_long not to reorder argv
      int c = getopt_long (argc, argv, "+t:f:m:i:h", longOptions, &option_index);

      /* detect the end of the options */
      if (c == -1)
	break;

      switch (c)
	{
	case 't':
	  timestep = atof (optarg); //*fixme* error checking
	  continue;
	case 'f':
	  freq = atof (optarg); //*fixme* error checking
	  continue;
	case 'm':
	  imaptype = optarg;
	  if (imaptype != "linear" && imaptype != "roundrobin")
	    {
	      usage (rank);
	      abort ();
	    }
	  continue;
	case 'i':
	  indextype = optarg;
	  if (indextype != "global" && indextype != "local")
	    {
	      usage (rank);
	      abort ();
	    }
	  continue;
	case '?':
	  break; // ignore unknown options
	case 'h':
	  usage (rank);

	default:
	  abort ();
	}
    }

  if (argc != optind + 1)
    usage (rank);

  nUnits = atoi (argv[optind]);
}

double
negexp (double m)
{
  return - m * log (drand48 ());
}

int
main (int argc, char *argv[])
{
  MUSIC::Setup* setup = new MUSIC::Setup (argc, argv);
  
  MPI::Intracomm comm = setup->communicator ();
  int nProcesses = comm.Get_size ();
  int rank = comm.Get_rank ();
  
  getargs (rank, argc, argv);

  MUSIC::EventOutputPort* out = setup->publishEventOutput ("out");
  if (!out->isConnected ())
    {
      if (rank == 0)
	std::cerr << "eventgenerator port is not connected" << std::endl;
      exit (1);
    }

  MUSIC::Index::Type type;
  if (indextype == "global")
    type = MUSIC::Index::GLOBAL;
  else
    type = MUSIC::Index::LOCAL;
  
  std::vector<MUSIC::GlobalIndex> ids;
  std::vector<double> nextSpike;
  
  if (imaptype == "linear")
    {
      int nUnitsPerProcess = nUnits / nProcesses;
      int nLocalUnits = nUnitsPerProcess;
      int rest = nUnits % nProcesses;
      int firstId = nUnitsPerProcess * rank;
      if (rank < rest)
	{
	  firstId += rank;
	  nLocalUnits += 1;
	}
      else
	firstId += rest;
      for (int i = 0; i < nLocalUnits; ++i)
	ids.push_back (firstId + i);
      MUSIC::LinearIndex indices (firstId, nLocalUnits);
      
      out->map (&indices, type);
    }
  else
    {
      for (int i = rank; i < nUnits; i += nProcesses)
	ids.push_back (i);
      MUSIC::PermutationIndex indices (&ids.front (), ids.size ());
      out->map (&indices, type);
    }

  double stoptime;
  setup->config ("stoptime", &stoptime);

  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, timestep);

  double m = 1.0 / freq;
  for (int i = 0; i < ids.size (); ++i)
    nextSpike.push_back (negexp (m));

  double time = runtime->time ();
  while (time < stoptime)
    {
      double nextTime = time + timestep;

      if (type == MUSIC::Index::GLOBAL)
	{
	  for (int i = 0; i < ids.size (); ++i)
	    while (nextSpike[i] < nextTime)
	      {
		out->insertEvent (nextSpike[i],
				   MUSIC::GlobalIndex (ids[i]));
		nextSpike[i] += negexp (m);
	      }
	}
      else
	{
	  for (int i = 0; i < ids.size (); ++i)
	    while (nextSpike[i] < nextTime)
	      {
		out->insertEvent (nextSpike[i],
				   MUSIC::LocalIndex (i));
		nextSpike[i] += negexp (m);
	      }
	}
      
      // Make data available for other programs
      runtime->tick ();

      time = runtime->time ();
    }

  runtime->finalize ();

  delete runtime;

  return 0;
}