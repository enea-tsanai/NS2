/* -*-	Mode:C++; c-basic-offset:3; tab-width:3; indent-tabs-mode:t -*- */
#define RCP_MODE
#define TFRC      /* define this if used with TFRC, undefine if used with P-AQM+ECF*/
#define MONITOR_NODE 20000000
/*****************************************************************************
* vbr_rateadapt.cc                                                          *
*                                                                           *
* Author: Arne Lie, NTNU Dept. of Telematics, Trondheim, Norway             *
* Date first created: October, 2005.                                        *
* Based on: mytraffictrace2.cc made by Ke ChihHeng, Taiwan                  *
* Modified to handle multiple input trace files to simulate rate adaptation *
* Modified to handle SVBR rate control as described by Hamdi/Roberts '97    *
* Modifed to handle rate adaptation based on P-AQM+ECF feedback             * 
* BURSTY variant: i.e. all packets from the frame are sent back-to-back     *
* (except for when TFRC is defined and used over TFRC agent)                *
*****************************************************************************/
#define APRIORI /* define this to enable one GOP apriori knowledge of Ropen */
//#define TEST_Q_XC_IN_GOP /* test code for LB(r,b) change inside GoP */
#ifdef TFRC
#define HDR_SIZE 36  /* This is for DCCP/TFRC + IP (12 + 4 + 20) */
#else
#define HDR_SIZE 28  /* This is for UDP + IP (8 + 20) */
#endif


#include <sys/types.h>
#include <sys/stat.h> 
#include <stdio.h>

#include "config.h"
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include "random.h"
#include "object.h" 
#include "trafgen.h"
// #include "myudp.h"

#define MAX_NODE 50  /* Maximum number of P-AQM routers sending ECF to this source */
#define MAX_RA_VARS 30 /* Maximum number of rate variations */

/* The following struct is used to hold all data about the transmitted video sequence */
struct tracerec {
  u_int32_t trec_time; /* inter-packet time (usec) */
  u_int32_t trec_size[MAX_RA_VARS]; /* frame size (bytes). AL: store all versions! */
  u_int32_t trec_type; /* packet type */ 
  u_int32_t trec_max; /* maximun fragmented size (bytes) */
  u_int32_t trec_source; /* AL: source number (i.e. "pointer" to which version of 
  video that is used for this frame) */
};

class vbrTraceFile : public NsObject {
  public:
	vbrTraceFile();
	void get_next(int&, struct tracerec&, int Q, int flag); /* called by TrafficGenerator
	* to get next record in trace.
	*/
	int setup();  /* initialize the trace file */ 
	int command(int argc, const char*const* argv);
	int get_a();		//added by smallko
	int num_extra_RA;
	int status_; 
	
  private:
	void recv(Packet*, Handler*); /* must be defined for NsObject */
	char *name_;  /* name of the file in which the binary trace is stored */
	int nrec_;    /* number of records in the trace file */
	struct tracerec *trace_; /* array holding the trace (AL: of all rate variants!) */
	int a_;
};

/* instance of a traffic generator.  has a pointer to the TraceFile
* object and implements the interval() function.
*/

class vbr_rateadapt2 : public TrafficGenerator {
  public:
	vbr_rateadapt2();
	int command(int argc, const char*const* argv);
	virtual double next_interval(int &);
  protected:
	void timeout();
	virtual void start();
	virtual void stop(); 
	int running2_;
	vbrTraceFile *tfile_;
	struct tracerec trec_;
	int ndx_;
	int f_, max_; // added by smallko
	int source_; // Added by AL
	int Q; // Added by AL 311005
	int Q_wait; // Added by AL 071205 to test imidiate Q-change, but not until current frame
	// is finished transmitted
	int Q_flag;
	int R_hist[20]; // store up to last 20 frame sizes
	int frameIndex;
	double r_, b_, q_; // The Hamdi LB(r,b) and q parameters
	double GoP_, fps_; // Needed to scale the values correctly
	double lb_X; // The Leaky Bucket X(k) value
	double lb_R; // The monitored R(ate) of the last GOP
	void init();
	int is_mine; // AL: added to differ between the investigated video, and the simple tracers
	/* The following variables are for ECF rate adaptivity */
	double ecf_new;
	int from_f;
	double new_rate, new_b;
	double ecf_store[MAX_NODE];  /* can handle 10 P-AQM nodes */
	double rate_store[MAX_NODE]; /* ---------  "  ----------- */
	double ECF_rateadapt[MAX_NODE];
	double rate_;   /* send rate during on time (bps) */
	int numbFrames;
	double old_rate_coeff, new_rate_coeff;
	double b_factor;
	int whoami_i;
	int numbGops;
	int update_count;
	int sub_sample;
};


