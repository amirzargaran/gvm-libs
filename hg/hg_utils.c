/* HostsGatherer
 * Copyright (C) 1999 Renaud Deraison
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <arpa/inet.h> /* for inet_addr */
#include <ctype.h> /* isalnum */
#include <netdb.h> /* for addrinfo */
#include <stdio.h> /* for stderr */
#include <stdlib.h> /* for free */
#include <string.h> /* for strncpy */

#include "hosts_gatherer.h"

/**
 * @brief Resolve an hostname.
 */
int
hg_resolv (char* hostname, struct in6_addr *in6addr, int family)
{
  struct addrinfo hints;
  struct addrinfo *ai;
  int    retval;

  *in6addr = in6addr_any;
  /* First check whether it is a numeric host. */
  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_INET6;
  hints.ai_flags = AI_V4MAPPED | AI_ALL | AI_NUMERICHOST;

  retval = getaddrinfo (hostname, NULL, &hints, &ai);
  if (!retval)
    {
      /** @TODO getaddrinfo: host might resolve to more than one ip.
       *        In this case (ai->ai_next != NULL).
       *        This should somehow be respected, probably needs changes to
       *        interface.
       */
      if (ai->ai_family == AF_INET)
        {
          in6addr->s6_addr32[0] = 0;
          in6addr->s6_addr32[1] = 0;
          in6addr->s6_addr32[2] = htonl(0xffff);
          memcpy (&in6addr->s6_addr32[3], &((struct sockaddr_in *)ai->ai_addr)->sin_addr, sizeof (struct in6_addr));
        }
      else
        {
          memcpy (in6addr, &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr, sizeof (struct in6_addr));
        }

      freeaddrinfo (ai);
      return 0;
    }

  /* First check whether it is an ipv4 host. */
  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_INET;

  retval = getaddrinfo (hostname, NULL, &hints, &ai);
  if (!retval)
    {
      in6addr->s6_addr32[0] = 0;
      in6addr->s6_addr32[1] = 0;
      in6addr->s6_addr32[2] = htonl(0xffff);
      memcpy(&in6addr->s6_addr32[3], &((struct sockaddr_in *)ai->ai_addr)->sin_addr, sizeof(struct in_addr));
      freeaddrinfo (ai);
      return 0;
    }

  /* Check whether it is an ipv6 host. */
  if (family != AF_INET6)
    return -1;   /* returning in6addr_any */

  memset (&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;

  retval = getaddrinfo (hostname, NULL, &hints, &ai);
  if (!retval)
    {
      memcpy (in6addr, &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr, sizeof (struct in6_addr));
      freeaddrinfo (ai);
      return 0;
    }
  return -1;  /* return in6addr_any*/
}


/**
 * @param[out] hostname Pointer to buffer that will contain hostname if
 *                      successful.
 * @param[in]  sz       Size of hostname buffer \ref hostname.
 *
 * @return Always returns 0.
 */
int
hg_get_name_from_ip (struct in6_addr *ip, char * hostname, int sz)
{
  int i;
  struct sockaddr_in saddr;
  struct sockaddr_in6 s6addr;
  struct sockaddr *sa;
  int    len;

  if (IN6_IS_ADDR_V4MAPPED (ip))
    {
      saddr.sin_family = AF_INET;
      len = sizeof (struct sockaddr_in);
      saddr.sin_addr.s_addr =  ip->s6_addr32[3];
      sa = (struct sockaddr *) & saddr;
    }
  else
    {
      s6addr.sin6_family = AF_INET6;
      len = sizeof (struct sockaddr_in6);
      memcpy (&s6addr.sin6_addr, ip, sizeof (struct in6_addr));
      sa = (struct sockaddr *) & s6addr;
    }
  if (getnameinfo (sa, len, hostname, sz, NULL, 0, 0))
    {
      if (IN6_IS_ADDR_V4MAPPED (ip))
        fprintf (stderr, "just copying address %s", inet_ntop (AF_INET, &saddr.sin_addr, hostname, sz));
      else
        fprintf (stderr, "just copying address %s", inet_ntop (AF_INET6, ip, hostname, sz));
    }
  else
    {
      fprintf (stderr, "resolved to name %s\n", hostname);
    }

  hostname[sz - 1] = '\0';
  for (i = 0 ; hostname [i] != '\0' ; i++)
    {
      if (!isalnum(hostname[i]) &&
          hostname[i] != '.' &&
          hostname[i] != '_' &&
          hostname[i] != '-' &&
          hostname[i] != ':' &&
          hostname[i] != '%' ) hostname[i] = '?';
      if (hostname[i] == '%')
        hostname[i] = '\0';    /* To handle % in the hostname returned from getnameinfo*/
    }
  return 0; /* We never fail */
}


/**
 * @return 0 if adress info for \ref hostname could be found, 1 otherwise.
 */
int
hg_valid_ip_addr (char *hostname)
{
  struct addrinfo hints;
  struct addrinfo *ai;
  int    retval;

  memset (&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_flags = AI_V4MAPPED | AI_NUMERICHOST;

  retval = getaddrinfo (hostname, NULL, &hints, &ai);
  if (retval)
    return 1;
  else
    {
      freeaddrinfo (ai);
      return 0;
    }
}


/**
 * input : hostname (ie : www.if.arf.com)
 * returns: if.arf.com
 *
 * If the input is arf.com
 * returns : NULL
 */
char *
hg_name_to_domain (char * hostname)
{
  unsigned int i = -1, j;
  char * ret;
  int len;

  if(inet_addr(hostname)!=INADDR_NONE)return(NULL);
  while(hostname[++i]!='.' && i<strlen(hostname));
  if(hostname[i]!='.')return(NULL);
  j=i;
  while(hostname[++j]!='.' && j<strlen(hostname));
  if(hostname[j]!='.')return(NULL);
  len = strlen(hostname+i+1);
  ret = malloc(len+1);
  strncpy(ret, hostname+i+1, len+1);
  return(ret);
}


void
hg_host_cleanup (struct hg_host * hosts)
{
  if (hosts->hostname != NULL)
    free (hosts->hostname);
  if (hosts->domain != NULL)
    free (hosts->domain);
  free (hosts);
}


/**
 * @brief Frees all hosts that are linked in hosts, using hg_host_cleanup.
 *
 * @param hosts host list to free.
 */
void
hg_hosts_cleanup (struct hg_host * hosts)
{
  while ( hosts != NULL ) 
  {
   struct hg_host * next;
   next = hosts->next;
   hg_host_cleanup(hosts);
   hosts = next;
 }
}
