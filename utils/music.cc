/*
 *  This file is part of MUSIC.
 *  Copyright (C) 2007, 2008, 2009 CSC, KTH
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

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>

#include "../config.h"

#include "music/error.hh"
#include "application_mapper.hh"

extern "C" {
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
};

using std::string;
using std::ifstream;

// Implementation-dependent code

#ifdef HAVE_RTS_GET_PERSONALITY
#define BGL
#else
#ifdef HAVE_OMPI_COMM_FREE
#define OPENMPI
#else
#define MPICH
#endif
#endif

#ifdef BGL
#include <rts.h>
#endif

int
getRank (int argc, char *argv[])
{
#ifdef BGL
  BGLPersonality p;
  rts_get_personality (&p, sizeof (p));
  int pid = rts_get_processor_id ();
  unsigned int rank, np;
  rts_rankForCoordinates (p.xCoord, p.yCoord, p.zCoord, pid, &rank, &np);
  return rank;
#endif
#ifdef MPICH
  int rank;
  const std::string rankopt = "-p4rmrank";
  for (int i = argc - 2; i > 0; --i)
    if (argv[i] == rankopt)
      {
	std::istringstream iss (argv[i + 1]);
	iss >> rank;
	return rank;
      }
  return 0;
#endif
#ifdef OPENMPI
  char* vpid = getenv ("OMPI_MCA_ns_nds_vpid");
  if (vpid == NULL)
    vpid = getenv ("OMPI_COMM_WORLD_RANK");
  if (vpid == NULL)
    MUSIC::error ("getRank: unable to determine process rank");
  std::istringstream iss (vpid);
  int rank;
  iss >> rank;
  return rank;
#endif
}


#ifdef MPICH
std::string
getSharedDir ()
{
  std::ostringstream dirname;
  char* musicSharedDir = getenv ("MUSIC_SHARED_DIR");
  if (musicSharedDir)
    dirname << musicSharedDir;
  else
    {
      char* home = getenv ("HOME");
      dirname << home;
    }
  return dirname.str ();
}
#endif


std::istream*
getConfig (int rank, int argc, char** argv)
{
#ifdef MPICH
  std::ostringstream fname;
  fname << getSharedDir () << "/.musicconf";
  std::string confname;
  if (rank == 0)
    {
      std::ofstream f (fname.str ().c_str ());
      confname = argv[1];
      f << confname;
    }
  else
    {
      std::ifstream f (fname.str ().c_str ());
      f >> confname;
    }
  return new std::ifstream (confname.c_str ());
#else
  return new ifstream (argv[1]);
#endif
}


// Generic code

void
usage (int rank)
{
  if (rank == 0)
    {
      std::cerr << "Usage: music [OPTION...] CONFIG" << std::endl
		<< "`music' launches an application as part of a multi-simulator job." << std::endl << std::endl
		<< "  -h, --help            print this help message" << std::endl << std::endl
		<< "Report bugs to <mikael@djurfeldt.com>." << std::endl;
    }
  exit (1);
}


void
launch (MUSIC::Configuration* config, char** argv)
{
  string binary;
  config->lookup ("binary", &binary);
  config->writeEnv ();
  string wd;
  if (config->lookup ("wd", &wd))
    chdir (wd.c_str ()); //*fixme* error checking
  execvp (binary.c_str (), argv);

  // if we get here, something is wrong
  perror ("MUSIC");
  exit (1);
}


int
main (int argc, char *argv[])
{
  int rank = getRank (argc, argv);

  opterr = 0; // handle errors ourselves
  while (1)
    {
      static struct option longOptions[] =
	{
	  {"help",    no_argument,       0, 'h'},
	  {0, 0, 0, 0}
	};
      /* `getopt_long' stores the option index here. */
      int option_index = 0;

      // the + below tells getopt_long not to reorder argv
      int c = getopt_long (argc, argv, "+h", longOptions, &option_index);

      /* detect the end of the options */
      if (c == -1)
	break;

      switch (c)
	{
	case '?':
	  break; // ignore unknown options
	case 'h':
	  usage (rank);

	default:
	  abort ();
	}
    }

  std::istream* configFile = getConfig (rank, argc, argv);

  if (!configFile)
    {
      if (rank == 0)
	std::cerr << "Couldn't open config file " << argv[1] << std::endl;
      exit (1);
    }

  MUSIC::ApplicationMapper map (configFile, rank);
  
  launch (map.config (), argv);

  return 0;
}