static class vbrTraceFileClass : public TclClass {
  public:
	vbrTraceFileClass() : TclClass("vbrTracefile2") {}
	TclObject* create(int, const char*const*) {
	  return (new vbrTraceFile());
	}
} class_vbr_tracefile;

vbrTraceFile::vbrTraceFile() : status_(0)
{
  a_=0;
  //printf("In vbrTraceFile constructor\n");
}

int vbrTraceFile::get_a()
{
  //printf("In ::get_a() \n");
  return a_;
}

void vbrTraceFile::get_next(int& ndx, struct tracerec& t, int Q, int flag_active)
{
  //printf("get_next\n");
  /* Note that t is a local pointer to trec_ (and not to *t from setup(), 
  so the lines below actually performs data copying from
  the big trace (all data) to this single frame stored by trec_ */
  /* Note also that trec_ only needs one element of size, and will always
  use position 0 to store, whatever Q-value is in use */
  t.trec_time = trace_[ndx].trec_time;
  //printf("::get_next: next time is %d, ndx=%d, Q=%d\n",t.trec_time,ndx,Q);
  // Next time is fixed to 40000us if 25fps
  t.trec_size[0] = trace_[ndx].trec_size[Q-2]; // Position zero is Q=2
  t.trec_type = trace_[ndx].trec_type;
  t.trec_max = trace_[ndx].trec_max;
  t.trec_source = trace_[ndx].trec_source; /* AL: new 201005 */
  //printf("ndx = %d, flag_active = %d, trec_type=%d\n",ndx, flag_active,t.trec_type);
  if (++ndx == nrec_) {
	ndx = 0; /* reached end of video trace */
	double t = Scheduler::instance().clock();
	//printf("Reached end of video tracefile at t=%6.5f, nrec_=%d, wrap pointer...\n",t,nrec_);
	if (flag_active == 1) {
	  a_= 1; /* This will force the source to stop sending!! */
	  printf("Primary Video source reached end, will be stopped now\n");
	}
  }
}

