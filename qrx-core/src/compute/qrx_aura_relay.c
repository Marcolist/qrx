#include "compute/qrx_aura_relay.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define qrx_close closesocket
#define qrx_shutdown(s) shutdown((s),SD_BOTH)
typedef CRITICAL_SECTION qrx_mutex_t;
static void m_init(qrx_mutex_t*m){InitializeCriticalSection(m);}static void m_lock(qrx_mutex_t*m){EnterCriticalSection(m);}static void m_unlock(qrx_mutex_t*m){LeaveCriticalSection(m);}static void m_free(qrx_mutex_t*m){DeleteCriticalSection(m);}
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#define qrx_close close
#define qrx_shutdown(s) shutdown((s),SHUT_RDWR)
typedef pthread_mutex_t qrx_mutex_t;
static void m_init(qrx_mutex_t*m){pthread_mutex_init(m,NULL);}static void m_lock(qrx_mutex_t*m){pthread_mutex_lock(m);}static void m_unlock(qrx_mutex_t*m){pthread_mutex_unlock(m);}static void m_free(qrx_mutex_t*m){pthread_mutex_destroy(m);}
#endif
#define RELAY_MAGIC "QRXRLY41"
#define ROLE_PROVIDER 1u
#define ROLE_CLIENT 2u
typedef struct{char route[QRX_AURA_RELAY_ROUTE_MAX];int fd;}Route;
struct QrxAuraRelayServer{int listen_fd;uint16_t port;volatile int stop;Route routes[QRX_AURA_RELAY_MAX_ROUTES];uint32_t route_count;qrx_mutex_t lock;
#ifdef _WIN32
HANDLE thread;
#else
pthread_t thread;
#endif
};
static int net_init(void){
#ifdef _WIN32
static LONG once=0;if(InterlockedCompareExchange(&once,1,0)==0){WSADATA w;if(WSAStartup(MAKEWORD(2,2),&w)){InterlockedExchange(&once,0);return-1;}}
#endif
return 0;}
static int send_all(int fd,const void*p,size_t n){const char*b=(const char*)p;while(n){int z=(int)send(fd,b,(int)(n>0x7fffffff?0x7fffffff:n),0);if(z<=0)return-1;b+=z;n-=(size_t)z;}return 0;}
static int recv_all(int fd,void*p,size_t n){char*b=(char*)p;while(n){int z=(int)recv(fd,b,(int)(n>0x7fffffff?0x7fffffff:n),0);if(z<=0)return-1;b+=z;n-=(size_t)z;}return 0;}
static int tcp_connect(const char*h,uint16_t port){if(net_init())return-1;char ps[16];snprintf(ps,sizeof(ps),"%u",(unsigned)port);struct addrinfo hints,*res=NULL,*it;memset(&hints,0,sizeof(hints));hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;if(getaddrinfo(h,ps,&hints,&res))return-1;int fd=-1;for(it=res;it;it=it->ai_next){fd=(int)socket(it->ai_family,it->ai_socktype,it->ai_protocol);if(fd<0)continue;if(connect(fd,it->ai_addr,(socklen_t)it->ai_addrlen)==0)break;qrx_close(fd);fd=-1;}freeaddrinfo(res);return fd;}
int qrx_aura_relay_endpoint_parse(const char*e,char*h,size_t hc,uint16_t*port,char*r,size_t rc){if(!e||!h||!port||!r||strncmp(e,"qrxrelay://",11))return-1;const char*s=e+11,*slash=strchr(s,'/');if(!slash||!slash[1])return-1;char hp[256];size_t n=(size_t)(slash-s);if(!n||n>=sizeof(hp)||strlen(slash+1)>=rc)return-1;memcpy(hp,s,n);hp[n]=0;const char*c=strrchr(hp,':');if(!c||c==hp||!c[1])return-1;char*end=NULL;long p=strtol(c+1,&end,10);if(!end||*end||p<1||p>65535)return-1;size_t hn=(size_t)(c-hp);if(hn>=hc)return-1;memcpy(h,hp,hn);h[hn]=0;snprintf(r,rc,"%s",slash+1);*port=(uint16_t)p;return 0;}
static int handshake(int fd,uint8_t role,const char*route){size_t n=strlen(route);if(n<1||n>=QRX_AURA_RELAY_ROUTE_MAX)return-1;uint8_t h[11];memcpy(h,RELAY_MAGIC,8);h[8]=role;h[9]=(uint8_t)(n>>8);h[10]=(uint8_t)n;return send_all(fd,h,sizeof(h))||send_all(fd,route,n)?-1:0;}
static int recv_handshake(int fd,uint8_t*role,char route[QRX_AURA_RELAY_ROUTE_MAX]){uint8_t h[11];if(recv_all(fd,h,sizeof(h))||memcmp(h,RELAY_MAGIC,8))return-1;size_t n=((size_t)h[9]<<8)|h[10];if(n<1||n>=QRX_AURA_RELAY_ROUTE_MAX)return-1;*role=h[8];if(recv_all(fd,route,n))return-1;route[n]=0;return 0;}
static Route*find_route(QrxAuraRelayServer*s,const char*r){for(uint32_t i=0;i<s->route_count;i++)if(!strcmp(s->routes[i].route,r))return&s->routes[i];return NULL;}
static int forward_packet(int from,int to){uint8_t h[4];if(recv_all(from,h,4))return-1;uint32_t n=((uint32_t)h[0]<<24)|((uint32_t)h[1]<<16)|((uint32_t)h[2]<<8)|h[3];if(n<16||n>64u*1024u*1024u)return-1;uint8_t*b=malloc(n);if(!b)return-1;int rc=recv_all(from,b,n)||send_all(to,h,4)||send_all(to,b,n)?-1:0;free(b);return rc;}
static void route_drop(QrxAuraRelayServer*s,Route*rt){if(!rt)return;if(rt->fd>=0){qrx_shutdown(rt->fd);qrx_close(rt->fd);}uint32_t idx=(uint32_t)(rt-s->routes);if(idx<s->route_count){for(uint32_t i=idx+1;i<s->route_count;i++)s->routes[i-1]=s->routes[i];s->route_count--;}}
static void handle_conn(QrxAuraRelayServer*s,int fd){uint8_t role=0;char route[QRX_AURA_RELAY_ROUTE_MAX];if(recv_handshake(fd,&role,route)){qrx_close(fd);return;}m_lock(&s->lock);if(role==ROLE_PROVIDER){Route*rt=find_route(s,route);if(rt)route_drop(s,rt);if(s->route_count<QRX_AURA_RELAY_MAX_ROUTES){rt=&s->routes[s->route_count++];memset(rt,0,sizeof(*rt));snprintf(rt->route,sizeof(rt->route),"%s",route);rt->fd=fd;fd=-1;}m_unlock(&s->lock);if(fd>=0)qrx_close(fd);return;}if(role!=ROLE_CLIENT){m_unlock(&s->lock);qrx_close(fd);return;}Route*rt=find_route(s,route);if(!rt||rt->fd<0){m_unlock(&s->lock);qrx_close(fd);return;}int pfd=rt->fd;/* Serialize a route: provider service processes one framed request at a time. */int ok=forward_packet(fd,pfd)==0&&forward_packet(pfd,fd)==0;if(!ok){rt=find_route(s,route);if(rt&&rt->fd==pfd)route_drop(s,rt);}m_unlock(&s->lock);qrx_shutdown(fd);qrx_close(fd);}
static int open_listener(const char*h,uint16_t port,uint16_t*out){if(net_init())return-1;int fd=(int)socket(AF_INET,SOCK_STREAM,0);if(fd<0)return-1;int one=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(const char*)&one,sizeof(one));struct sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons(port);if(!h||!h[0]||!strcmp(h,"0.0.0.0"))a.sin_addr.s_addr=htonl(INADDR_ANY);else if(inet_pton(AF_INET,h,&a.sin_addr)!=1){qrx_close(fd);return-1;}if(bind(fd,(struct sockaddr*)&a,sizeof(a))||listen(fd,64)){qrx_close(fd);return-1;}socklen_t z=sizeof(a);if(getsockname(fd,(struct sockaddr*)&a,&z)){qrx_close(fd);return-1;}*out=ntohs(a.sin_port);return fd;}
static void loop(QrxAuraRelayServer*s){while(!s->stop){fd_set rf;FD_ZERO(&rf);FD_SET(s->listen_fd,&rf);struct timeval tv={0,200000};int z=select(s->listen_fd+1,&rf,NULL,NULL,&tv);if(z<=0)continue;int fd=(int)accept(s->listen_fd,NULL,NULL);if(fd>=0)handle_conn(s,fd);}}
#ifdef _WIN32
static DWORD WINAPI relay_thread(LPVOID p){loop((QrxAuraRelayServer*)p);return 0;}
#else
static void*relay_thread(void*p){loop((QrxAuraRelayServer*)p);return NULL;}
#endif
int qrx_aura_relay_server_start(const char*h,uint16_t port,QrxAuraRelayServer**out){if(!out)return-1;*out=NULL;QrxAuraRelayServer*s=calloc(1,sizeof(*s));if(!s)return-1;for(uint32_t i=0;i<QRX_AURA_RELAY_MAX_ROUTES;i++)s->routes[i].fd=-1;m_init(&s->lock);s->listen_fd=open_listener(h,port,&s->port);if(s->listen_fd<0){m_free(&s->lock);free(s);return-2;}
#ifdef _WIN32
s->thread=CreateThread(NULL,0,relay_thread,s,0,NULL);if(!s->thread){qrx_close(s->listen_fd);m_free(&s->lock);free(s);return-3;}
#else
if(pthread_create(&s->thread,NULL,relay_thread,s)){qrx_close(s->listen_fd);m_free(&s->lock);free(s);return-3;}
#endif
*out=s;return 0;}
int qrx_aura_relay_server_stop(QrxAuraRelayServer*s){if(!s)return-1;if(s->stop)return 0;s->stop=1;qrx_shutdown(s->listen_fd);qrx_close(s->listen_fd);
#ifdef _WIN32
WaitForSingleObject(s->thread,5000);CloseHandle(s->thread);s->thread=NULL;
#else
pthread_join(s->thread,NULL);
#endif
m_lock(&s->lock);while(s->route_count)route_drop(s,&s->routes[s->route_count-1]);m_unlock(&s->lock);return 0;}
void qrx_aura_relay_server_free(QrxAuraRelayServer*s){if(!s)return;if(!s->stop)qrx_aura_relay_server_stop(s);m_free(&s->lock);free(s);}uint16_t qrx_aura_relay_server_port(const QrxAuraRelayServer*s){return s?s->port:0;}
static int role_connect(const char*e,uint8_t role){char h[256],r[QRX_AURA_RELAY_ROUTE_MAX];uint16_t p;if(qrx_aura_relay_endpoint_parse(e,h,sizeof(h),&p,r,sizeof(r)))return-1;int fd=tcp_connect(h,p);if(fd<0)return-1;if(handshake(fd,role,r)){qrx_close(fd);return-1;}return fd;}
int qrx_aura_relay_client_connect(const char*e){return role_connect(e,ROLE_CLIENT);}int qrx_aura_relay_provider_connect(const char*e){return role_connect(e,ROLE_PROVIDER);}
