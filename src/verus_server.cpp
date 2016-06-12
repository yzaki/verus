#include "verus.hpp"

int s, s1, err;
int port;

double deltaDBar = 1.0;
double wMax = 0.0;
double dMax = 0.0;
double wBar = 1;
double dTBar = 0.0;
double dMaxLast =-10;
double dEst = 0.0;
int S = 0;
int ssId = 0;
double dMin = 1000.0;
double wList[MAX_W_DELAY_CURVE];

double delay;
int curveStop = MAX_W_DELAY_CURVE;
int maxWCurve = 0;
long long pktSeq = 0;
unsigned long long seqLast = 0;

double timeToRun;

bool slowStart = true;
bool exitSlowStart = false;
bool haveSpline = false;
bool terminate = false;
bool lossPhase = false;

char command[512];
char *name;

pthread_mutex_t lockSendingList;
pthread_mutex_t lockSPline;
pthread_mutex_t lockWList;
pthread_mutex_t restartLock;

pthread_t receiver_tid, delayProfile_tid, sending_tid;

struct sockaddr_in adr_clnt;
struct sockaddr_in adr_clnt2;

struct timeval startTime;
struct timeval lastAckTime;

socklen_t len_inet;
spline1dinterpolant spline;

std::atomic<long long> wCrt(0);
std::atomic<long long> tempS(0);
std::vector<double> delaysEpochList;
std::map <int, long long> sendingList;

// output files
std::ofstream receiverLog;
std::ofstream lossLog;
std::ofstream verusLog;

void segfault_sigaction(int signal, siginfo_t *si, void *arg)
{
    std::cout << "caught seg fault \n";

    verusLog.close();
    lossLog.close();
    receiverLog.close();

    exit(0);
}

static void displayError(const char *on_what) {
    fputs(strerror(errno),stderr);
    fputs(": ",stderr);
    fputs(on_what,stderr);
    fputc('\n',stderr);

    std::cout << "Error \n";

    exit(0);
}

void write2Log (std::ofstream &logFile, std::string arg1, std::string arg2, std::string arg3, std::string arg4, std::string arg5) {
  double relativeTime;
  struct timeval currentTime;

  gettimeofday(&currentTime,NULL);
    relativeTime = (currentTime.tv_sec-startTime.tv_sec)+(currentTime.tv_usec-startTime.tv_usec)/1000000.0;

    logFile << relativeTime << "," << arg1;

    if (arg2 != "")
      logFile << "," << arg2;
    if (arg3 != "")
      logFile << "," << arg3;
    if (arg4 != "")
      logFile << "," << arg4;
    if (arg5 != "")
      logFile << "," << arg5;

    logFile << "\n";

    return;
}

double ewma (double vals, double delay, double alpha) {
    double avg;

    // checking if the value is negative, meanning it has not been udpated
    if (vals < 0)
        avg = delay;
    else
        avg = vals * alpha + (1-alpha) * delay;

    return avg;
}

verus_packet *
udp_pdu_init(int seq, unsigned int packetSize, int w, int ssId) {
    verus_packet *pdu;

    struct timeval timestamp;
    
    // fixme
    if ((packetSize + sizeof(verus_header)) > MTU) {
        printf("cannot fit data in pdu");
        exit(0);
    }
    
    pdu = (verus_packet*)malloc(packetSize + sizeof(verus_header));

    if (pdu) {
        pdu->header.payloadlength = packetSize;
        pdu->header.seq = seq;
        pdu->header.w = w;
        pdu->header.ss_id = ssId;
        gettimeofday(&timestamp,NULL);
        pdu->header.seconds = timestamp.tv_sec;
        pdu->header.millis = timestamp.tv_usec;
    }

  return pdu;
}

void restartSlowStart(void) {

    pthread_mutex_lock(&restartLock);

    int i;

    maxWCurve = 0;
    dEst =0.0;
    seqLast = 0;
    wBar =1;
    dTBar = 0.0;
    wCrt = 0;
    dMin = 1000.0;
    pktSeq =0;
    dMax = 0.0;
    deltaDBar = 1.0;
    wMax = 0.0;
    dMaxLast = -10;
    slowStart = true;
    lossPhase = false;
    haveSpline = false;

    // stop the delay profile curve thread and restart it
    // pthread_cancel(delayProfile_tid);
    // if (pthread_create(&(delayProfile_tid), NULL, &delayProfile_thread, NULL) != 0)
    //         std::cout << "can't create thread: " <<  strerror(err) << "\n";
	
    // increase slow start session ID
    ssId ++;
    
    // cleaning up
    pthread_mutex_lock(&lockSendingList);
    sendingList.clear();
    pthread_mutex_unlock(&lockSendingList);

    delaysEpochList.clear();

    for (i=0; i<MAX_W_DELAY_CURVE; i++)
        wList[i]=-1;

    // sending the first packet for slow start
    tempS = 1;

    pthread_mutex_unlock(&restartLock);
}

