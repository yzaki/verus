#include "verus.hpp"

int len_inet;                // length
int s,err;

char* port;
char* srvr_addr;
unsigned int delay = 0;

bool receivedPkt = false;
bool terminate = false;

struct sockaddr_in adr_srvr;
struct sockaddr_in adr_srvr2;
struct sockaddr_in adr;

pthread_t timeout_tid;
pthread_t sending_tid;

typedef struct {
  verus_packet *pdu;
  long long seconds;
  long long millis;
} sendPkt;

std::vector<sendPkt*> sendingList;
pthread_mutex_t lockSendingList;

static void displayError(const char *on_what) {
  fputs(strerror(errno),stderr);
  fputs(": ",stderr);
  fputs(on_what,stderr);
  fputc('\n',stderr);
  exit(1);
}

void* sending_thread (void *arg)
{
  int s1, z;
  struct timeval timestamp;
  sendPkt *pkt;

  memset(&adr_srvr,0,sizeof adr_srvr);

  while (!terminate) {
    gettimeofday(&timestamp,NULL);

    pthread_mutex_lock(&lockSendingList);

    if (sendingList.size() > 0)
       pkt = * sendingList.begin();

    // since tc qdisc command in Linux seems to have some issues when adding delay, we defer the packet here
    if (sendingList.size() > 0 && (timestamp.tv_sec-pkt->seconds)*1000.0+(timestamp.tv_usec-pkt->millis)/1000.0 > delay) {
      // sending ACK
      z = send(s, &pkt->pdu->header, sizeof(verus_header), 0);

      free (pkt->pdu);

      if (z < 0)
        if (errno == ENOBUFS || errno == EAGAIN || errno == EWOULDBLOCK)
          std::cout << "reached maximum OS UDP buffer size\n";
        else
          displayError("sendto(2)");

      sendingList.erase(sendingList.begin());
      pthread_mutex_unlock(&lockSendingList);
    }
    else{
      pthread_mutex_unlock(&lockSendingList);
      usleep(0.01);
    }
  }

  return NULL;
}

int main(int argc,char **argv) {
  int z;
  int i = 1;
  char command[512];
  char tmp[512];
  char buf[MTU];

  verus_packet *pdu;
  verus_header header;

  sendPkt *pkt;
  struct timeval timestamp;
  setbuf(stdout, NULL);
  std::ofstream clientLog;

  pthread_mutex_init(&lockSendingList, NULL);

  if (argc < 4) {
    std::cout << "syntax should be ./verus_client <server address> -p <server port> [-d <additional link delay in ms>] \n";
    exit(0);
  }

  srvr_addr = argv[1];

  while (i != (argc-1)) { // Check that we haven't finished parsing already
    i=i+1;
    if (!strcmp (argv[i], "-p")) {
      i=i+1;
      port = argv[i];
    } else if (!strcmp (argv[i], "-d")) {
      i=i+1;
      delay = atoi(argv[i]);
    }else {
      std::cout << "syntax should be ./verus_client <server address> -p <server port> [-d <additional link delay in ms>] \n";
      exit(0);
    }
  }


  s = socket(AF_INET,SOCK_STREAM,0);
  if ( s == -1 ) {
    displayError("socket()");
  }

  int reuse = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
	perror("setsockopt(SO_REUSEADDR) failed");


  memset(&adr_srvr,0,sizeof (adr_srvr));
  adr_srvr.sin_family = AF_INET;
  adr_srvr.sin_port = htons(atoi(port));
  adr_srvr.sin_addr.s_addr =  inet_addr(srvr_addr);

  if ( adr_srvr.sin_addr.s_addr == INADDR_NONE ) {
    displayError("bad address.");
  }

  std::cout << "Sending request to server \n";

  z = connect (s, (struct sockaddr *)&adr_srvr, sizeof(adr_srvr));
  if ( z < 0 )
    perror("connect");

  if (pthread_create(&(sending_tid), NULL, &sending_thread, NULL) != 0)
      std::cout << "can't create thread: " <<  strerror(err) << "\n";

  // starting to loop waiting to receive data and to ACK
  while(!terminate) {
    pdu = (verus_packet *) malloc(sizeof(verus_packet));
	socklen_t len = sizeof(struct sockaddr_in);
    
    z = 0;
    while (z < sizeof(verus_header)) {
    	z = recvfrom(s, &buf[z], sizeof(verus_header)-z, 0, (struct sockaddr *)&adr, &len);
    }
    memcpy (&header, buf, sizeof(verus_header));

    //std::cout << "received bytes "<< z << " " << header.payloadlength << " " << header.seq << " \n";

    z=0;
    while (z < header.payloadlength) {
    	z += recvfrom (s, &buf[z+sizeof(verus_header)], header.payloadlength-z, 0, (struct sockaddr *)&adr, &len);
    }
    memcpy (pdu, buf, sizeof (verus_packet));

    //std::cout << "received bytes "<< z << " " << pdu->header.ss_id << " " << pdu->header.seq << " \n";
    if ( z < 0 )
      displayError("recvfrom(2)");


    if (pdu->header.ss_id < 0) {
      clientLog.close();
      terminate = true;
    }

    // stopping the io timer for the timeout
    if (!receivedPkt) {
      sprintf (command, "client_%s.out", port);
      clientLog.open(command);
      receivedPkt = true;
      std::cout << "Connected to server \n";
    }

    gettimeofday(&timestamp,NULL);
    //sprintf(tmp, "%ld.%06d, %llu\n", timestamp.tv_sec, timestamp.tv_usec, pdu->header.seq);
    std::cout << tmp;
    clientLog << tmp;

    pkt = (sendPkt *) malloc(sizeof(sendPkt));
    pkt->pdu = pdu;
    pkt->seconds = timestamp.tv_sec;
    pkt->millis = timestamp.tv_usec;

    pthread_mutex_lock(&lockSendingList);
    sendingList.push_back(pkt);
    pthread_mutex_unlock(&lockSendingList);

  }

  std::cout << "Client exiting \n";
  close(s);
  return 0;
}
