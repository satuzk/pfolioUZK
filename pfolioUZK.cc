/*
 * This file is part of pfolioUZK.
 *
 * pfolioUZK is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pfolioUZK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You can find a copy of the GNU General Public License
 * at <http://www.gnu.org/licenses/>.
 *
 */


#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <poll.h>

#include <boost/program_options.hpp>

namespace nPO = boost::program_options;

using namespace std;

#define mydebug

int exitCode = 0;

/**
 * return the list of cores allocated to this process
 */
void getCoreList(vector<unsigned short int>& allocatedCores) {
  cpu_set_t mask;
  allocatedCores.clear();

  sched_getaffinity(0,sizeof(cpu_set_t),&mask);

  for (int i=0; i<sizeof(cpu_set_t)<<3; ++i)
    if (CPU_ISSET(i,&mask))
      allocatedCores.push_back(i);
}


cpu_set_t affinityMask(const vector<unsigned short int>& cores) {
  cpu_set_t mask;

  CPU_ZERO(&mask);
  for (size_t i=0; i<cores.size(); ++i)
    CPU_SET(cores[i],&mask);

  return mask;
}

/**
 * select the cores we'll run on
 *
 * @parm nbCore the number of cores we may use
 * @return the number of cores that we're actually using
 */
int selectCores(int nbCore, vector<unsigned short int> &cores)  {
  getCoreList(cores);

  if (nbCore==0 || cores.size()<=nbCore)
    	return cores.size();

  // the user wants us to use only nbCore cores
  // only select the first nbCore cores
  cores.erase(cores.begin()+nbCore,cores.end());

  assert(cores.size()==nbCore);

  cpu_set_t mask=affinityMask(cores);
  sched_getaffinity(0,sizeof(cpu_set_t),&mask);

  return nbCore;
}

/**
 * replace in s each occurrence of "keyword" by the string
 * representation of value
 */
template<typename T>
void replaceKeyword(string& s, const string& keyword,  const T& value) {
  ostringstream tmp;
  int pos;
  int keywordLength,valueLength;

  tmp << value;

  pos=0;
  keywordLength=keyword.length();
  valueLength=tmp.str().length();

  while ((pos=s.find(keyword,pos))!=string::npos) {
    s.replace(pos,keywordLength,tmp.str());
    pos+=valueLength;
  }
}

template<typename T>
void setEnvVar(const string var, const T val) {
  ostringstream tmp;
  
  tmp << val;
  if (setenv(var.c_str(),tmp.str().c_str(),true))
    throw runtime_error("setenv failed");
}

/**
 * parse <cmd> to generate a array of arguments <argv> suitable for execv
 */
void splitCommandLine(vector<char*>& argv, const string& cmd) {
  string word;
  size_t i=0;

  argv.clear();
  do {
    while (i<cmd.length() && isspace(cmd[i]))
      ++i;

    word.clear();

    while (i<cmd.length() && !isspace(cmd[i]))
      word+=cmd[i++];

    if(!word.empty()) {
      // don't care about the memory leak
      argv.push_back(strdup(word.c_str()));
    }
  }
  while(i<cmd.length());

  argv.push_back(NULL);
}

// the first solver to answer must lock the other solvers output
bool lockOutput=false;

struct Solver {
  string name;
  string authors;
  string header;
  string commandLine;
  pid_t pid;
  int output;
  bool hangup,term;

  char buffer[1024];
  int nbCharInBuf;
  char answerLineType; // 's', 'v' or 0
  string sLine;
  bool answering;
  // allocated cores
  vector<unsigned short int> cores;

  Solver(const string& name, const string& authors,
         const string& header, const string& commandLine):
    name(name), authors(authors), header(header), commandLine(commandLine) {

    hangup=false;
    term=false;

    pid=0;
    output=0;

    buffer[0]='\n';
    nbCharInBuf=1;

    answerLineType=0;
    answering=false;
  }

