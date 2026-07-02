#ifndef __XM_SDL_NET_H__
#define __XM_SDL_NET_H__

#include "common/BuildConfig.h"

#if ENABLE_NETWORK
#  include <SDL2/SDL_net.h>
#else
/* Stub types so the net/ headers compile when networking is disabled.
   The net_web_stubs.cpp provides empty singleton implementations;
   these types are never actually instantiated on the web build.        */
typedef struct { unsigned short port; unsigned int host; } IPaddress;
typedef struct _UDPpacket {
  int channel;
  unsigned char *data;
  int len, maxlen, status;
  IPaddress address;
} UDPpacket;
struct _UDPsocket;
struct _TCPsocket;
struct _SDLNet_SocketSet;
typedef struct _UDPsocket        *UDPsocket;
typedef struct _TCPsocket        *TCPsocket;
typedef struct _SDLNet_SocketSet *SDLNet_SocketSet;

/* Stub functions used only in GameInit.cpp initNetwork/uninitNetwork */
static inline int         SDLNet_Init(void)        { return 0; }
static inline void        SDLNet_Quit(void)         {}
static inline const char *SDLNet_GetError(void)     { return "network disabled"; }
#endif /* ENABLE_NETWORK */

#endif /* __XM_SDL_NET_H__ */