int vbrTraceFile::setup()
{
  tracerec* t;
  struct stat buf;
  int i, j;
  unsigned long time, size, type, max, source;
  FILE *fp;
  char tracestring[1000], *tracevalue;
  
  if (! status_) {
	status_ = 1;
	
	// printf("In ::setup()\n");
	/* Note that original traffictrace.cc had "rb" to read the binary trace file */
	/* The first version from "henry" also used binary data. In addition, the first
	version supported multipple instances but one file read, but had masked
	out the random set point within the file. */
	// printf("%s\n",name_);
	if((fp = fopen(name_, "r")) == NULL) {
	  printf("can't open file %s\n", name_);
	  return -1;
	}
	/* 
	* The following loop just for counting the number of frames in trace file 
	*/
	nrec_ = 0;
	while (!feof(fp)){
	  //   fscanf(fp, "%ld%ld%ld%ld%ld", &time, &size, &type, &max, &source);
	  // if (fgets(tracestring,1000,fp) == NULL)
	  //   printf("error, trace file is empty?\n");
	  // else
	  fgets(tracestring,1000,fp);
	  nrec_++;
	}	
	//nrec_=nrec_-2;	
	nrec_=nrec_ - 1;	
	//printf("%d records\n", nrec_);
	
	/*
	* Count the number of possible media rate variations
	*/
	tracevalue = strtok(tracestring," "); /* read the us */
	tracevalue = strtok(NULL," "); /* read the size (first one) */
	tracevalue = strtok(NULL," "); /* read the type */
	tracevalue = strtok(NULL," "); /* read the max size per packet */
	//	for (i=0;i<num_variants;i++) {
	  num_extra_RA = 0;
	  while((tracevalue = strtok(NULL, " "))!=NULL)
		num_extra_RA++;
	  if (num_extra_RA < MAX_RA_VARS)
		printf("Number of extra RA variants are %d\n",num_extra_RA);
	  else {
		printf("number of RA variants %d are greater than max value, increase MAX_RA_VARS\n"
		,num_extra_RA);
		return -1;
	  }
	  /*
	  * The following loop is for reading the tracefile (again) and store all
	  * information in memory
	  */
	  rewind(fp);
	  trace_ = new struct tracerec[nrec_];
	  /* Read whole video2.dat file and store in memory */
	  for (i = 0, t = trace_; i < nrec_; i++, t++){
		//fscanf(fp, "%ld%ld%ld%ld%ld", &time, &size, &type, &max, &source);
		//printf("i = %d\n",i);
		fgets(tracestring,1000,fp);
		tracevalue = strtok(tracestring," "); /* read the us */	    
		t->trec_time = atoi(tracevalue);
		
		tracevalue = strtok(NULL," "); /* read the size (first one) */
		t->trec_size[0] = atoi(tracevalue);
		
		tracevalue = strtok(NULL," "); /* read the type */
		t->trec_type = atoi(tracevalue);
		
		tracevalue = strtok(NULL," "); /* read the max size per packet */
		t->trec_max = atoi(tracevalue);
		
		//t->trec_source = source; /* AL: new 201005 */
		for (j=1;j<=num_extra_RA;j++) {
		  tracevalue = strtok(NULL," "); /* read the size (first one) */
		  t->trec_size[j] = atoi(tracevalue);
		  //printf("Extra size value is %d\n",t->trec_size[j]);
		}
	  }
	  //printf("leaving ::setup()\n");
	  return 0; // The return value is the index start position in tracerec!!
  }
  else {
	//	Random::seed_heuristically(0);
	int rnd_time_start = int(Random::uniform((double)nrec_)+.0);
	rnd_time_start = int(Random::uniform((double)nrec_)+.0);
	//printf("In ::setup() other than first one, RND start frame = %d\n",rnd_time_start);
	/* pick a random starting place in the trace file */
	return rnd_time_start;
  }
}

void vbrTraceFile::recv(Packet*, Handler*)
{
  /* shouldn't get here */
  abort();
}

int vbrTraceFile::command(int argc, const char*const* argv)
{    
  if (argc == 3) {
	if (strcmp(argv[1], "filename") == 0) {
	  name_ = new char[strlen(argv[2])+1];
	  strcpy(name_, argv[2]);
	  //printf("In vbrTraceFile::command() filename \n");
	  return(TCL_OK);
	}
  }
  return (NsObject::command(argc, argv));
}

/**************************************************************/
/* Below this line is the Class vbr_rateadapt2 methods        */
/**************************************************************/

static class vbr_rateadapt2Class : public TclClass {
  public:
	vbr_rateadapt2Class() : TclClass("Application/Traffic/eraVbrTrace") {}
	TclObject* create(int, const char*const*) {
	  return(new vbr_rateadapt2());
	}
} class_vbr_traffictrace;

vbr_rateadapt2::vbr_rateadapt2()
{
  tfile_ = (vbrTraceFile *)NULL;
  //printf("In vbr_rateadapt2() constructor\n");
  bind_bw("r_", &r_); // r_ in (bits/s)
	bind("b_", &b_);    // b in bits
	bind("q_", &q_);
	bind("GoP_", &GoP_);
	bind("fps_", &fps_);
	bind("running_", &running2_);
	// printf("Init1 constructor: r_=%4.3f, b_=%4.3f, lb_X=%4.3f, Q=%d\n",r_,b_,lb_X,Q);
}