double calcDelayCurve (double delay) {
    int w;

    for (w=2; w < MAX_W_DELAY_CURVE; w++) {
        if (!haveSpline) {
            pthread_mutex_lock(&lockWList);
            if (wList[w] > delay) {
                pthread_mutex_unlock(&lockWList);
                return (w-1);
            }
            pthread_mutex_unlock(&lockWList);
        }
        else {
            pthread_mutex_lock(&lockSPline);
            try {
                if (spline1dcalc(spline, w) > delay) {
                    pthread_mutex_unlock(&lockSPline);
                    return (w-1);
                }
            }
            catch (alglib::ap_error exc) {
                std::cout << "alglib1 " << exc.msg.c_str() << "\n";
                write2Log (lossLog, "CalcDelayCurve error", exc.msg.c_str(), "", "", "");
            }
            pthread_mutex_unlock(&lockSPline);
        }
    }

    if (!haveSpline)
        return 1.0; // special case: when verus starts working and we dont have a curve
                    // here we reached the max W and did not find a match, we return a w of 1
    else
        return -1000.0;
}

double calcDelayCurveInv (double w) {
    double ret;

    if (!haveSpline) {
        if (w < MAX_W_DELAY_CURVE) {
            pthread_mutex_lock(&lockWList);
            ret = wList[(int)w];
            pthread_mutex_unlock(&lockWList);
            return ret;
        }
        else
            return wList[MAX_W_DELAY_CURVE];
    }

    pthread_mutex_lock(&lockSPline);
    try{
        ret = spline1dcalc(spline, w);
    }
    catch (alglib::ap_error exc) {
        std::cout << "alglib2 " << exc.msg.c_str() << "\n";
        write2Log (lossLog, "Alglib error", exc.msg.c_str(), std::to_string(w), "", "");
    }
    pthread_mutex_unlock(&lockSPline);
    return ret;
}

int calcSi (double wBar) {
    int S;
    int n;

    n = (int) ceil(dTBar/(EPOCH/1000.0));

    if (n > 1)
        S = (int) fmax (0, (wBar+wCrt*(2-n)/(n-1)));
    else
        S = (int) fmax (0, (wBar - wCrt));

    return S;
}

void* sending_thread (void *arg)
{
    int i, ret, z;
    int sPkts;
    verus_packet *pdu;

    while (!terminate) {
        while (tempS > 0) {
            sPkts = tempS;
            tempS = 0;

            for (i=0; i<sPkts; i++) {
                pktSeq ++;

                // FIXME: change packetsize
                pdu = udp_pdu_init(pktSeq, MTU-sizeof(verus_header), wBar, ssId);
                ret = sendto(s1, pdu, MTU, MSG_DONTWAIT, (struct sockaddr *)&adr_clnt, len_inet);
                
                // z = 0;
                // while (z < MTU) {
                //     ret = sendto(s1, pdu+z, MTU-z, 0, (struct sockaddr *)&adr_clnt, len_inet);
                //     z+=ret;
                // }

                //std::cout << "sending " << pktSeq << " " << ret << "\n";

                if (ret < 0) {
                    std::cout << "OS Buffer \n";

                    if (slowStart) { // if the current delay exceeds half a second during slow start we should time out and exit slow start
                        std::cout << "Exit slow start: OS Buffer \n";
                        wBar = VERUS_M_DECREASE * pdu->header.w;
                        dEst = 0.75 * dMin * VERUS_R; // setting dEst to half of the allowed maximum delay, for effeciency purposes
                        lossPhase = true;
                        slowStart = false;
                        exitSlowStart = true;

                        write2Log (lossLog, "Exit slow start", "exceeding SS_EXIT_THRESHOLD", "", "", "");
                    }

                    // if UDP buffer of OS is full, we exit slow start and treat the current packet as lost
                    if (errno == ENOBUFS || errno == EAGAIN || errno == EWOULDBLOCK) {
                        // this packet was not sent we should decrease the packet seq number and free the pdu
                        pktSeq --;

                        free(pdu);
                        write2Log (lossLog, "OS buffer full", "reached maximum OS UDP buffer size", std::to_string(wCrt), "", "");
                        break; 
                    }
                    else
                        displayError("sendto(2)");
                }
                // storing sending packet info in sending list with sending time
                pthread_mutex_lock(&lockSendingList);
                sendingList[pdu->header.seq]=pdu->header.w;
                pthread_mutex_unlock(&lockSendingList);
                free (pdu);

                // sending one new packet -> increase packets in flight
                wCrt ++;
            }
            if (tempS > 0 && !slowStart)
                write2Log (lossLog, "Epoch Error", "couldn't send everything within the epoch. Have more to send", std::to_string(tempS.load()), std::to_string(slowStart), "");
        }
    }
    return NULL;
}