  /**
   * allocate a subset of the cores to this solver (nb cores starting
   * from list[first])
   */
  void chooseCores(const vector<unsigned short int>& list, 
		   size_t first, size_t nb) {
    cores.clear();
    for (size_t i=first;i<first+nb && i<list.size();++i)
      cores.push_back(list[i]);
  }

  /**
   *
   */
  void selectCores() {
    cpu_set_t mask=affinityMask(cores);
    if (sched_setaffinity(0,sizeof(cpu_set_t),&mask)!=0)
      perror("sched_setaffinity failed: ");

#ifdef debug
    vector<unsigned short int> list;
    getCoreList(list);
    cout << "running on core(s)";
    for (size_t i=0;i<list.size();++i)
      cout << ' ' << list[i];
    cout << "\n";
#endif
  }

  /**
   * start solvers[id]
   */
  void startSolver(const string& benchName) {
    int pipeDesc[2];
    pid_t pid;

    cout << "c Starting " << name;
    if (cores.size()) {
      cout << " on core(s)";
      for (size_t i=0;i<cores.size();++i)
	cout << ' ' << cores[i];
    }
    cout << "\n";
    if (!header.empty())
      cout << header << endl;

    pipe(pipeDesc);
    output=pipeDesc[0];

    // create tmp dir
    string solverTmpDir="/tmp";

    string command=commandLine;

    replaceKeyword(command,"BENCHNAME",benchName);
    //replaceKeyword(command,"RANDOMSEED",seed);
    replaceKeyword(command,"TMPDIR",solverTmpDir);

    setEnvVar("TMPDIR",solverTmpDir);

    vector<char*> argv;
    splitCommandLine(argv,command);

    pid=fork();
    if (pid==0) {
      
      // child process
      close(pipeDesc[0]);

      //set tmpdir

      // redirect
      if (dup2(pipeDesc[1],1)<0)
	perror("failed to redirect stdout");

      if (dup2(pipeDesc[1],2)<0)
	perror("failed to redirect stderr");

      selectCores();

      execv(argv[0],&argv[0]);

      cout << "failed to start solver: " << commandLine << endl;
      perror("reason: ");
      exit(99);
    } else if(pid>0) {
	// parent process
	this->pid=pid;
	close(pipeDesc[1]);
    } else
	cout << "Failed to start this solver: " << strerror(errno) << endl;
  }

  /**
   * to be called when the solver terminates
   */
  void terminated(int status) {
    //pid=0;
    //output=0;
    if (WEXITSTATUS(status)==99)
      cout << "failed to start solver: " << name << endl;

    term=true;
  }

  /**
   * read some solver output
   *
   * returns false for EOF
   */
  bool read() {
    int n=::read(output,buffer+nbCharInBuf,sizeof(buffer)-nbCharInBuf);
    
    if(n<0) {
      //perror("read()->");
      return errno==EINTR;
    }

    if(n==0) {
      close(output);
      output=0;

      return false;
    }

    // once a solver has started answering, we just ignore each other
    // solver data
    if (lockOutput && !answering)
      return true;

    n+=nbCharInBuf;
    char* end=buffer;
    
    if (answerLineType) {
      while (end<buffer+n && *end!='\n')
	++end;

      if (answerLineType=='s')
	readSLine(buffer,end-buffer,end<buffer+n);
      else {
	write(buffer,end-buffer);
	if (end<buffer+n)
	  write("\n",1);
      }

      if (end!=buffer+n)
	answerLineType=0;      
    }

    for (char* p=end; p<buffer+n-2; )
      if (*p=='\n' && (p[1]=='s' || p[1]=='v') && p[2]==' ') {
	answerLineType=p[1];

	char *q=p+1;
	while (q<buffer+n && *q!='\n')
	  ++q;

	assert(q-p-1>0);

	if (answerLineType=='s')
	  readSLine(p+1,q-p-1,q<buffer+n);
	else
	  write(p+1,q-p-1);

	if(answerLineType=='v') {
	  lockOutput=true;
	  answering=true;

	  //	  cout << "SOLVER: " << name << "(" << pid << ")" << endl;
	}

	if(q<buffer+n) {
	  write("\n",1);
	  answerLineType=0;
	} else {
	  nbCharInBuf=0;
	  return true;
	}

	p=q;
      } else
	++p;

    if (n>=1 && buffer[n-1]=='\n') {
      nbCharInBuf=1;
      buffer[0]='\n';
    } else if (n>=2 && buffer[n-2]=='\n' && (buffer[n-1]=='s' || buffer[n-1]=='v')) {
	nbCharInBuf=2;
	buffer[0]='\n';
	buffer[1]=buffer[n-1];
    } else
	nbCharInBuf=0;

    return true;
  }