void vbr_rateadapt2::start()
{
  //printf("Yes, is in ::start()!\n");
  init();
  running_ = running2_ = 1;
  //printf("Yes, is in ::start() 2!\n");
  // timeout();
  nextPkttime_ = next_interval(size_);
  timer_.resched(nextPkttime_);
  
  //printf("Yes, is in ::start() 3!\n");
}

void vbr_rateadapt2::stop()
{
  if (running_)
	timer_.cancel();
  running_ = running2_ = 0;
}


void vbr_rateadapt2::init()
{
  int i;
  
  //printf("In ::init() \n");
  
  r_ = r_ * GoP_ / (8*fps_) ; // r_ now has dimensions of bytes/GOP
  b_factor = b_; // b_ is initiated as b_factor, where b_ = b_factor * r_
  b_ = b_factor * r_; // b_ in bytes
  lb_X = b_ * 0.5; // Init buffer to half value
  lb_R = 0.0;
  Q_wait = Q = 2 * (int) q_; // New 200306: start with much higher Q value to have smoother start
  if (Q > 31)
	Q_wait = Q = 31;
  Q_flag = 0;
  numbFrames = 0;
  for (i=0;i<MAX_NODE;i++) {
	ECF_rateadapt[i] = 1.0;
	rate_store[i] = r_;
  }
  for (i=0;i<20;i++)
	R_hist[i] = 0;
  new_rate = rate_ = r_;
  #ifdef TFRC
  new_rate = 0.0; // New 130306: to filter multiple feedbacks per GoP
  new_rate = r_; // New 151106: error model causes feedback to be absent
  #endif
  new_b = b_;
  old_rate_coeff = 0.0;
  new_rate_coeff = 1.0;
  ecf_new = 1.0;
  running2_ = running_;
  is_mine = 0;
  frameIndex = 0;
  numbGops = 0;
  update_count = 1; // Modified 151106: start with the last averaged value, instead from zero
  sub_sample = 0;
  if (tfile_->status_ == 0) {
	is_mine = 1;
  }
  //printf("::init(): r_=%4.3f, b_=%4.3f, lb_X=%4.3f, Q=%d, is_mine=%d\n",r_,b_,lb_X,Q,is_mine);
  if (tfile_) {
	ndx_ = tfile_->setup(); /* call method of Class vbrTraceFile */
	//printf("Start position in trace file is %d\n",ndx_);
  }
}