void* delayProfile_thread (void *arg)
{
    int i;
    std::vector<int> x;
    std::vector<double> y;

    int max_i;
    double N = 15.0;
    int cnt;
    double* xs = NULL;
    double* ys = NULL;
    real_1d_array xi;
    real_1d_array yi;
    ae_int_t info;
    spline1dfitreport rep;
    spline1dinterpolant splineTemp;

    while (!terminate) {
        if (slowStart){
            xs = NULL;
            ys = NULL;

            // still in slowstart phase we should not calculate the delay curve
            usleep (1);
        }
        else{
            // in normal mode we should compute the delay curve and sleep for the delay curve refresh timer
            // clearing the vectors
            x.clear();
            y.clear();

            max_i = 1;
            cnt = 0;
            maxWCurve = MAX_W_DELAY_CURVE;

            pthread_mutex_lock(&lockWList);
            for (i=1; i<maxWCurve; i++) {
                if (wList[i] >=0) {
                    if (i < curveStop) {
                        x.push_back(i);
                        y.push_back(wList[i]);

                        cnt++;
                        xs=(double*)realloc(xs,cnt*sizeof(double));
                        ys=(double*)realloc(ys,cnt*sizeof(double));

                        xs[cnt-1] = (double) i;
                        ys[cnt-1] = (double) wList[i];
                        max_i = i;
                    }
                    else{
                        wList[i] = -1;
                    }
                }
            }
            pthread_mutex_unlock(&lockWList);

            curveStop = MAX_W_DELAY_CURVE;
            maxWCurve = 0;

            xi.setcontent(cnt, xs);
            yi.setcontent(cnt, ys);

            while (max_i/N < 5) {
                N--;
                if (N < 1) {
                    write2Log (lossLog, "Restart", "Alglib M<4!", "", "", "");
                    restartSlowStart();
                    return NULL;
                }
            }
            if (max_i/N > 4) {
                try {
                    // if alglib takes long time to compute, we can use the previous spline in other threads
                    spline1dfitpenalized(xi, yi, max_i/N, 2.0, info, splineTemp, rep);

                    // copy newly calculated splinetemp to update spline
                    pthread_mutex_lock(&lockSPline);
                    spline=splineTemp;
                    pthread_mutex_unlock(&lockSPline);

                    haveSpline = true;
                }
                catch (alglib::ap_error exc) {
                    write2Log (lossLog, "Restart", "Spline exception", exc.msg.c_str(), "", "");
                    restartSlowStart();
                }
            }
            usleep (CURVE_TIMER);
        }
    }
    return NULL;
}

void updateUponReceivingPacket (double delay, int w) {

    if (wCrt > 0)
        wCrt --;

    // processing the delay and updating the verus parameters and the delay curve points
    delaysEpochList.push_back(delay);
    dTBar = ewma (dTBar, delay, 0.875);

    // updating the minimum delay
    if (delay < dMin)
        dMin = delay;

    // not to update the wList with any values that comes within the loss phase
    if (!lossPhase) {
        pthread_mutex_lock(&lockWList);
        wList[w] = ewma (wList[w], delay, 0.875);
        pthread_mutex_unlock(&lockWList);

        if (maxWCurve < w)
            maxWCurve = w;
    }
    else {    // still in loss phase, received an ACK, do similar to congestion avoidance
        wBar += 1.0/wBar;

        if(haveSpline)
            dEst = fmin (dEst, calcDelayCurveInv (wBar));
    }
    return;
}