  void readSLine(const char* p, int len, bool complete) {
    sLine.append(p,len);

    string ans=sLine.substr(2,3);
	  
    if (ans=="SAT" || ans=="UNS") {
      lockOutput=true;
      answering=true;

      if (ans=="SAT")
	exitCode = 10;
      else
	exitCode = 20;
	
      if (complete)
	write(sLine.c_str(),sLine.size());
    }
  }

  /**
   * handle partial writes and EINTR
   */
  void write(const char* buf, int len) {
    const char* p = buf;
    int n;

    do {
      n=::write(1,p,len);
      if(n<0) {
	if(errno==EINTR)
	  continue;

	perror("write failed: ");
	break;
      }

      len-=n;
      p+=n;
    }
    while (len);
  }
};


vector<Solver*> solvers;

void sigChildHandler(int signum, siginfo_t* siginfo, void* ucontext) {
  int status;
  pid_t pid=waitpid(-1,&status,0);
  int id=0;

  for (int i=0;i<solvers.size();++i) 
    if (solvers[i]->pid==pid) {
      id=i;
      solvers[i]->terminated(status);
      break;
    }

#ifdef debug
  cout << "process terminated (id=" << id+1 << " pid=" << pid << ")\n";
#endif
}

/**
 * loop and read the solvers answers
 */
void runSolvers(const string& filename) {
  struct sigaction handler;
  bool killed=false;

  handler.sa_sigaction=sigChildHandler;
  sigemptyset(&handler.sa_mask);
  handler.sa_flags=SA_SIGINFO;

  sigaction(SIGCHLD,&handler,NULL);

  for (int id=0; id<solvers.size(); ++id)
    solvers[id]->startSolver(filename);
     
  cout.flush();

  struct pollfd* pollData=new struct pollfd[solvers.size()];
  vector<size_t> pollIndexToSolverId(solvers.size());

  int nfds;

  while(true) {
    bool modif=false;

    nfds=0;
    for (int id=0; id<solvers.size(); ++id) {
      if (solvers[id]->hangup)
	continue;
      
      pollData[nfds].fd=solvers[id]->output;
      pollData[nfds].events=POLLIN;
      pollIndexToSolverId[nfds]=id;

      ++nfds;
    }

    if (!nfds)
      break;

    do {
      int ret=poll(pollData,nfds,-1);

      if (ret<0) {
	if (errno==EINTR)
	  continue;

	perror("poll failed: ");
	exit(1);
      }

      for (int p=0;p<nfds;++p)
	if (!pollData[p].revents)
	  continue;
	else {
	  if (pollData[p].revents & POLLIN) {
	    int id=pollIndexToSolverId[p];
#ifdef debug
	    cout << "POLLIN(" << id << ")\n";
#endif
	    if (!solvers[id]->read()) {
	      cout << "EOF for solver " << id << " pid=" 
		   << solvers[id]->pid << endl;
	      
	      modif=true;	     
	    }
	  }

	  if (pollData[p].revents & (POLLERR|POLLHUP|POLLNVAL)) {
	    int id=pollIndexToSolverId[p];

#ifdef debug
	    if (pollData[p].revents & POLLERR)
	      cout << "POLLERR(" << id << ")\n";
	    if (pollData[p].revents & POLLHUP)
	      cout << "POLLHUP(" << id << ")\n";
	    if (pollData[p].revents & POLLNVAL)
	      cout << "POLLNVAL(" << id << ")\n";
#endif
	    // we can have some last data
	    while (solvers[id]->read());

	    modif=true;
	    close(solvers[id]->output);
	    solvers[id]->hangup=true;
	    solvers[id]->output=0;
#ifdef debug
	    cout << "lost output of solver " << id+1
		 << " pid=" << solvers[id]->pid << endl;
#endif
	  }
	}

      if (lockOutput && !killed) {
	// one solver is answering, kill all other solvers
	for (int k=0; k<solvers.size(); ++k)
	  if (!solvers[k]->term && !solvers[k]->answering) {
	      kill(solvers[k]->pid,SIGKILL);
	  }
	killed=true; // do it once
      }
    }
    while (!modif);
  }
}

