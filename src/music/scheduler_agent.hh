#ifndef MUSIC_SCHEDULER_AGENT_HH

#include "music/music-config.hh"
#include "music/scheduler.hh"
#include "music/multibuffer.hh"

#if MUSIC_USE_MPI

namespace MUSIC {
  class Scheduler;
  typedef  std::vector< Scheduler::SConnection> SConnectionV;
  class SchedulerAgent
  {
  protected:
    Scheduler *scheduler_;
    SchedulerAgent(Scheduler *scheduler);
    class NextCommObject
    {
    public:
      NextCommObject():time(-1.0){};
      NextCommObject(double time_, Connector *connector_):time(time_), connector(connector_){};
      double time;
      Connector *connector;
      void reset(){time = -1.0;}
      bool empty(){return time < 0;}
    };
    virtual bool fillSchedule() = 0;
  public:
    virtual ~SchedulerAgent(){};
    virtual void initialize()=0;
    virtual bool tick(Clock& localTime)=0;
    virtual void finalize (std::set<int> &cnn_ports) = 0;
  };

  class MulticommAgent: public virtual SchedulerAgent
  {
    std::map<int, Clock> commTimes;
    int rNodes;
    Clock time_;

    MultiBuffer* multiBuffer_;
    std::vector<MultiConnector*> multiConnectors;    

    std::vector<NextCommObject> schedule;
    class Filter1
    {
      MulticommAgent &multCommObj_;
    public:
      Filter1(MulticommAgent &multCommObj);
      bool operator()(const Scheduler::SConnection &conn);
    };

    class Filter2
    {
      MulticommAgent &multCommObj_;
    public:
      Filter2(MulticommAgent &multCommObj);
      bool operator()(const Scheduler::SConnection &conn);
    };

  public:
    MulticommAgent(Scheduler *scheduler);
    ~MulticommAgent();
    void initialize();
    void createMultiConnectors (Clock& localTime,
				MPI::Intracomm comm,
				int leader,
				std::vector<Connector*>& connectors);
    bool tick(Clock& localTime);
    void finalize (std::set<int> &cnn_ports);

  private:
    bool fillSchedule();
    void fillSchedule( SConnectionV &candidates);
    SConnectionV::iterator  NextMultiConnection(SConnectionV::iterator &last_bound,
        SConnectionV::iterator last,
        std::map<int, Clock> &prevCommTime);
    void printMulticonn(Clock time,SConnectionV::iterator first, SConnectionV::iterator last);

    friend class Filter1;
    friend class Filter2;
    Filter1 *filter1;
    Filter2 *filter2;
  };
  class UnicommAgent: public virtual SchedulerAgent
  {
    NextCommObject schedule;
    bool fillSchedule();
  public:
    UnicommAgent(Scheduler *scheduler);
    ~UnicommAgent(){};
    void initialize(){};
    bool tick(Clock& localTime);
    void finalize (std::set<int> &cnn_ports);
  };
}
#endif

#define MUSIC_SCHEDULER_AGENT_HH
#endif