void* receiver_thread (void *arg)
{
    unsigned int i;
    double timeouttimer=0.0;
    socklen_t len_inet;
    verus_packet *ack;
    char buf[sizeof(verus_header)];
    struct timeval receivedtime;

    len_inet = sizeof(struct sockaddr_in);

    sprintf (command, "%s/Losses.out", name);
    lossLog.open(command);
    sprintf (command, "%s/Receiver.out", name);
    receiverLog.open(command);

    ack = (verus_packet *) malloc(sizeof(verus_packet));

    int ret;

    while (!terminate) {
        ret = 0;
        while (ret < sizeof(verus_header)) {
            ret += recv(s1, &buf[ret], sizeof(verus_header) - ret, 0);
        }
        memcpy(ack, buf, sizeof(verus_header));
        pthread_mutex_lock(&restartLock);

        // we have started a new SS session, this packet belongs to the old SS session, so we discard it
        if (ack->header.ss_id < ssId) {
            pthread_mutex_unlock(&restartLock);
            continue;
        }

        gettimeofday(&receivedtime,NULL);
        delay = (receivedtime.tv_sec-ack->header.seconds)*1000.0+(receivedtime.tv_usec-ack->header.millis)/1000.0;

        if (delay > 10000) {
            std::cout <<"\n" <<ack->header.seconds*1000.0 << " " << ack->header.millis/1000.0 << " " << receivedtime.tv_sec*1000.0 << " " <<receivedtime.tv_usec/1000.0 << " " << ack->header.seq << "\n";
            std::cout << ret <<" "<< sizeof(verus_header) <<"\n";
        }

        if (slowStart && delay > SS_EXIT_THRESHOLD) { // if the current delay exceeds half a second during slow start we should time out and exit slow start
            wBar = VERUS_M_DECREASE * ack->header.w;
            dEst = 0.75 * dMin * VERUS_R; // setting dEst to half of the allowed maximum delay, for effeciency purposes
            lossPhase = true;
            slowStart = false;
            exitSlowStart = true;

            write2Log (lossLog, "Exit slow start", "exceeding SS_EXIT_THRESHOLD", "", "", "");
        }

        write2Log (receiverLog, std::to_string(ack->header.seq), std::to_string(delay), std::to_string(wCrt), std::to_string(wBar), "");

        // exiting the loss phase in case we are receiving new packets ack with w equal or smaller to the new w after the loss
        if (lossPhase && ack->header.w <= wBar) {
            delaysEpochList.clear();
            lossPhase = false;
            exitSlowStart = false;
        }

        updateUponReceivingPacket (delay, ack->header.w);

        // setting the last received sequence number to the current received one for next packet arrival processing
        // making sure we dont take out of order packet
        if (ack->header.seq >= seqLast+1 && ack->header.ss_id == ssId) {
        	seqLast = ack->header.seq;
        	gettimeofday(&lastAckTime,NULL);
        }

        // freeing that received pdu from the sendinglist map
       	pthread_mutex_lock(&lockSendingList);
        if (sendingList.find(ack->header.seq) != sendingList.end()) {
        	sendingList.erase(ack->header.seq);
        }
       	pthread_mutex_unlock(&lockSendingList);

        // ------------------ verus slow start ------------------
        if(slowStart) {
            // since we received an ACK we can increase the sending window by 1 packet according to slow start
            wBar ++;
            tempS += fmax(0, wBar-wCrt);   // let the sending thread start sending the new packets
        }
        pthread_mutex_unlock(&restartLock);
    }
    return NULL;
}