long timevaldiff(struct timeval* starttime, struct timeval* finishtime) {
  long msec;
  msec = (finishtime->tv_sec - starttime->tv_sec)*1000;
  msec += (finishtime->tv_usec - starttime->tv_usec)/1000;
  return msec;
}

int main(int argc, char **argv) {
  vector<string> inputFiles;
  timeval start, end;
  int nbCore; // number of cores we may use

  gettimeofday(&start, 0);

  nPO::options_description desc("Allowed options"),hidden,all;

  desc.add_options()
    ("help,h", "produce help message")
    ("nbcore,c", nPO::value<int>(&nbCore)->default_value(0),"number of processing units to use (allowed: 1, 2,4, or 8")
    ;

  hidden.add_options()
    ("input-file",nPO::value<vector<string> >(&inputFiles), "input file")
    ;

  all.add(desc).add(hidden);

  nPO::variables_map vm;

  nPO::positional_options_description positionalOptions;
  positionalOptions.add("input-file", -1);

  nPO::store(nPO::command_line_parser(argc,argv).options(all).
             positional(positionalOptions).run(),vm);

  nPO::notify(vm);    

  if (vm.count("help")) {
    cout << desc << "\n";
    exit(1);
  }

  if(inputFiles.size()!=1) {
    cout << "syntax: ppfolioUZK [options] filename\n";
    exit(1);
  }

  vector<unsigned short int> cores;
  nbCore=selectCores(nbCore,cores);

  cout << "c This is a portfolio solver pfolioUZK, a naive parallel portfolio sat solver\n";
  cout << "c based on the implementation of pico-portfolio by Olivier Roussel 2010\n";

#ifdef debug
  Solver test("output test","",
	      "",
	      "./testOutput");
  solvers.push_back(&test);
#endif


  string  plingelingCmd =  "bin/plingeling -t 4 BENCHNAME";

  Solver
    satUZKp("satUZK 34p","Alexander van der Grinten and Andreas Wotzlaw",
             "",
             "bin/satUZK -preproc-adaptive -budget 900 -show-model BENCHNAME"),

    satUZKsp("satUZK 34sp","Alexander van der Grinten and Andreas Wotzlaw",
             "",
             "bin/satUZK_wrapper satUZK -preproc-adaptive -budget 900 BENCHNAME"),

    glucose("glucose 2.0","Gilles Audemard and Laurent Simon",
             "",
             "bin/glucose_wrapper BENCHNAME"),

    contrasat("minisat hack: contrasat", "Allen van Gelder",
                "",
                "bin/contrasat BENCHNAME"),

    lingeling("lingeling 587f-4882048-110513","Armin Biere",
              "",
              "bin/lingeling BENCHNAME"),

    plingeling("plingeling 587f-4882048-110513","Armin Biere",
	       "",
	       plingelingCmd.c_str()),

    tnm("TNM 2009","Wanxia Wei and Chu Min Li",
	"",
	"bin/TNM BENCHNAME"),

    sparrow("sparrow2011","Dave Tompkins", 
            "",
            "bin/sparrow2011 BENCHNAME 27"),

    mphasesatm("MPhaseSAT_M","Jingchao Chen",
	       "",
	       "bin/MPhaseSAT_M BENCHNAME"),

    march("march_hi 2009","Marijn Heule and Hans Van Maaren",
          "",
          "bin/march_hi BENCHNAME");


  string command1 = "bin/isUniform.sh " + inputFiles[0],
         command2 = "bin/isTooLarge.sh " + inputFiles[0];

  switch(nbCore) {
  case 1:
    if(system(command1.c_str())) {
      cout << "c I guess it is a uniform cnf formula" << endl;
      solvers.push_back(&sparrow);
      solvers.push_back(&mphasesatm);
      solvers.push_back(&march);
      solvers.push_back(&tnm);

      sparrow.chooseCores(cores,0,1);
      mphasesatm.chooseCores(cores,0,1);
      march.chooseCores(cores,0,1);
      tnm.chooseCores(cores,0,1);

    } else {
      solvers.push_back(&satUZKp);
      solvers.push_back(&lingeling);
      solvers.push_back(&tnm);
      solvers.push_back(&mphasesatm);
	
      satUZKp.chooseCores(cores,0,1);
      lingeling.chooseCores(cores,0,1);
      tnm.chooseCores(cores,0,1);
      mphasesatm.chooseCores(cores,0,1);
    }
    break;

  case 2:
    solvers.push_back(&glucose);
    solvers.push_back(&satUZKp);
    solvers.push_back(&mphasesatm);
    solvers.push_back(&tnm);

    satUZKp.chooseCores(cores,0,1);
    tnm.chooseCores(cores,0,1);
    glucose.chooseCores(cores,1,1);
    mphasesatm.chooseCores(cores,1,1);
    break;

  case 4:
    solvers.push_back(&glucose);
    solvers.push_back(&satUZKp);
    solvers.push_back(&lingeling);
    solvers.push_back(&contrasat);

    satUZKp.chooseCores(cores,0,1);
    lingeling.chooseCores(cores,2,1);
    glucose.chooseCores(cores,3,1);
    contrasat.chooseCores(cores,1,1);
    break;

   case 8:
    solvers.push_back(&glucose);
    solvers.push_back(&plingeling);
    solvers.push_back(&contrasat);

    glucose.chooseCores(cores,0,1);
    plingeling.chooseCores(cores,4,4);
    if (system(command2.c_str())) {
      solvers.push_back(&satUZKsp);
      satUZKsp.chooseCores(cores,2,1);
      contrasat.chooseCores(cores,1,1);
    } else
      contrasat.chooseCores(cores,2,1);

    break;

   default:
    cout << "Input error: only 1, 2, 4, or 8 are allowed!\n";
    exit(1);
  }

  cout << "c Using " << nbCore << " processing units\n";
  cout << "c This portfolio uses the following solvers:\n";
  for (int id=0; id<solvers.size(); ++id) {
    cout << "c [" << id+1 << "] " << solvers[id]->name 
	 << " (" << solvers[id]->authors << ")\n";
    if (solvers[id]->header.size())
      cout << solvers[id]->header << "\n";
    //cout << "\n";
  }
		    
  runSolvers(inputFiles[0]);

  for (int id=0; id<solvers.size(); ++id)
    if (solvers[id]->answering)
      cout << "c solver [" << id+1 << "] " << solvers[id]->name 
	   << " answered\n";

  gettimeofday(&end, 0);
  double delta = static_cast<double>(timevaldiff(&start,&end))/1000;
  cout << "c cpu time: " << delta << "s" << endl;
  exit(exitCode);
}
