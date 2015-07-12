#include "verus.hpp"

int len_inet;                // length
int s,err;                       // Socket

bool receivedPkt = false;

char* srvr_addr;
struct sockaddr_in adr_srvr; // AF_INET
struct sockaddr_in adr;      // AF_INET

pthread_t timeout_tid;
boost::asio::io_service io;
boost::asio::deadline_timer timer (io, boost::posix_time::milliseconds(SS_INIT_TIMEOUT));

static void displayError(const char *on_what) {
  fputs(strerror(errno),stderr);
  fputs(": ",stderr);
  fputs(on_what,stderr);
  fputc('\n',stderr);
  exit(1);
}

void TimeoutHandler( const boost::system::error_code& e) {
    int z;

    if (e) return;

    z = sendto(s,"Hallo", strlen("Hallo"), 0, (struct sockaddr *)&adr_srvr, len_inet);
    if ( z < 0 )
      displayError("sendto(2)");

    //update timer and restart
    timer.expires_from_now (boost::posix_time::milliseconds(1000));
    timer.async_wait(&TimeoutHandler);

    return;
}

void* timeout_thread (void *arg)
{
  boost::asio::io_service::work work(io);

  timer.expires_from_now (boost::posix_time::milliseconds(SS_INIT_TIMEOUT));
  timer.async_wait(&TimeoutHandler);
  io.run();

  return NULL;
}

int main(int argc,char **argv) {
  int z;
  int i = 0;
  char* port;
  udp_packet_t *pdu;
  struct timeval timestamp;
  setbuf(stdout, NULL);

  if (argc < 5) {
    std::cout << "syntax should be ./verus_client -ip IP -p PORT \n";
    exit(0);
  }

  while (i != (argc-1)) { // Check that we haven't finished parsing already
    i=i+1;
    if (!strcmp (argv[i], "-ip")) {
      i=i+1;
      srvr_addr = argv[i];
    } else if (!strcmp (argv[i], "-p")) {
      i=i+1;
      port = argv[i];
    } else {
      std::cout << "syntax should be ./verus_client -ip IP -p PORT \n";
      exit(0);
    }
  }

  memset(&adr_srvr,0,sizeof adr_srvr);

  adr_srvr.sin_family = AF_INET;
  adr_srvr.sin_port = htons(atoi(port));
  adr_srvr.sin_addr.s_addr =  inet_addr(srvr_addr);

  if ( adr_srvr.sin_addr.s_addr == INADDR_NONE ) {
    displayError("bad address.");
  }

  len_inet = sizeof adr_srvr;

  s = socket(AF_INET,SOCK_DGRAM,0);
  if ( s == -1 ) {
    displayError("socket()");
  }

  //printf("Sending Hallo to %s:%s\n", srvr_addr, port);
  z = sendto(s,"Hallo", strlen("Hallo"), 0, (struct sockaddr *)&adr_srvr, len_inet);
  if ( z < 0 )
    displayError("sendto(2)");

  if (pthread_create(&(timeout_tid), NULL, &timeout_thread, NULL) != 0)
      std::cout << "can't create thread: " <<  strerror(err) << "\n";

  pdu = (udp_packet_t *) malloc(sizeof(udp_packet_t));

  // starting to loop waiting to receive data and to ACK
  while(1) {

    socklen_t len = sizeof(struct sockaddr_in);
    z = recvfrom(s, pdu, sizeof(udp_packet_t), 0, (struct sockaddr *)&adr, &len);
    if ( z < 0 )
      displayError("recvfrom(2)");

    // stopping the io timer for the timeout
    if (!receivedPkt) {
      receivedPkt = true;
      io.stop();
    }

    gettimeofday(&timestamp,NULL);
    printf("%ld.%06d, %llu\n", timestamp.tv_sec, timestamp.tv_usec, pdu->seq);

    // sending ACK
    z = sendto(s, pdu, sizeof(udp_packet_t), 0, (struct sockaddr *)&adr_srvr, len_inet);
    if ( z < 0 )
      displayError("sendto(2)");
  }

  close(s);
  return 0;
}