int main(int argc,char **argv) {

    int i=0;
    double bufferingDelay;
    double relativeTime=0;
    double wBarTemp;
    bool dMinStop=false;

    char dgram[512];             // Recv buffer

    struct stat info;
    struct sigaction sa;
    struct sockaddr_in adr_inet, client;
    struct timeval currentTime;

    // catching segmentation faults
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segfault_sigaction;
    sa.sa_flags   = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);

    for (int j=0; j<MAX_W_DELAY_CURVE; j++)
        wList[j]=-1;

    if (argc < 7) {
        std::cout << "syntax should be ./verus_server -name NAME -p PORT -t TIME (sec) \n";
        exit(0);
    }

    while (i != (argc-1)) {
        i=i+1;
        if (!strcmp (argv[i], "-name")) {
            i=i+1;
            name = argv[i];
            }
        else if (!strcmp (argv[i], "-p")) {
            i=i+1;
            port = atoi (argv[i]);
            }
        else if (!strcmp (argv[i], "-t")) {
            i=i+1;
            timeToRun = std::stod(argv[i]);
            }
        else {
            std::cout << "syntax should be ./verus_server -name NAME -p PORT -t TIME (sec) \n";
            exit(0);
        }
    }

    s = socket(AF_INET,SOCK_STREAM,0);
    
    if ( s == -1 )
    	displayError("socket error()");

    int sndbuf = 1000000;
    if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0)
        displayError("socket error() set buf");

    memset(&adr_inet,0,sizeof adr_inet);
    adr_inet.sin_family = AF_INET;
    adr_inet.sin_port = htons(port);
    adr_inet.sin_addr.s_addr = INADDR_ANY;

    if ( adr_inet.sin_addr.s_addr == INADDR_NONE )
        displayError("bad address.");

    len_inet = sizeof(struct sockaddr_in);

    bind (s, (struct sockaddr *)&adr_inet, sizeof(adr_inet));
    listen(s, 5);

    std::cout << "Server " << port << " waiting for request\n";
    socklen_t size = sizeof(struct sockaddr_in);
    s1 = accept(s, (struct sockaddr*) &client, &size);

    int flag = 1;
    int result = setsockopt(s1,IPPROTO_TCP,TCP_NODELAY,(char *) &flag, sizeof(int));
    if (result < 0)
        displayError("socket error() set TCP_NODELAY");


    if (stat (name, &info) != 0) {
        sprintf (command, "exec mkdir %s", name);
        system(command);
    }
    sprintf (command, "%s/Verus.out", name);
    verusLog.open(command);

    // getting the start time of the program, to make relative timestamps
    gettimeofday(&startTime,NULL);

    // create mutex
    pthread_mutex_init(&lockSendingList, NULL);
    pthread_mutex_init(&lockSPline, NULL);
    pthread_mutex_init(&lockWList, NULL);
    pthread_mutex_init(&restartLock, NULL);

    // starting the threads
    if (pthread_create(&(receiver_tid), NULL, &receiver_thread, NULL) != 0)
        std::cout << "Can't create thread: " << strerror(err);
    if (pthread_create(&(sending_tid), NULL, &sending_thread, NULL) != 0)
        std::cout << "Can't create thread: " << strerror(err);
    if (pthread_create(&(delayProfile_tid), NULL, &delayProfile_thread, NULL) != 0)
        std::cout << "can't create thread: " <<  strerror(err) << "\n";

    // sending the first for slow start
    tempS = 1;

    std::cout << "Client " << port << " is connected\n";

    while (relativeTime <= timeToRun) {

        gettimeofday(&currentTime,NULL);
        relativeTime = (currentTime.tv_sec-startTime.tv_sec)+(currentTime.tv_usec-startTime.tv_usec)/1000000.0;

        // waiting for slow start to finish to start sending data
        if (!slowStart) {
            dMinStop=false;

            // -------------- Verus sender ------------------
            if (delaysEpochList.size() > 0) {
                dMax = *std::max_element(delaysEpochList.begin(),delaysEpochList.end());
                delaysEpochList.clear();
            }
            else {
                bufferingDelay = (currentTime.tv_sec-lastAckTime.tv_sec)*1000.0+(currentTime.tv_usec-lastAckTime.tv_usec)/1000.0;
                dMax = fmax (dMaxLast, bufferingDelay);
            }

            // only first verus epoch, dMaxLast is intialized to -10
            if (dMaxLast == -10)
                dMaxLast = dMax;

            deltaDBar = dMax - dMaxLast;

            // normal verus protocol
            if (dMaxLast/dMin > VERUS_R) {
                if (!exitSlowStart) {
                    dEst = fmax (dMin, (dEst-DELTA2));

                    if (dEst == dMin && wCrt < 2) {
                        dMin += 10;
                    }
                    else if (dEst == dMin)
                        dMinStop=true;
                }
            }
            else if (deltaDBar > 0.0001)
                dEst = fmax (dMin, (dEst-DELTA1));
            else
                dEst += DELTA2;

            wBarTemp = calcDelayCurve (dEst);

            if (wBarTemp < 0) {
                write2Log (lossLog, "Restart", "can't map delay on delay curve", "", "", "");
                restartSlowStart();
                continue;
            }

            wBar = fmax (1.0, wBarTemp);
            S = calcSi (wBar);

            dMaxLast = ewma(dMaxLast, dMax, 0.2);

            if (!dMinStop)
                tempS += S;

            write2Log (verusLog, std::to_string(dEst), std::to_string(dMin), std::to_string(wCrt), std::to_string(wBar), std::to_string(tempS));
        }
        usleep (EPOCH);
    }

    verusLog.close();
    lossLog.close();
    receiverLog.close();
    ssId = -1;
    tempS = 1;

    usleep (1000000);
    terminate = true;
    usleep (1000000);

    std::cout << "Server " << port << " is exiting\n";
    close(s);

    return 0;
}