void vbr_rateadapt2::timeout()
{
  unsigned long x_, y_, i;
  int size_w_oh; // AL: Size with overhead
  double e1, e2;
  double Rtemp, Qtemp, r_open;
  double x_temp;
  
  //printf("==================>timeout, max_=%d\n", max_);
  if (! running_)
	return;
  
  if (tfile_->get_a()==1 && is_mine == 1){
	running_=0;
	return;
  }
  
  x_=size_/max_; // AL: number of complete (full sized) IP packets
  y_=size_%max_; // AL: the remaining rest part
  
  //printf("size_:%ld, max_:%ld, x_:%d, y_:%d, source_:%d, Q:%d\n", size_, max_, x_, y_, source_,Q);
  
  if(f_==0 || f_==1){
	agent_->set_prio(0);
  }
  else if(f_==2){
	agent_->set_prio(1);
  }
  else if(f_==3){
	agent_->set_prio(2);
  }
  else {
	agent_->set_prio(2);
  }
  
  if (f_ != 0)
	numbFrames++;
  
  #ifndef TFRC
  agent_->set_pkttype(PT_VIDEO);
  #endif
  agent_->set_frametype(f_);
  
  // AL: Here is the Tx of packets, all packets of same frame is sent at *same* time
  // First the max size packets (if any)
  // (normally, as in H264, the packets are divided on slice boundary, and not as this!!)
  size_w_oh = 0;
  if(x_ > 0){
	for(i=0 ; i<x_; i++) {
	  // agent_->Qrecv = Q;
	  agent_->set_quant(Q);
	  agent_->sendmsg(max_+HDR_SIZE); //e.g. udp header + ip header ( 8 + 20 =28 bytes)
	  size_w_oh += max_+HDR_SIZE;
	}
  }
  // here goes the rest packet (normally, as in H264, the packets are divided on slice boundary)
  if(y_!=0) {
	agent_->set_quant(Q);
	agent_->sendmsg(y_+HDR_SIZE); 
	#ifdef TFRC
	size_w_oh += max_+HDR_SIZE; /* Because TFRC uses fixed packet sizes, 
						thus partial packets are stuffed */
						#else
						size_w_oh += y_+HDR_SIZE;
						#endif
  }
  Q = Q_wait;
  /*
  * New 31.10.2005: Hamdi/Roberts LB(r,b) algorithm modified for RA!!
  * Trigger this always when current frame is a H-frame! All H-frames are equal in
  * size, regardless of Q, and a new Q-value can therefore be selected AFTER the H-frame
  * is transmitted. This will cause the writing of Q-value to sd_be file to be wrong for
  * the H-frame, but is correct for the following I-frame.
  *
  * Note that the buffer and rates dimensions are in Bytes and Bytes/GOP.
  *
  * If H-frames are NOT to be afterall, the function must trigger on I-frames. The problem then
  * is that the I-frames have different size. perhaps action is to take place after last frame
  * in GoP is transmitted. This forces the knowledge of static sized GoP.
  */
  //lb_X += size_w_oh; // AL: Must verify that this value shall take IP/UDP packet overhead in...
  //printf("Inside GOP: lb_X=%4.3f, whoami_i=%d\n",lb_X,whoami_i);
  #ifndef TEST_Q_XC_IN_GOP
  lb_R += size_w_oh; // AL: Must verify that this value shall take IP/UDP packet overhead in...
  #else
  if (frameIndex>GoP_)
	frameIndex = 0;
  lb_R -= (double)R_hist[frameIndex];
  R_hist[frameIndex] = size_w_oh;
  lb_R += size_w_oh;
  if (whoami_i == 5) {
	//	printf("numbFrames=%d, frameIndex=%d, lb_R=%4.1f\n",numbFrames,frameIndex,lb_R);
  }
  frameIndex++;    
  #endif
  if (f_==0) {
	numbGops++;
	//double t = Scheduler::instance().clock();
	//printf("===>>>>>t=%4.3f, numbGops=%d\n",t,numbGops);
  }
  #ifndef TEST_Q_XC_IN_GOP
  if (f_==0 && numbGops > 1) {
	r_ = r_ * old_rate_coeff + new_rate * new_rate_coeff;
	b_ = b_ * old_rate_coeff + new_b * new_rate_coeff;
	//double t = Scheduler::instance().clock();
	//printf("===>>>>>>>>>>>>t=%4.3f, numbGops=%d, whoami_i=%d\n",t,numbGops,whoami_i);
	#else
	if ((f_==0 && numbGops > 1) || Q_flag==1) {
	  if (Q_flag==1) {
		if (whoami_i == 5) {
		  printf("Q_flag = 1\n");
		}
		old_rate_coeff = numbFrames / GoP_;
		new_rate_coeff = (GoP_ - numbFrames)/GoP_;
		Q_flag = 0;
	  }
	  #endif
	  
	  old_rate_coeff = 0.0;
	  new_rate_coeff = 1.0;
	  //printf("Before -r:lb_X=%4.3f, r_=%4.3f, b_=%4.3f, lb_R=%4.3f\n",lb_X,r_,b_,lb_R);	
	  lb_X -=  r_;                  // Eq. 5
	  if (lb_X < 0.0) {
		lb_X = 0.0;   // Eq. 5
		//printf("vvv LB underrun vvv\n");
	  }
	  lb_X += lb_R;                 // Eq. 5
	  if (lb_X > b_) {
		lb_X = b_;     // Eq. 5
		//printf("                                      ^^^ LB overrun ^^^\n");
	  }
	  //printf("After -r:lb_X=%4.3f\n",lb_X);
	  //lb_X = b_*0.5; /* TO BE REMOVED: force open loop */
	  x_temp = lb_X/b_;
	  e1 = 1 - pow((1-x_temp),5);
	  e2 = pow(x_temp,5);
	  //printf("x_temp=%4.3f, e1=%4.3f, e2=%4.3f\n",x_temp,e1,e2);
	  if (e2 > 1.0)
		e2 = 1.0;
	  r_open = lb_R*Q/(q_+0.0000000001); /* This makes a good estimate of the Rate of the Q=q_ variant LAST period */
	  /* But is it a good estimate for next GOP if the scene complexity changes substantially? No... */
	  /* If pre-coded media, read from file instead! */
	  #ifdef APRIORI
	  /* This piece of code fetches the size of the next GOP with Q=q_ */
	  /* and overrides the r_open estimate. Simulate pre-coded media possibilities. */
	  /* This is not an option for live media encoding, except if not buffering at least one GoP at Tx */
	  // printf("APRIORI start\n");
	  r_open = 0;
	  int save_ndx = ndx_, loopcnt=0, hdrcnt=0,nhdr=0;
	  tfile_->get_next(ndx_, trec_,(int)q_,0);
	  r_open += trec_.trec_size[0];
	  nhdr = ((int)(1.0*trec_.trec_size[0]/trec_.trec_max)+1);
	  hdrcnt = nhdr*HDR_SIZE;
	  do {
		loopcnt++;
		tfile_->get_next(ndx_, trec_,(int)q_,0);
		r_open += (double)trec_.trec_size[0];
		nhdr = ((int)(1.0*trec_.trec_size[0]/trec_.trec_max)+1);
		//printf("nhdr=%d, frameType=%d\n",nhdr,trec_.trec_type);
		hdrcnt += nhdr*HDR_SIZE;
	  } while(trec_.trec_type!=0);
	  ndx_ = save_ndx;
	  r_open += hdrcnt;
	  #ifdef TFRC
	  r_open *= 1.25;
	  #endif
	  // printf("APRIORI end\n");
	  #endif
	  //printf("loopcnt=%d,Ropen=%f, hdrcnt=%d\n",loopcnt,r_open,hdrcnt);
	  r_ = new_rate;
	  b_ = new_b;
	  #ifdef TFRC
	  // Removed 151106:
	  //new_rate = 0.0; // New 130306: to filter multiple feedbacks per GoP
	  // Changed 151106: start from 1 instead from 0
	  update_count = 1; // New 130306: to filter multiple feedbacks per GoP
	  #endif
	  if (r_open > r_)
		Rtemp = (1-e1)*r_open+e1*r_; // Eq. 8
		else
		  Rtemp = e2*r_open+(1-e2)*r_; // Eq. 9
		  /* Note that below, r_open is used as if it is valid also for the next period!! */
		  /* For pre-encoded media, the correct r_open could be taken from the data! */
		  Qtemp = q_*r_open/(Rtemp+0.000001); // Eq. 11 and 12 (depend on eq 8 or 9 in use)
		  //printf("r_open=%4.3f, Rtemp=%4.3f, Qtemp=%4.3f\n",r_open,Rtemp,Qtemp);
		/* Input some hysterisis on Q change epoch aka C-code from Hamdi */
		if (Qtemp > tfile_->num_extra_RA + 4) {
		  Qtemp = tfile_->num_extra_RA + 4;
		}
		if (r_open > 1.05*r_ || r_open < 0.95*r_) {
		  #ifdef APRIORI
		  Q = (int) (Qtemp+0.5); // To find a good mapping here
		  #else
		  Q = (int) (Qtemp+0.7); // To find a good mapping here
		  #endif
		}
		//printf("r_ = %4.3f, b_ = %4.3f, Q=%d\n",r_,b_,Q);
		//Q = (int) floor(Qtemp); // To find a good mapping here
	//Q = (int) ceil(Qtemp); // To find a good mapping here
	//Q = 15; /* TO BE REMOVED: force max datarate */
	if (whoami_i == 5) {
	  double t = Scheduler::instance().clock();
	  //printf("\n===>t=%4.3f, Q(k+1)=%d, lb_R(k)=%4.3f, lb_X(k)=%4.3f, r_=%4.3f, b_=%4.3f, whoami_i=%d\n",t,Q,lb_R,lb_X,r_,b_,whoami_i);
	}
	if (Q > tfile_->num_extra_RA + 2) {
	  /*printf(" --VVV: Do not have RA variant with Q=%d, force to Q=%d for source %d\n",
	  Q,tfile_->num_extra_RA + 2,whoami_i);*/
	  Q = tfile_->num_extra_RA + 2;
	}
	if (Q < 2) {
	  //printf(" --^^^: Do not have RA variant with Q=%d, force to Q=%d\n",Q,2);
	  Q = 2;
	}
	/* TO BE REMOVED: 
	if (Q_wait< 5) Q = 25;
	else Q = 4; */
	/******************/
	
	Q_wait = Q;
	numbFrames = 0;
	#ifndef TEST_Q_XC_IN_GOP
	lb_R = 0.0;
	#endif
	/* The result is a Q-value, stored in class variable Q */
	}
	
	/* GA */
	double t_ga = Scheduler::instance().clock();
	printf("\nGA_Q===>t= %4.3f Q_= %d\n",t_ga,Q);
	
	
	/* figure out when to send the next one */
	//printf("Before next_interval()\n");
	nextPkttime_ = next_interval(size_); /* fetch both next interval and size of next frame */
	/* now, the nextPkttime_ contain how long time to start next frame transmission */
	/* schedule it */
	//printf("Before .resched()\n");
	timer_.resched(nextPkttime_);
	//printf("After .resched()\n");
  }
  
  
  double vbr_rateadapt2::next_interval(int& size)
  {
	//printf("next_interval\n");
	tfile_->get_next(ndx_, trec_,Q,is_mine);
	/* the right size[Q] value is always copied to [0] in get_next(): */
	size = trec_.trec_size[0]; 
	f_ = trec_.trec_type;
	max_=trec_.trec_max;
	source_=trec_.trec_source;
	return(((double)trec_.trec_time)/1000000.0); /* usecs->secs */
  }
  
  int vbr_rateadapt2::command(int argc, const char*const* argv)
  {
	Tcl& tcl = Tcl::instance();
	
	if (argc == 3) {
	  /* Here the input trace file is opened */
	  if (strcmp(argv[1], "attach-tracefile") == 0) {
		//printf("In vbr_rateadapt2::command() attach-tracefile\n");
		tfile_ = (vbrTraceFile *)TclObject::lookup(argv[2]);
		if (tfile_ == 0) {
		  tcl.resultf("no such node %s", argv[2]);
		  return(TCL_ERROR);
		}
		//printf("In EndOf vbr_rateadapt2::command() attach-tracefile\n");
		return(TCL_OK);
	  }
	}
	if (argc == 4) {
	  
	  return (TCL_OK);
	}
	if (argc == 5) {
	  if (strcmp(argv[1], "rateadapt") == 0) {
		// This version might receive ECFs from multiple nodes
		// rate_ is the original rate (maximum rate) in bytes/GoP (08.11.2005)
		int i;
		double min_rate;
		const char* ecf_c = argv[2];
		const char* from_c = argv[3];
		
		ecf_new = (double) atof(ecf_c);
		from_f = (int) atoi(from_c);
		//printf("Is in CBR_RA command rateadapt, from_f = %d\n",from_f);
		ecf_store[from_f] = ecf_new;
		
		if (ecf_store[from_f] < 1.0) {
		  ECF_rateadapt[from_f] *= ecf_store[from_f];
		  rate_store[from_f] = rate_ * ECF_rateadapt[from_f];
		}
		else if (ecf_store[from_f] > 1.0) {
		  /* first convert from bps to B/GoP */
		  #ifdef RCP_MODE
		  ecf_store[from_f] *= GoP_ / (8*fps_) ;
		  rate_store[from_f] = ecf_store[from_f];
		  if (rate_store[from_f] > rate_) {
			rate_store[from_f] = rate_;
		  }
		  #else
		  ecf_store[from_f] *= GoP_ / (8*fps_) ;
		  rate_store[from_f] = rate_store[from_f] + ecf_store[from_f];
		  ECF_rateadapt[from_f] = rate_store[from_f] / rate_;
		  if (rate_store[from_f] > rate_) {
			rate_store[from_f] = rate_;
			ECF_rateadapt[from_f] = 1.0;
		  }
		  #endif
		}
		min_rate = rate_;
		double old_rate = new_rate;
		for (i=0;i<MAX_NODE;i++) {
		  if (rate_store[i]<min_rate)
			min_rate = rate_store[i];
		}
		if (min_rate != new_rate) {
		  old_rate_coeff = numbFrames / GoP_;
		  new_rate_coeff = (GoP_ - numbFrames)/GoP_;
		  new_rate = min_rate;
		  //new_b = new_rate * 1.5;
		  new_b = new_rate * b_factor;
		  #ifdef TEST_Q_XC_IN_GOP
		  /* Q_wait = Q + 5;
		  if (Q_wait > 31)
		  Q_wait = 31; */
		  Q_flag = 1;
		  #endif
		}
		const char* whoami_c = argv[4];
		whoami_i = (int) atoi(whoami_c);
		//if ((whoami_i == 5 || whoami_i == 55) && from_f == 0) {
	  if ((whoami_i == MONITOR_NODE) && from_f == 0) {
		double t = Scheduler::instance().clock();
		printf(">>rate_adapt: t=%2.2f, ecf_new=%2.1f, from_f=%d, rates: %2.1f %2.1f %2.1f %2.1f\n",
			   t,ecf_new,from_f,rate_store[0],rate_store[2],rate_store[3],rate_store[5]); 
			   
			   /*
			   printf(">>SVBR_PAQM_RA, t=%6.5f, new_rate=%8.7f, old_rate=%8.7f\n",t,new_rate,old_rate); */
			   /*		printf(">>SVBR_RA, t=%6.5f, ecf=%8.7f, from_f=%d, new_rate = %5.1f,r_s[0]=%5.1f,r_s[1]=%5.1f\n",
			   t,ecf_new,from_f,new_rate,rate_store[0],rate_store[1]); */
			   //printf("......>>rate_store[2]=%4.3f,rate_store[3]=%4.3f\n",rate_store[2],rate_store[3]);
	  }
	  }
	  if (strcmp(argv[1], "TFRC_rateadapt") == 0) {
		const char* tfrc_rate_c = argv[2];	    
		double tfrc_rate = (double) atof(tfrc_rate_c);
		double old_rate;
		const char* whoami_c = argv[3];
		whoami_i = (int) atoi(whoami_c);
		const char* backlog_c = argv[4]; /* In addition to rate, the RA engine also need to know the TFRC tx
		queue backlog, so that the rate information is scaled down to
		enforce queue draining */
		int backlog = (int) atoi(backlog_c);
		//printf("tfrc_rate=%6.5f\n",tfrc_rate);
		/* Convert from B/s to B/GoP */
		tfrc_rate *= GoP_ / (1*fps_) ;
		old_rate = new_rate;
		// New 130306: to filter multiple feedbacks per GoP:
		old_rate_coeff = 0.0;
		new_rate_coeff = 1.0;
		/* Modify the TFRC given rate downwars in order to drain the Tx queue is any */
		float d_f = 1000.0; /* decay factor d_f as used in paper */
		tfrc_rate = tfrc_rate*exp(-(double)backlog/d_f);
		/* Filter multiple TFRC rate feedbacks within one GOP to the running average */
		double tmp = new_rate * update_count + tfrc_rate;
		update_count++;
		new_rate = tmp / update_count; /* Updated flat average */
		if (new_rate > rate_)
		  new_rate = rate_;
		new_b = new_rate * b_factor;
		//printf("whoami_i = %d\n",whoami_i);
		if (whoami_i == MONITOR_NODE && update_count == 2) {
		  sub_sample++;
		  double t = Scheduler::instance().clock();
		  if (sub_sample == 10) {
			printf(">>SVBR_TFRC_RA, t=%6.5f, new_rate=%6.5f, backlog=%d\n"
			,t,new_rate,backlog); 
			sub_sample = 0;
		  }
		}
		
	  }
	  return (TCL_OK);
	}
	//	return (Application::command(argc, argv));
	return (TrafficGenerator::command(argc, argv));
  }
  
